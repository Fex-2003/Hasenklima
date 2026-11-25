#include "Model.h"

#include <algorithm>
#include <cmath>

// SI saturation and humidity-ratio equations adapted from PsychroLib v2.5.0.
// Copyright (c) 2018-2020 The PsychroLib Contributors.
// ASHRAE Handbook — Fundamentals (2017), chapter 1, equations 5, 6, 33 and 35.
// Source: https://github.com/psychrometrics/psychrolib/blob/2.5.0/src/c/psychrolib.c
// License: see docs/PsychroLib-LICENSE.txt.
namespace klima {
namespace {
constexpr double minHumidityRatio = 1e-7;
constexpr double minSaturationC = -100.0;
constexpr double toleranceK = 0.01;
constexpr unsigned maxIterations = 100;

double saturationPressure(double celsius) {
    const double kelvin = celsius + 273.15;
    const double logK = std::log(kelvin);
    const double logPressure = celsius <= 0.01
        ? -5674.5359 / kelvin + 6.3925247 + kelvin *
            (-0.009677843 + kelvin * (6.2215701e-7 + kelvin * (2.0747825e-9 - 9.484024e-13 * kelvin))) +
            4.1635019 * logK
        : -5800.2206 / kelvin + 1.3914993 + kelvin *
            (-0.048640239 + kelvin * (4.1764768e-5 - kelvin * 1.4452093e-8)) + 6.5459673 * logK;
    return std::exp(logPressure);
}

double humidityRatio(double vapourPressure, double totalPressure) {
    return std::max(minHumidityRatio, 0.621945 * vapourPressure / (totalPressure - vapourPressure));
}

double humidityRatioAtWetbulb(double dryC, double wetC, double pressurePa) {
    const double saturated = humidityRatio(saturationPressure(wetC), pressurePa);
    const double sensibleHeat = 1.006 * (dryC - wetC);
    if (wetC >= 0) {
        return std::max(minHumidityRatio,
            ((2501 - 2.326 * wetC) * saturated - sensibleHeat) / (2501 + 1.86 * dryC - 4.186 * wetC));
    }
    return std::max(minHumidityRatio,
        ((2830 - 0.24 * wetC) * saturated - sensibleHeat) / (2830 + 1.86 * dryC - 2.1 * wetC));
}
}  // namespace

float wetbulbC(float temperatureC, float humidityPct, float pressurePa) {
    if (!std::isfinite(temperatureC) || temperatureC < -40 || temperatureC > 85 ||
        !std::isfinite(humidityPct) || humidityPct < 0 || humidityPct > 100 ||
        !std::isfinite(pressurePa) || pressurePa < 30000 || pressurePa > 110000) return unset;
    const double vapourPressureAtSaturation = saturationPressure(temperatureC);
    if (vapourPressureAtSaturation >= pressurePa) return unset;
    if (humidityPct == 100) return temperatureC;
    const double target = humidityRatio(vapourPressureAtSaturation * humidityPct / 100.0, pressurePa);
    // Bracket directly from the saturation equation domain. This avoids a separate
    // Newton solve for the dew point and does not change the root of the same SI equation.
    double low = minSaturationC;
    double high = temperatureC;
    for (unsigned i = 0; i < maxIterations; ++i) {
        const double middle = (low + high) / 2;
        if (high - low <= toleranceK) return static_cast<float>(middle);
        // The reference humidity ratio is clamped at 1e-7. Keeping equality
        // on the low side selects the boundary of that clamp for RH = 0.
        if (humidityRatioAtWetbulb(temperatureC, middle, pressurePa) > target) high = middle;
        else low = middle;
    }
    return unset;
}
}  // namespace klima
