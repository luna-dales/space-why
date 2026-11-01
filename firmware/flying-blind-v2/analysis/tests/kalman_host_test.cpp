#include "AltitudeKalman.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <random>

// Independent full-matrix implementation for numerical comparison.
struct Reference {
    double x[2] = {0, 0};
    double p[2][2] = {{1, 0}, {0, 25}};
    void step(double measurement, double dt) {
        double f[2][2] = {{1, dt}, {0, 1}};
        double g[2] = {dt * dt / 2, dt};
        double predicted[2][2] = {};
        for (int i=0;i<2;i++) for(int j=0;j<2;j++) {
            for(int k=0;k<2;k++) for(int l=0;l<2;l++)
                predicted[i][j] += f[i][k] * p[k][l] * f[j][l];
            predicted[i][j] += g[i] * g[j];
        }
        double xp[2] = {x[0] + dt*x[1], x[1]};
        double gain[2] = {predicted[0][0]/(predicted[0][0]+1),
                          predicted[1][0]/(predicted[0][0]+1)};
        double m[2][2] = {{1-gain[0],0},{-gain[1],1}};
        for(int i=0;i<2;i++) {
            x[i] = xp[i] + gain[i]*(measurement-xp[0]);
            for(int j=0;j<2;j++) {
                p[i][j] = gain[i]*gain[j];
                for(int k=0;k<2;k++) for(int l=0;l<2;l++)
                    p[i][j] += m[i][k]*predicted[k][l]*m[j][l];
            }
        }
    }
};

int main() {
    assert(std::abs(relativeBarometricAltitudeM(1005,1005)) < 1e-10);
    assert(relativeBarometricAltitudeM(1004,1005)>0);
    assert(relativeBarometricAltitudeM(1006,1005)<0);
    assert(std::isnan(relativeBarometricAltitudeM(0,1005)));
    const double pressureAt10m = 1005 * std::pow(1-10.0/44330,1/0.1903);
    assert(std::abs(relativeBarometricAltitudeM(pressureAt10m,1005)-10)<1e-8);

    AltitudeKalman filter;
    assert(!filter.update(0,1));
    assert(filter.reset(0));
    const double nan = std::numeric_limits<double>::quiet_NaN();
    assert(!filter.update(nan,1));
    assert(!filter.update(1,0));
    assert(!filter.update(1,-1));
    assert(!filter.update(1,nan));
    assert(filter.altitudeM()==0);

    Reference reference;
    std::mt19937 generator(42);
    std::normal_distribution<double> noise(0,1);
    double t=0;
    const double steps[] = {0.1,0.25,0.7,1,2,4};
    for(int i=0;i<3000;i++) {
        double dt=steps[i%6];
        t+=dt;
        double measurement=0.7*t+noise(generator);
        assert(filter.update(measurement,dt));
        reference.step(measurement,dt);
        assert(std::abs(filter.altitudeM()-reference.x[0])<1e-8);
        assert(std::abs(filter.velocityMps()-reference.x[1])<1e-8);
        assert(std::abs(filter.altitudeSigmaM()*filter.altitudeSigmaM()
                        -reference.p[0][0])<1e-8);
        assert(std::abs(filter.velocitySigmaMps()*filter.velocitySigmaMps()
                        -reference.p[1][1])<1e-8);
        assert(reference.p[0][0]*reference.p[1][1]
               -reference.p[0][1]*reference.p[1][0]>=-1e-10);
    }

    filter.reset(0);
    double rawError=0,filteredError=0;
    for(int i=0;i<1200;i++) {
        double measured=noise(generator);
        assert(filter.update(measured,0.1));
        if(i>=200) {
            rawError+=measured*measured;
            filteredError+=filter.altitudeM()*filter.altitudeM();
        }
    }
    assert(filteredError<rawError);
    std::printf("PASS: atmosphere conversion, invalid inputs, 3000 variable-dt "
                "matrix comparisons, covariance checks, stationary simulation.\n");
    std::printf("Synthetic stationary RMSE: raw %.4f m, filtered %.4f m.\n",
                std::sqrt(rawError/1000),std::sqrt(filteredError/1000));
}
