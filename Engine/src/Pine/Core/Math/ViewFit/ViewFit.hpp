#pragma once

#include "Pine/Core/Math/Math.hpp"

namespace Pine
{
    // Where a camera has to sit, and what depth range it needs, for a world-space bounding box to
    // fill a perspective view without clipping.
    //
    // Built from bounds and projection parameters rather than from a camera or an entity, so
    // anything that wants to frame something is a valid caller: an asset thumbnail, focusing the
    // editor viewport on the selection, framing a whole level on load. It deliberately answers only
    // "how far away, and over what depth range" - the view direction stays the caller's, since that
    // is the part every caller wants to pick differently.
    //
    // Perspective only. An orthographic fit solves for a size rather than a distance, so it would
    // be a second constructor here rather than a flag on this one.
    struct ViewFit
    {
        // The point to aim at, and the natural pivot to orbit around.
        Vector3f Center = Vector3f(0.f);

        // Radius of the sphere bounding the box. Zero for degenerate bounds - the rest of the fit
        // is still usable, but a caller that wants to skip empty subjects can test this.
        float Radius = 0.f;

        // How far Center should be from the camera, along whichever direction the caller picks.
        float Distance = 0.f;

        float NearPlane = 0.f;
        float FarPlane = 0.f;

        // Fits the box's bounding sphere rather than its projected corners, on purpose: a sphere is
        // rotation invariant, so the framing holds still while the caller orbits around Center. A
        // corner-exact fit would make the subject breathe as it turned.
        //
        // fieldOfView is the vertical FOV in degrees, matching Camera. padding scales the distance,
        // where 1.0 makes the sphere touch the narrower pair of frustum planes exactly.
        static ViewFit FromBounds(const Vector3f& boundsMin, const Vector3f& boundsMax, float fieldOfView, float aspectRatio, float padding = 1.f);
    };
}
