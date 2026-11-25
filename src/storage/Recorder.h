#pragma once

#include "RecordFormat.h"

#include <freertos/FreeRTOS.h>

namespace klima {

enum class RecorderStart { Queued, Unavailable, Busy, DownloadActive, Invalid };
enum class RecorderStartFailure { None, Capacity, Io };

struct RecorderStatus {
    TestMetadata metadata;
    uint32_t points = 0, durablePoints = 0;
    uint8_t queueDepth = 0;
    bool available = false, running = false, downloadActive = false, ioError = false;
    RecorderStartFailure startFailure = RecorderStartFailure::None;
};

class Recorder {
public:
    bool begin();

    // Control-task entry points. Each copies bounded RAM state only.
    RecorderStart requestStart(const TestMetadata& metadata);
    bool pollStart(bool& success);
    bool record(const TestPoint& point);
    bool marker(const TestMetadata& metadata);
    bool requestFinish(const TestMetadata& metadata);
    bool pollFinish(bool& success);
    RecorderStatus status() const;

    // Storage task owns writes, fsync and recovery.
    void service(uint64_t nowUs);

    // Dedicated transfer task owns the reader. A lease excludes writers.
    bool beginDownload();
    int nextCsv(char* buffer, size_t capacity);
    void endDownload();

private:
    enum class Result : uint8_t { None, Pending, Success, Failure };
    enum class Work : uint8_t { None, Start, Point, Marker, Finish };
    struct DeferredMarker { TestRecord data; uint32_t afterPoints; };

    void serviceOne(uint64_t nowUs);
    RecorderStartFailure openTest(const TestMetadata& metadata);
    void failRecording();
    bool recover(TestMetadata& metadata, uint32_t& points);
    bool flush(uint64_t nowUs);
    int formatCsvHeader(char* buffer, size_t capacity, unsigned line) const;

    mutable portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
    RecorderStatus state_;
    TestMetadata pending_;
    Result startResult_ = Result::None, finishResult_ = Result::None;
    std::array<TestPoint, 16> points_{};
    std::array<DeferredMarker, 4> markers_{};
    uint8_t pointHead_ = 0, markerHead_ = 0, markerCount_ = 0;
    uint32_t submitted_ = 0, written_ = 0;
    uint64_t lastFlushUs_ = 0;
    int activeSlot_ = -1;
    int writer_ = -1;
    int reader_ = -1;
    unsigned readerHeaderLine_ = 0;
    uint32_t readerPoints_ = 0;
    RecorderStatus readerState_;
};

} // namespace klima
