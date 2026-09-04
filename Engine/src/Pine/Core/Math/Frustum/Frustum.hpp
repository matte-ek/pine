#pragma once

#include <array>

#include "Pine/Core/Math/Math.hpp"

namespace Pine
{
    // The volume a projection can see, as six inward-facing world-space planes.
    //
    // Built from a view-projection matrix rather than from a camera, so anything that projects is a
    // valid source: the scene camera, a shadow cascade, a spot light's cone, one face of a point
    // light. Culling against it therefore stays one mechanism instead of growing a variant per
    // caller.
    struct Frustum
    {
        enum PlaneIndex
        {
            Left = 0,
            Right,
            Bottom,
            Top,
            Near,
            Far,

            PlaneCount
        };

        // Each plane is vec4(normal.xyz, distance). A point p is on the inside when
        // dot(plane.xyz, p) + plane.w >= 0. Normals are normalized, so that value is also the
        // signed distance to the plane - which is what makes the extent test below work.
        std::array<Vector4f, PlaneCount> Planes = {};

        // Gribb-Hartmann extraction: each plane falls out of a sum or difference of two rows of the
        // matrix. Works for perspective and orthographic alike, which is why cascades come free.
        static Frustum FromViewProjection(const Matrix4f& viewProjection);

        // Both tests are conservative: they reject only what is certainly outside, and may accept
        // something just outside a corner. That is the correct bias for culling.
        bool Intersects(const Vector3f& boundsMin, const Vector3f& boundsMax) const;
        bool Intersects(const Vector3f& center, float radius) const;
    };
}
