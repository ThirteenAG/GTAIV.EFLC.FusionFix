#pragma once
#include "StableHeadlightSelector.hpp"

namespace fusionfix::shadows
{
    // Geometric relevance only: neither occlusion nor renderer visibility is
    // known here. CE spot lights store cos(outer half-angle), not degrees.
    inline bool LightVolumeContains(Vec3 target, Vec3 position, Vec3 direction,
                                    int type, float radius, float outerCos) noexcept
    {
        if (!std::isfinite(radius) || radius <= 0) return false;
        const double x = static_cast<double>(target.x) - position.x;
        const double y = static_cast<double>(target.y) - position.y;
        const double z = static_cast<double>(target.z) - position.z;
        const double squared = x * x + y * y + z * z;
        if (!std::isfinite(squared) || squared > static_cast<double>(radius) * radius) return false;
        if (type == 0) return true;
        if (type != 2 || !std::isfinite(outerCos) || outerCos <= 0 || outerCos > 1) return false;
        const double nx = direction.x, ny = direction.y, nz = direction.z;
        const double norm = nx * nx + ny * ny + nz * nz;
        const double dot = x * nx + y * ny + z * nz;
        return std::isfinite(norm) && norm > 0.0001 &&
            (squared < 0.0001 || (dot > 0 && dot * dot >=
                static_cast<double>(outerCos) * outerCos * squared * norm));
    }
}
