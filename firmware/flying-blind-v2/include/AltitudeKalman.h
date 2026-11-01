#pragma once

#include <cmath>

// Experimental barometer-only constant-velocity Kalman filter.
// State: [relative altitude (m), vertical velocity (m/s)].
// No IMU acceleration is fused in this implementation.
class AltitudeKalman
{
public:
    // Provisional bench settings, not a validated sensor/flight model.
    static constexpr double measurementVarianceM2 = 1.0;
    static constexpr double accelerationVarianceM2S4 = 1.0;

    bool initialized() const { return ready_; }
    double altitudeM() const { return height_; }
    double velocityMps() const { return velocity_; }
    double altitudeSigmaM() const { return std::sqrt(p00_); }
    double velocitySigmaMps() const { return std::sqrt(p11_); }

    bool reset(double altitudeM)
    {
        if (!std::isfinite(altitudeM)) return false;
        height_ = altitudeM;
        velocity_ = 0.0;
        p00_ = measurementVarianceM2;
        p01_ = 0.0;
        p11_ = 25.0; // Initial velocity variance (m/s)^2.
        ready_ = true;
        return true;
    }

    bool update(double measuredAltitudeM, double dtSeconds)
    {
        if (!ready_ || !std::isfinite(measuredAltitudeM) ||
            !std::isfinite(dtSeconds) || dtSeconds <= 0.0) return false;

        const double dt2 = dtSeconds * dtSeconds;
        const double q = accelerationVarianceM2S4;
        // F = [[1,dt],[0,1]], Q = G*q*G', G = [dt^2/2,dt]'.
        // q models an independent unknown acceleration during each interval.
        const double a = p00_ + 2.0 * dtSeconds * p01_ + dt2 * p11_
                       + 0.25 * dt2 * dt2 * q;
        const double b = p01_ + dtSeconds * p11_
                       + 0.5 * dt2 * dtSeconds * q;
        const double c = p11_ + dt2 * q;
        const double innovationVariance = a + measurementVarianceM2;
        if (!std::isfinite(innovationVariance) || innovationVariance <= 0.0)
            return false;

        const double k0 = a / innovationVariance;
        const double k1 = b / innovationVariance;
        const double predictedHeight = height_ + dtSeconds * velocity_;
        const double innovation = measuredAltitudeM - predictedHeight;
        const double nextHeight = predictedHeight + k0 * innovation;
        const double nextVelocity = velocity_ + k1 * innovation;

        // Joseph covariance update: (I-KH)P(I-KH)' + KRK'.
        // Store only the symmetric covariance's three distinct elements.
        const double u = 1.0 - k0;
        const double next00 = u * u * a + k0 * k0 * measurementVarianceM2;
        const double next01 = u * (b - k1 * a)
                            + k0 * k1 * measurementVarianceM2;
        const double next11 = c - 2.0 * k1 * b + k1 * k1 * a
                            + k1 * k1 * measurementVarianceM2;
        if (!std::isfinite(nextHeight) || !std::isfinite(nextVelocity) ||
            !std::isfinite(next00) || !std::isfinite(next01) ||
            !std::isfinite(next11) || next00 < 0.0 || next11 < 0.0)
            return false;

        height_ = nextHeight;
        velocity_ = nextVelocity;
        p00_ = next00;
        p01_ = next01;
        p11_ = next11;
        return true;
    }

private:
    bool ready_ = false;
    double height_ = 0.0;
    double velocity_ = 0.0;
    double p00_ = measurementVarianceM2;
    double p01_ = 0.0;
    double p11_ = 25.0;
};

// Conventional standard-atmosphere pressure-ratio approximation.
// Same pressure units required for both arguments; output is relative meters.
// A first-sample reference is noisy and is not surveyed height or MSL altitude.
inline double relativeBarometricAltitudeM(double pressure, double reference)
{
    if (!std::isfinite(pressure) || !std::isfinite(reference) ||
        pressure <= 0.0 || reference <= 0.0)
        return NAN;
    return 44330.0 * (1.0 - std::pow(pressure / reference, 0.1903));
}
