#pragma once
#include "gps_protocol.hpp"
#include <vector>

struct GpsBandSlice {
    GpsPoint left, right;
    float alpha;
};
inline std::vector<GpsBandSlice> gpsBandSlices(const std::vector<GpsPoint>& line, float halfWidth) {
    if (line.size() < 2 || !std::isfinite(halfWidth) || halfWidth < .2f || halfWidth > 5) return {};
    std::vector<float> arcs(line.size());
    for (size_t i = 1; i < line.size(); ++i) {
        const float length = gpsDistance(line[i - 1], line[i]);
        if (length > 15) return {};
        arcs[i] = arcs[i - 1] + length;
    }
    if (arcs.back() < .1f) return {};
    std::vector<GpsBandSlice> out;
    for (size_t i = 0; i < line.size(); ++i) {
        const auto p = line[i];
        const auto a = line[i ? i - 1 : i], b = line[std::min(i + 1, line.size() - 1)];
        const float dx = b.x - a.x, dy = b.y - a.y, length = std::hypot(dx, dy);
        if (length < .001f || !gpsPointValid(p)) return {};
        const float x = dy * halfWidth / length, y = dx * halfWidth / length;
        const float fade = std::clamp((arcs[i] / arcs.back() - .42f) / .58f, 0.0f, 1.0f);
        const float alpha = 96 * (1 - fade * fade * (3 - 2 * fade));
        out.push_back({{p.x - x,p.y + y,p.z}, {p.x + x,p.y - y,p.z}, alpha});
    }
    return out;
}
struct GpsScreenPoint {float x, y;};
