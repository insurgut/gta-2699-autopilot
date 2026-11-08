#pragma once
#include "gps_protocol.hpp"
#include "route.hpp"

constexpr uint32_t GPS_ROUTE_COLOR = 0xa875ff;
constexpr float GPS_RIBBON_LENGTH = 4.0f;
constexpr float GPS_GRID_Z = -.68f;
inline std::vector<Point> gpsRoute(const GpsFrame& frame, float speed) {
    std::vector<Point> route;
    route.reserve(std::min(frame.count, GPS_MAX_POINTS));
    for (unsigned i = 0; i < std::min(frame.count, GPS_MAX_POINTS); ++i) {
        const auto& p = frame.points[i];
        route.push_back({p.x, p.y, p.z, speed, 0, 1});
    }
    return route;
}
inline float gpsEdgeProgress(Point a, Point b, Point car) {
    const float length = distance(a, b);
    if (length < .001f) return 0;
    const float along = ((car.x - a.x) * (b.x - a.x) +
                         (car.y - a.y) * (b.y - a.y) +
                         (car.z - a.z) * (b.z - a.z)) / length;
    return std::clamp(along, 0.0f, length);
}
inline GpsPoint gpsInterpolate(GpsPoint a, GpsPoint b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}
inline std::vector<GpsPoint> gpsDistanceSamples(const std::vector<Point>& path, size_t from,
                                                float end, float step, float start = 0) {
    std::vector<GpsPoint> out;
    float arc = 0;
    for (size_t i = from; i + 1 < path.size() && arc <= end; ++i) {
        const auto &a = path[i], &b = path[i + 1];
        const float length = distance(a, b);
        if (length < .001f) continue;
        while (start <= arc + length && start <= end) {
            out.push_back(gpsInterpolate({a.x,a.y,a.z}, {b.x,b.y,b.z}, (start - arc) / length));
            start += step;
        }
        arc += length;
    }
    return out;
}
inline std::vector<GpsPoint> gpsRibbon(const std::vector<Point>& path, size_t from,
                                       Point car, float fx, float fy, float halfLength) {
    if (from + 1 >= path.size()) return {};
    const float forwardLength = std::hypot(fx, fy);
    if (forwardLength < .5f) return {};
    fx /= forwardLength;
    fy /= forwardLength;
    const float noseDistance = halfLength > 0 ? halfLength : 2.5f;
    const float progress = gpsEdgeProgress(path[from], path[from + 1], car);
    const float start = progress + noseDistance;
    auto road = gpsDistanceSamples(path, from, start + GPS_RIBBON_LENGTH, .5f, start + .5f);
    if (road.empty()) return {};
    const GpsPoint nose{car.x + fx * noseDistance, car.y + fy * noseDistance, car.z};
    std::vector<GpsPoint> smooth{nose, {nose.x + fx * .35f, nose.y + fy * .35f, nose.z}};
    const auto &a = path[from], &b = path[from + 1];
    const float length = distance(a,b);
    const float roadZ = a.z + (b.z - a.z) * (length > .001f ? progress / length : 0);
    for (auto p : road) {
        p.z += car.z - roadZ;
        if ((p.x - nose.x) * fx + (p.y - nose.y) * fy <= .2f) continue;
        const float gap = gpsDistance(smooth.back(), p);
        if (gap > .03f) smooth.push_back(p);
    }
    if (smooth.size() < 3) return {};
    // Two corner-cutting passes leave the nose and its tangent fixed.
    for (int pass = 0; pass < 2; ++pass) {
        std::vector<GpsPoint> next{smooth.front()};
        for (size_t i = 1; i < smooth.size(); ++i) {
            next.push_back(gpsInterpolate(smooth[i - 1], smooth[i], .25f));
            next.push_back(gpsInterpolate(smooth[i - 1], smooth[i], .75f));
        }
        next.push_back(smooth.back());
        smooth = std::move(next);
    }
    float remaining = GPS_RIBBON_LENGTH;
    for (size_t i = 1; i < smooth.size(); ++i) {
        const float length = gpsDistance(smooth[i - 1], smooth[i]);
        if (length > remaining) {
            smooth[i] = gpsInterpolate(smooth[i - 1], smooth[i], remaining / length);
            smooth.resize(i + 1);
            break;
        }
        remaining -= length;
    }
    return smooth;
}
inline float gpsWheelSpeed(float speed,int delta,float step) {
    return std::clamp(speed + float(delta) / 120 * step, 5.0f, 300.0f);
}
