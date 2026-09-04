#include "ViewFit.hpp"

Pine::ViewFit Pine::ViewFit::FromBounds(const Vector3f& boundsMin, const Vector3f& boundsMax, const float fieldOfView, const float aspectRatio, const float padding)
{
    ViewFit fit;

    fit.Center = (boundsMin + boundsMax) * 0.5f;
    fit.Radius = glm::length(boundsMax - boundsMin) * 0.5f;

    // Degenerate bounds are a real input (an empty mesh, a single point), and everything below
    // divides by the radius somewhere. Solve for an arbitrarily tiny sphere instead of returning
    // infinities; Radius itself stays truthful so the caller can still tell.
    const float radius = glm::max(fit.Radius, 0.0001f);

    // The projection is built from the vertical FOV, but the horizontal one is narrower whenever
    // the target is taller than it is wide - fit against whichever would clip first.
    const float verticalFov = glm::radians(glm::clamp(fieldOfView, 1.f, 179.f));
    const float horizontalFov = 2.f * glm::atan(glm::tan(verticalFov * 0.5f) * glm::max(aspectRatio, 0.0001f));

    const float fieldOfViewToFit = glm::min(verticalFov, horizontalFov);

    fit.Distance = (radius / glm::sin(fieldOfViewToFit * 0.5f)) * glm::max(padding, 0.01f);

    // Scaling the planes with the subject is the half of this that keeps very large and very small
    // subjects visible at all: a fixed near/far either clips the subject away entirely or spends
    // the whole depth range on empty space in front of it.
    fit.NearPlane = glm::max(radius * 0.01f, fit.Distance - radius * 1.5f);
    fit.FarPlane = fit.Distance + radius * 2.f;

    return fit;
}
