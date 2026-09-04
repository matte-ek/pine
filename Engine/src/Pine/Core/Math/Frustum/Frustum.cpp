#include "Frustum.hpp"

namespace
{
    // Scales the plane so its normal is unit length. The tests below read the plane equation as a
    // signed distance, which is only true once it is normalized.
    Pine::Vector4f NormalizePlane(const Pine::Vector4f& plane)
    {
        const float length = glm::length(Pine::Vector3f(plane));

        if (length <= 0.f)
        {
            return plane;
        }

        return plane / length;
    }
}

Pine::Frustum Pine::Frustum::FromViewProjection(const Matrix4f& viewProjection)
{
    // glm is column-major, so m[column][row]. Row i of the matrix is therefore
    // (m[0][i], m[1][i], m[2][i], m[3][i]).
    const auto& m = viewProjection;

    const auto row = [&m](const int i)
    {
        return Vector4f(m[0][i], m[1][i], m[2][i], m[3][i]);
    };

    const Vector4f rowX = row(0);
    const Vector4f rowY = row(1);
    const Vector4f rowZ = row(2);
    const Vector4f rowW = row(3);

    Frustum frustum;

    frustum.Planes[Left]   = NormalizePlane(rowW + rowX);
    frustum.Planes[Right]  = NormalizePlane(rowW - rowX);
    frustum.Planes[Bottom] = NormalizePlane(rowW + rowY);
    frustum.Planes[Top]    = NormalizePlane(rowW - rowY);
    frustum.Planes[Near]   = NormalizePlane(rowW + rowZ);
    frustum.Planes[Far]    = NormalizePlane(rowW - rowZ);

    return frustum;
}

bool Pine::Frustum::Intersects(const Vector3f& boundsMin, const Vector3f& boundsMax) const
{
    const Vector3f center = (boundsMin + boundsMax) * 0.5f;
    const Vector3f extent = (boundsMax - boundsMin) * 0.5f;

    for (const auto& plane : Planes)
    {
        const auto normal = Vector3f(plane);

        // How far the box reaches along the plane normal. Using |normal| projects the extent onto
        // the normal regardless of which way the box faces, so no per-plane corner search is needed.
        const float projectedExtent = glm::dot(extent, glm::abs(normal));
        const float centerDistance = glm::dot(normal, center) + plane.w;

        // Entirely on the outside of this plane, so it cannot be inside the frustum.
        if (centerDistance + projectedExtent < 0.f)
        {
            return false;
        }
    }

    return true;
}

bool Pine::Frustum::Intersects(const Vector3f& center, const float radius) const
{
    for (const auto& plane : Planes)
    {
        if (glm::dot(Vector3f(plane), center) + plane.w + radius < 0.f)
        {
            return false;
        }
    }

    return true;
}
