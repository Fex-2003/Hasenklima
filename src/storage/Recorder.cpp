#include "Recorder.h"

#include <LittleFS.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

namespace klima {
namespace {
constexpr const char* kTestPaths[] = {"/littlefs/test-a.dat", "/littlefs/test-b.dat"};
constexpr char kSelectorPath[] = "/littlefs/test.idx";
constexpr char kSelectorTemp[] = "/littlefs/test.new";
constexpr uint32_t kSelectorMagic = 0x5844494b; // KIDX
constexpr size_t kFilesystemReserveBytes = 3 * 4096;
constexpr size_t kMarkerReserve = 8;

bool writeExactly(int file, const uint8_t* data, size_t size) {
    return ::write(file, data, size) == static_cast<ssize_t>(size);
}

const char* optionalTime(uint32_t value, char (&buffer)[16]) {
    if (value == kNoTestTime) return "unbekannt";
    std::snprintf(buffer, sizeof(buffer), "%lu", static_cast<unsigned long>(value));
    return buffer;
}

const char* optionalFloat(float value, char (&buffer)[24]) {
    if (!std::isfinite(value)) return "UNSET";
    std::snprintf(buffer, sizeof(buffer), "%.9g", value);
    return buffer;
}
} // namespace

bool Recorder::begin() {
    if (!LittleFS.begin(false, "/littlefs", 2)) {
        portENTER_CRITICAL(&mutex_);
        state_.ioError = true;
        portEXIT_CRITICAL(&mutex_);
        return false;
    }
    TestMetadata meta;
    uint32_t count = 0;
    const bool recovered = recover(meta, count);
    const bool broken = !recovered && LittleFS.exists("/test.idx");
    portENTER_CRITICAL(&mutex_);
    state_.available = true;
    state_.ioError = broken;
    if (recovered) {
        state_.metadata = meta;
        state_.points = state_.durablePoints = count;
        state_.ioError = meta.reason == TestReason::Incomplete;
    }
    portEXIT_CRITICAL(&mutex_);
    return true;
}

RecorderStart Recorder::requestStart(const TestMetadata& meta) {
    if (!meta.plan.valid() || !meta.testId || !meta.activeMask || meta.activeMask > 0x1ff ||
        !(meta.confirmedDuty > 0 && meta.confirmedDuty <= 1) || meta.confirmedDuty * 100 > meta.plan.pwmPercent) {
        return RecorderStart::Invalid;
    }
    RecorderStart result = RecorderStart::Queued;
    portENTER_CRITICAL(&mutex_);
    if (!state_.available) result = RecorderStart::Unavailable;
    else if (state_.downloadActive) result = RecorderStart::DownloadActive;
    else if (state_.running || startResult_ != Result::None || finishResult_ != Result::None) result = RecorderStart::Busy;
    else {
        pending_ = meta;
        startResult_ = Result::Pending;
        state_.startFailure = RecorderStartFailure::None;
    }
    portEXIT_CRITICAL(&mutex_);
    return result;
}

bool Recorder::pollStart(bool& success) {
    portENTER_CRITICAL(&mutex_);
    const bool ready = startResult_ == Result::Success || startResult_ == Result::Failure;
    if (ready) {
        success = startResult_ == Result::Success;
        startResult_ = Result::None;
    }
    portEXIT_CRITICAL(&mutex_);
    return ready;
}

bool Recorder::record(const TestPoint& point) {
    if (!point.valid()) return false;
    portENTER_CRITICAL(&mutex_);
    const bool accepted = state_.running && finishResult_ == Result::None && state_.queueDepth < points_.size() &&
                          submitted_ < kMaximumTestPoints;
    if (accepted) {
        points_[(pointHead_ + state_.queueDepth) % points_.size()] = point;
        ++state_.queueDepth;
        state_.points = ++submitted_;
    }
    portEXIT_CRITICAL(&mutex_);
    return accepted;
}

bool Recorder::marker(const TestMetadata& meta) {
    const bool first = meta.phase == TestPhase::Baseline;
    const TestRecord bytes = first ? encodeStartMarker(meta) : TestRecord{};
    portENTER_CRITICAL(&mutex_);
    const bool accepted = state_.running && meta.testId == state_.metadata.testId &&
                          finishResult_ == Result::None && markerCount_ < markers_.size();
    if (accepted) {
        const unsigned index = (markerHead_ + markerCount_) % markers_.size();
        // Snapshot the submitted point count to preserve point/marker order.
        markers_[index] = {first ? bytes : encodePhaseMarker(meta, submitted_, false), submitted_};
        ++markerCount_;
        state_.metadata = meta;
    }
    portEXIT_CRITICAL(&mutex_);
    return accepted;
}

bool Recorder::requestFinish(const TestMetadata& meta) {
    portENTER_CRITICAL(&mutex_);
    const bool accepted = state_.running && meta.testId == state_.metadata.testId && finishResult_ == Result::None;
    if (accepted) {
        pending_ = meta;
        state_.metadata = meta;
        finishResult_ = Result::Pending;
    }
    portEXIT_CRITICAL(&mutex_);
    return accepted;
}

bool Recorder::pollFinish(bool& success) {
    portENTER_CRITICAL(&mutex_);
    const bool ready = finishResult_ == Result::Success || finishResult_ == Result::Failure;
    if (ready) {
        success = finishResult_ == Result::Success;
        finishResult_ = Result::None;
    }
    portEXIT_CRITICAL(&mutex_);
    return ready;
}

RecorderStatus Recorder::status() const {
    portENTER_CRITICAL(&mutex_);
    const auto value = state_;
    portEXIT_CRITICAL(&mutex_);
    return value;
}

RecorderStartFailure Recorder::openTest(const TestMetadata& meta) {
    const int candidateSlot = activeSlot_ == 0 ? 1 : 0;
    const char* candidatePath = kTestPaths[candidateSlot];
    if ((::unlink(kSelectorTemp) != 0 && errno != ENOENT) ||
        (::unlink(candidatePath) != 0 && errno != ENOENT)) return RecorderStartFailure::Io;
    const size_t required = kTestHeaderBytes + (meta.plan.expectedPoints() + kMarkerReserve) * kTestRecordBytes +
                            kFilesystemReserveBytes;
    const size_t total = LittleFS.totalBytes();
    const size_t used = LittleFS.usedBytes();
    // Reserve enough while the prior test still exists, including COW metadata blocks.
    if (used > total || total - used < required) return RecorderStartFailure::Capacity;
    const int candidate = ::open(candidatePath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (candidate < 0) return RecorderStartFailure::Io;
    const auto header = encodeTestHeader(meta);
    if (!writeExactly(candidate, header.data(), header.size()) || ::fsync(candidate) != 0) {
        ::close(candidate);
        ::unlink(candidatePath);
        return RecorderStartFailure::Io;
    }
    // This ESP32 LittleFS VFS rejects renaming an open file. The candidate stays
    // open and the closed selector is replaced atomically after both are synced.
    std::array<uint8_t, 24> selector{};
    putLe32(selector.data(), kSelectorMagic);
    putLe32(selector.data() + 4, 1);
    putLe32(selector.data() + 8, candidateSlot);
    putLe64(selector.data() + 12, meta.testId);
    putLe32(selector.data() + 20, crc32(selector.data(), 20));
    const int index = ::open(kSelectorTemp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    bool indexReady = false;
    if (index >= 0) {
        indexReady = writeExactly(index, selector.data(), selector.size()) && ::fsync(index) == 0;
        indexReady = (::close(index) == 0) && indexReady;
    }
    if (!indexReady) {
        ::close(candidate);
        ::unlink(candidatePath);
        ::unlink(kSelectorTemp);
        return RecorderStartFailure::Io;
    }
    if (::rename(kSelectorTemp, kSelectorPath) != 0) {
        // Preserve both files when the filesystem reports a failed commit.
        // A reboot can still resolve whichever complete selector survived.
        ::close(candidate);
        return RecorderStartFailure::Io;
    }
    const int previous = activeSlot_;
    activeSlot_ = candidateSlot;
    writer_ = candidate;
    // The other slot is unreachable after commit. If cleanup fails, the next
    // start retries it before calculating the usable capacity.
    if (previous >= 0) ::unlink(kTestPaths[previous]);
    return RecorderStartFailure::None;
}

bool Recorder::flush(uint64_t nowUs) {
    if (writer_ < 0 || ::fsync(writer_) != 0) return false;
    lastFlushUs_ = nowUs;
    portENTER_CRITICAL(&mutex_);
    state_.durablePoints = written_;
    portEXIT_CRITICAL(&mutex_);
    return true;
}

void Recorder::service(uint64_t nowUs) {
    // Service bounded work per pass so configuration and energy can share the worker.
    for (unsigned i = 0; i < 4; ++i) serviceOne(nowUs);
    if (writer_ >= 0 && nowUs - lastFlushUs_ >= kTestFlushMs * 1000ULL) {
        if (!flush(nowUs)) failRecording();
    }
}

void Recorder::serviceOne(uint64_t nowUs) {
    Work work = Work::None;
    TestPoint point;
    TestRecord markerBytes;
    TestMetadata meta;
    portENTER_CRITICAL(&mutex_);
    if (startResult_ == Result::Pending) {
        work = Work::Start;
        meta = pending_;
    } else if (writer_ >= 0 && markerCount_ && markers_[markerHead_].afterPoints == written_) {
        work = Work::Marker;
        markerBytes = markers_[markerHead_].data;
        markerHead_ = (markerHead_ + 1) % markers_.size();
        --markerCount_;
    } else if (writer_ >= 0 && state_.queueDepth) {
        work = Work::Point;
        point = points_[pointHead_];
        pointHead_ = (pointHead_ + 1) % points_.size();
        --state_.queueDepth;
    } else if (writer_ >= 0 && finishResult_ == Result::Pending) {
        work = Work::Finish;
        meta = pending_;
    }
    portEXIT_CRITICAL(&mutex_);

    switch (work) {
    case Work::None: return;
    case Work::Start: {
        const auto error = openTest(meta);
        portENTER_CRITICAL(&mutex_);
        state_.startFailure = error;
        if (error == RecorderStartFailure::None) {
            meta.phase = TestPhase::Precheck;
            state_.metadata = meta;
            state_.points = state_.durablePoints = 0;
            state_.queueDepth = pointHead_ = markerHead_ = markerCount_ = 0;
            submitted_ = written_ = 0;
            lastFlushUs_ = nowUs;
            state_.running = true;
            state_.ioError = false;
            startResult_ = Result::Success;
        } else {
            if (error == RecorderStartFailure::Io) state_.ioError = true;
            startResult_ = Result::Failure;
        }
        portEXIT_CRITICAL(&mutex_);
        return;
    }
    case Work::Point: {
        const auto bytes = encodeTestPoint(point);
        if (!writeExactly(writer_, bytes.data(), bytes.size())) failRecording();
        else ++written_;
        return;
    }
    case Work::Marker:
        if (!writeExactly(writer_, markerBytes.data(), markerBytes.size()) || !flush(nowUs)) failRecording();
        return;
    case Work::Finish: {
        if (meta.reason == TestReason::None && written_ != meta.plan.expectedPoints()) meta.reason = TestReason::Incomplete;
        const auto bytes = encodePhaseMarker(meta, written_, true);
        if (!writeExactly(writer_, bytes.data(), bytes.size()) || !flush(nowUs)) {
            failRecording();
            return;
        }
        const bool closed = ::close(writer_) == 0;
        writer_ = -1;
        if (!closed) {
            failRecording();
            return;
        }
        portENTER_CRITICAL(&mutex_);
        meta.phase = meta.reason == TestReason::None ? TestPhase::Complete : TestPhase::Aborted;
        state_.metadata = meta;
        state_.running = false;
        state_.ioError = meta.reason == TestReason::Incomplete;
        finishResult_ = state_.ioError ? Result::Failure : Result::Success;
        portEXIT_CRITICAL(&mutex_);
        return;
    }
    }
}

void Recorder::failRecording() {
    // Signal the controller before closing the descriptor, which can itself write flash.
    portENTER_CRITICAL(&mutex_);
    state_.ioError = true;
    portEXIT_CRITICAL(&mutex_);
    if (writer_ >= 0) ::close(writer_);
    writer_ = -1;
    // The already-synced prefix remains available after the storage owner closes.
    portENTER_CRITICAL(&mutex_);
    state_.metadata.reason = TestReason::Io;
    state_.metadata.phase = TestPhase::Aborted;
    state_.points = state_.durablePoints;
    state_.queueDepth = markerCount_ = 0;
    state_.running = false;
    state_.ioError = true;
    finishResult_ = Result::Failure;
    portEXIT_CRITICAL(&mutex_);
}

bool Recorder::recover(TestMetadata& meta, uint32_t& points) {
    std::array<uint8_t, 24> selector{};
    const int index = ::open(kSelectorPath, O_RDONLY);
    if (index < 0) return false;
    const auto selectorLength = ::read(index, selector.data(), selector.size());
    ::close(index);
    if (selectorLength != static_cast<ssize_t>(selector.size()) || getLe32(selector.data()) != kSelectorMagic ||
        getLe32(selector.data() + 4) != 1 || getLe32(selector.data() + 8) > 1 ||
        getLe32(selector.data() + 20) != crc32(selector.data(), 20)) return false;
    activeSlot_ = static_cast<int>(getLe32(selector.data() + 8));
    const char* testPath = kTestPaths[activeSlot_];
    const int file = ::open(testPath, O_RDONLY);
    if (file < 0) return false;
    TestHeader header{};
    if (::read(file, header.data(), header.size()) != static_cast<ssize_t>(header.size()) || !decodeTestHeader(header, meta) ||
        meta.testId != getLe64(selector.data() + 12)) {
        ::close(file);
        return false;
    }
    TestRecord bytes;
    TestPrefix prefix(meta);
    ssize_t length = 0;
    for (unsigned i = 0; i < kMaximumTestPoints + kMarkerReserve; ++i) {
        length = ::read(file, bytes.data(), bytes.size());
        if (length != static_cast<ssize_t>(bytes.size()) || !prefix.append(bytes)) break;
    }
    ::close(file);
    meta = prefix.metadata();
    points = prefix.points();
    if (!prefix.terminal() || length != 0) {
        meta.phase = TestPhase::Aborted;
        const bool corruptConclusion = prefix.terminal() || length < 0 || (length >= 4 && terminalFrameTag(bytes));
        meta.reason = corruptConclusion ? TestReason::Incomplete : TestReason::Reset;
        if (length == 0) {
            // An intact RUNNING prefix can receive a durable ABORTED_RESET footer.
            const int append = ::open(testPath, O_WRONLY | O_APPEND);
            if (append >= 0) {
                const auto footer = encodePhaseMarker(meta, points, true);
                const bool ok = writeExactly(append, footer.data(), footer.size()) && ::fsync(append) == 0;
                const bool closed = ::close(append) == 0;
                if (!ok || !closed) meta.reason = TestReason::Incomplete;
            } else {
                meta.reason = TestReason::Incomplete;
            }
        }
    }
    return true;
}

bool Recorder::beginDownload() {
    portENTER_CRITICAL(&mutex_);
    const bool acquired = state_.metadata.testId != 0 && !state_.running && !state_.downloadActive &&
                          startResult_ != Result::Pending;
    if (acquired) {
        state_.downloadActive = true;
        readerState_ = state_;
    }
    const int slot = acquired ? activeSlot_ : -1;
    portEXIT_CRITICAL(&mutex_);
    if (!acquired) return false;
    reader_ = slot >= 0 ? ::open(kTestPaths[slot], O_RDONLY) : -1;
    if (reader_ < 0 || ::lseek(reader_, kTestHeaderBytes, SEEK_SET) != static_cast<off_t>(kTestHeaderBytes)) {
        endDownload();
        return false;
    }
    readerHeaderLine_ = 0;
    readerPoints_ = 0;
    return true;
}

int Recorder::nextCsv(char* output, size_t capacity) {
    if (reader_ < 0) return -1;
    if (readerHeaderLine_ < 23) return formatCsvHeader(output, capacity, readerHeaderLine_++);
    if (readerPoints_ == readerState_.durablePoints) return 0;
    TestRecord bytes;
    for (size_t i = 0; i < kMarkerReserve; ++i) {
        if (::read(reader_, bytes.data(), bytes.size()) != static_cast<ssize_t>(bytes.size())) return -1;
        const auto type = inspectFrame(bytes);
        if (type == RecordFrame::Invalid || type == RecordFrame::Terminal) return -1;
        if (type != RecordFrame::Point) continue;
        TestPoint point;
        if (!decodeTestPoint(bytes, point)) return -1;
        ++readerPoints_;
        return formatCsvPoint(output, capacity, point);
    }
    return -1;
}

void Recorder::endDownload() {
    if (reader_ >= 0) ::close(reader_);
    reader_ = -1;
    portENTER_CRITICAL(&mutex_);
    state_.downloadActive = false;
    portEXIT_CRITICAL(&mutex_);
}

int Recorder::formatCsvHeader(char* output, size_t capacity, unsigned line) const {
    const auto& meta = readerState_.metadata;
    char a[16], b[16], c[16], x[24], y[24], z[24];
    const bool lossUnknown = meta.reason == TestReason::Reset || meta.reason == TestReason::Io ||
                             meta.reason == TestReason::Incomplete;
    int n = -1;
    switch (line) {
    case 0: n = std::snprintf(output, capacity, "# schema_version=%u\n# firmware_version=%s\n# test_id=%llu\n# boot_id=%llu\n# config_revision=%lu\n",
        kTestSchemaVersion, meta.firmwareVersion.data(), static_cast<unsigned long long>(meta.testId),
        static_cast<unsigned long long>(meta.bootId), static_cast<unsigned long>(meta.configRevision)); break;
    case 1: n = std::snprintf(output, capacity, "# start_uptime_us=%llu\n# utc_synchronized=%s\n",
        static_cast<unsigned long long>(meta.startUs), meta.startUtcSeconds ? "true" : "false"); break;
    case 2: n = meta.startUtcSeconds
        ? std::snprintf(output, capacity, "# start_utc_unix_s=%llu\n", static_cast<unsigned long long>(meta.startUtcSeconds))
        : std::snprintf(output, capacity, "# start_utc_unix_s=unbekannt\n"); break;
    case 3: n = std::snprintf(output, capacity, "# baseline_minutes=%u\n# heat_minutes=%u\n# cooldown_minutes=%u\n# interval_s=10\n",
        meta.plan.baselineMinutes, meta.plan.heatMinutes, meta.plan.cooldownMinutes); break;
    case 4: n = std::snprintf(output, capacity, "# heat_start_ms=%s\n# heat_stop_ms=%s\n# end_ms=%s\n",
        optionalTime(meta.heatStartMs, a), optionalTime(meta.heatStopMs, b), optionalTime(meta.endMs, c)); break;
    case 5: n = std::snprintf(output, capacity, "# pwm_requested_percent=%.9g\n# pwm_confirmed_percent=%.9g\n# active_mask=0x%03x\n",
        meta.plan.pwmPercent, meta.confirmedDuty * 100, meta.activeMask); break;
    case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14:
        n = std::snprintf(output, capacity, "# plate_rom_p%u=%016llX\n", line - 5,
            static_cast<unsigned long long>(meta.plateRomIds[line - 6])); break;
    case 15: n = std::snprintf(output, capacity, "# heater_resistance_ohm=%s\n# heater_input_current_limit_a=%s\n# max_plate_temp_c=%s\n",
        optionalFloat(meta.resistanceOhm, x), optionalFloat(meta.currentLimitA, y), optionalFloat(meta.plateLimitC, z)); break;
    case 16: n = std::snprintf(output, capacity, "# status=%s\n# reason=%s\n# trigger=%s\n# durable_points=%lu\n# unflushed_loss=%s\n# max_flush_interval_s=%lu\n",
        testPhaseCode(meta.phase), testReasonCode(meta.reason), optionalFloat(meta.trigger, x),
        static_cast<unsigned long>(readerState_.durablePoints), lossUnknown ? "unbekannt" : "0",
        static_cast<unsigned long>(kTestFlushMs / 1000)); break;
    case 17: n = std::snprintf(output, capacity, "# vin_gain=%s\n# vin_offset_v=%s\n# supply_rated_current_a=%s\n",
        optionalFloat(meta.vinGain, x), optionalFloat(meta.vinOffsetV, y), optionalFloat(meta.supplyRatedCurrentA, z)); break;
    case 18: n = std::snprintf(output, capacity, "# vin_valid_min_v=%s\n# vin_valid_max_v=%s\n",
        optionalFloat(meta.vinMinV, x), optionalFloat(meta.vinMaxV, y)); break;
    case 19: n = std::snprintf(output, capacity, "# pi_kp_duty_per_k=%s\n# pi_ti_s=%s\n# pi_interval_ms=%lu\n# pi_integral_enabled=%s\n",
        optionalFloat(meta.piKp, x), optionalFloat(meta.piTiS, y), static_cast<unsigned long>(meta.piIntervalMs),
        meta.piIntegralEnabled ? "true" : "false"); break;
    case 20: n = std::snprintf(output, capacity, "# heating_target_c=%s\n# cooling_target_c=%s\n",
        optionalFloat(meta.heatingTargetC, x), optionalFloat(meta.coolingTargetC, y)); break;
    case 21: n = std::snprintf(output, capacity, "# sample_order=t=0 vor Heizsprung, spaetere Phasengrenzen nach Ausgangswechsel\n"); break;
    case 22: n = std::snprintf(output, capacity, "%s", kTestCsvColumns); break;
    default: break;
    }
    return n >= 0 && static_cast<size_t>(n) < capacity ? n : -1;
}

} // namespace klima
