#pragma once

#include <cmath>

// Experimental barometer-only constant-velocity Kalman filter.
// State: [relative altitude (m), vertical velocity (m/s)].
// No IMU acceleration is fused in this implementation.
// The pressure measurement observes height directly (H=[1,0]); velocity is
// inferred through its covariance with height over successive measurements.
// Constant velocity is a prediction assumption, not a claim about rocket
// motion. Unknown acceleration is represented by process noise.
// Lifecycle: reset(firstHeight), then update(height, actualElapsedSeconds).
class AltitudeKalman
{
public:
    // Provisional bench settings, not a validated sensor/flight model.
    // R: assumed variance of the altitude observation, in square meters.
    // q: variance of a constant but unknown acceleration within each interval,
    // independent from interval to interval, in (m/s^2)^2. This is NOT an
    // accelerometer measurement or continuous white-noise spectral density.
    static constexpr double measurementVarianceM2 = 1.0;
    static constexpr double accelerationVarianceM2S4 = 1.0;

    // Getters describe the most recently accepted state. sigma=sqrt(variance)
    // is uncertainty under this model, not experimentally measured accuracy.
    bool initialized() const { return ready_; }
    double altitudeM() const { return height_; }
    double velocityMps() const { return velocity_; }
    double altitudeSigmaM() const { return std::sqrt(p00_); }
    double velocitySigmaMps() const { return std::sqrt(p11_); }

    // Establish a known reference observation with initially unknown motion.
    // Zero starting velocity is an initial estimate with substantial variance,
    // not a zero-velocity constraint. Invalid input leaves the state unchanged.
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

    // Advance to one new observation and return true only if accepted.
    // Calculation uses local candidates; rejected input or invalid arithmetic
    // leaves the stored state and covariance unchanged. Callers retain elapsed
    // time since their last accepted update when a measurement is skipped.
    bool update(double measuredAltitudeM, double dtSeconds)
    {
        if (!ready_ || !std::isfinite(measuredAltitudeM) ||
            !std::isfinite(dtSeconds) || dtSeconds <= 0.0) return false;

        const double dt2 = dtSeconds * dtSeconds;
        const double q = accelerationVarianceM2S4;
        // F = [[1,dt],[0,1]], Q = G*q*G', G = [dt^2/2,dt]'.
        // q models an independent unknown acceleration during each interval.
        // a,b,c are the predicted height variance, height/velocity covariance,
        // and velocity variance: P_pred = F*P*F' + Q. The off-diagonal term
        // carries the information needed to infer velocity from height.
        const double a = p00_ + 2.0 * dtSeconds * p01_ + dt2 * p11_
                       + 0.25 * dt2 * dt2 * q;
        const double b = p01_ + dtSeconds * p11_
                       + 0.5 * dt2 * dtSeconds * q;
        const double c = p11_ + dt2 * q;
        // S=H*P_pred*H'+R: expected variance of observation-minus-prediction.
        // Because H observes height only, this reduces to a+R.
        const double innovationVariance = a + measurementVarianceM2;
        if (!std::isfinite(innovationVariance) || innovationVariance <= 0.0)
            return false;

        // Kalman gains distribute the height discrepancy into height (k0)
        // and velocity (k1). A larger measurement variance trusts the model
        // more; larger predicted uncertainty trusts the observation more.
        const double k0 = a / innovationVariance;
        const double k1 = b / innovationVariance;
        const double predictedHeight = height_ + dtSeconds * velocity_;
        const double innovation = measuredAltitudeM - predictedHeight;
        const double nextHeight = predictedHeight + k0 * innovation;
        const double nextVelocity = velocity_ + k1 * innovation;

        // Joseph covariance update: (I-KH)P(I-KH)' + KRK'.
        // Store only the symmetric covariance's three distinct elements.
        // Joseph form retains the measurement-noise contribution explicitly,
        // reducing numerical cancellation relative to a subtractive update.
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

        // Commit only after candidate values and variance signs are checked.
        // No measurement outlier gate or pressure-weather model is included.
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
    // Symmetric P = [[p00_,p01_],[p01_,p11_]]. Units are m^2,
    // m^2/s, and m^2/s^2 respectively; storing three entries enforces symmetry.
    double p00_ = measurementVarianceM2;
    double p01_ = 0.0;
    double p11_ = 25.0;
};

// Conventional standard-atmosphere pressure-ratio approximation.
// Same pressure units required for both arguments; output is relative meters.
// A first-sample reference is noisy and is not surveyed height or MSL altitude.
// Pressure below the reference yields positive height; above it yields
// negative height. The constants approximate a standard atmosphere. Local
// weather, temperature profile, airflow, and reference noise can cause errors.
// Invalid inputs return NaN so the caller can reject the sample.
inline double relativeBarometricAltitudeM(double pressure, double reference)
{
    if (!std::isfinite(pressure) || !std::isfinite(reference) ||
        pressure <= 0.0 || reference <= 0.0)
        return NAN;
    return 44330.0 * (1.0 - std::pow(pressure / reference, 0.1903));
}
