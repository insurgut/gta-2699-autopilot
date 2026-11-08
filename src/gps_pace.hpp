#pragma once
#include "gps_navigation.hpp"

struct GpsBrakeModel {
    float measured = 4.0f;
    unsigned observations = 0;
    uint64_t lastTick = 0;
    float brakeSeconds = 0, elapsed = 0;
    float previousSpeed = 0;
    bool haveSpeed = false;

    void track(uint64_t now, bool braking) {
        if (lastTick && now > lastTick) {
            const float dt = float(now - lastTick) / 1000;
            if (dt > .25f) {
                haveSpeed = false;
                elapsed = brakeSeconds = 0;
            } else {
                elapsed += dt;
                if (braking) brakeSeconds += dt;
            }
        }
        lastTick = now;
    }

    void sample(float speed, bool eligible) {
        if (eligible && haveSpeed && elapsed >= .075f && elapsed <= .35f &&
            brakeSeconds >= elapsed * .65f && previousSpeed > 2 && speed > .6f) {
            const float deceleration = (previousSpeed - speed) / brakeSeconds;
            if (deceleration >= .8f && deceleration <= 12) {
                measured = std::clamp(measured + (deceleration - measured) * .15f, 1.5f, 8.0f);
                ++observations;
            }
        }
        previousSpeed = speed;
        haveSpeed = eligible;
        elapsed = brakeSeconds = 0;
    }

    float planningDeceleration() const { return measured * .72f; }
    float brakeDuty(float actual, float target) const {
        if (target <= .15f) return actual > .4f ? 1.0f : 0.0f;
        return std::clamp((actual - target - .35f) * 1.8f / measured, 0.0f, 1.0f);
    }
};

struct GpsTurnPace {
    float cap = 0, steer = 0;
    uint64_t lastUpdate = 0, lastSteer = 0;

    float update(uint64_t now, const std::vector<Point>& path, size_t segment,
                 Point car, float actual, float cruise, float deceleration) {
        if (lastUpdate && now - lastUpdate < 45) return std::min(cruise, cap);
        lastUpdate = now;
        cap = cruise;
        if (path.size() < 2) return cap;

        // Begin slightly behind the car so the turn remains visible through its apex.
        segment = std::min(segment, path.size() - 2);
        size_t first = segment;
        float behind = 0;
        while (first > 0 && behind < 10) {
            behind += distance(path[first], path[first - 1]);
            --first;
        }
        const float carArc = behind + gpsEdgeProgress(path[segment], path[segment + 1], car);
        const float horizon = std::max(25.0f, actual * actual / (2 * deceleration) + actual * .4f + 15);
        const auto samples = gpsDistanceSamples(path, first, carArc + horizon + 4, 2);
        const float minimumCorner = std::min(cruise, 14.0f / 3.6f);
        for (size_t i = 1; i + 1 < samples.size(); ++i) {
            const float ahead = float(i) * 2 - carArc;
            if (ahead < -2 || ahead > horizon) continue;
            const auto &a = samples[i - std::min<size_t>(2,i)], &b = samples[i],
                       &c = samples[std::min(i + 2, samples.size() - 1)];
            const float ax = b.x - a.x, ay = b.y - a.y, bx = c.x - b.x, by = c.y - b.y;
            const float inLength = std::hypot(ax, ay), outLength = std::hypot(bx, by);
            if (inLength < 1 || outLength < 1) continue;
            const float angle = std::abs(std::atan2(ax * by - ay * bx, ax * bx + ay * by));
            // The same angle spread over a longer arc permits a higher speed.
            const float curvature = 2 * angle / (inLength + outLength);
            if (curvature < .012f) continue;
            const float corner = std::clamp(std::sqrt(4.4f / curvature), minimumCorner, cruise);
            const float before = std::max(0.0f, ahead - 2.0f - actual * .28f);
            const float approach = std::sqrt(corner * corner + 2 * deceleration * before);
            cap = std::min(cap, approach);
        }
        return cap;
    }

    float smoothSteering(uint64_t now, float requested) {
        if (std::abs(requested) < .065f) requested = 0;
        if (!lastSteer || now - lastSteer > 400) steer = requested;
        else {
            const float dt = float(now - lastSteer) / 1000;
            steer += (requested - steer) * (1 - std::exp(-dt / .13f));
        }
        lastSteer = now;
        return steer;
    }
};
