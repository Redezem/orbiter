#pragma once
#include <cmath>
#include <initializer_list>
#include <stdexcept>

namespace core_metal {
// Mesh.h / reference D3D9 Mesh.cpp: template material indices are ZERO-based.
// SPEC_DEFAULT (-1) selects opaque white; SPEC_INHERIT (-2) selects the previous
// resolved group, even when that previous group is hidden or has no indices.
inline constexpr unsigned defaultMaterial = unsigned(-1);
inline constexpr unsigned inheritMaterial = unsigned(-2);
inline unsigned resolveMaterial(unsigned raw, unsigned &previous, unsigned count) {
    unsigned resolved = raw == inheritMaterial ? previous : raw;
    if (resolved != defaultMaterial && resolved >= count)
        throw std::runtime_error("engine group material index out of range");
    previous = resolved;
    return resolved;
}
inline bool validDiffuse(float r, float g, float b, float a) {
    for (float v : {r,g,b,a})
        if (!std::isfinite(v) || v < 0 || v > 1) return false;
    return true;
}
// Independent mesh-only readback membership: RGB must be one normalized diffuse
// color scaled by the original interpolated normal shade, +/- UNORM half-LSB.
inline bool diffusePixel(const unsigned char *bgra, const float *rgba) {
    double low=.34999, high=.95001;
    if (bgra[3] != 255) return false;
    for (unsigned channel=0; channel<3; ++channel) {
        unsigned value=bgra[2-channel];
        double d=rgba[channel];
        if (d==0) { if(value) return false; }
        else {
            low=std::fmax(low,(value-.51)/(255*d));
            high=std::fmin(high,(value+.51)/(255*d));
        }
    }
    return low<=high;
}
}
