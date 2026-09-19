#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Pine
{
    namespace Math
    {

        template<typename T>
        T LinearInterpolation(T begin, T end, T x)
        {
            return begin + (end - begin) * x;
        }

    }

    using Vector2i = glm::ivec2;
    using Vector3i = glm::ivec3;
    using Vector4i = glm::ivec4;

    using Vector2f = glm::vec2;
    using Vector3f = glm::vec3;
    using Vector4f = glm::vec4;

    using Vector2d = glm::dvec2;
    using Vector3d = glm::dvec3;
    using Vector4d = glm::dvec4;

    using Matrix3f = glm::mat3;
    using Matrix4f = glm::mat4;

    using Quaternion = glm::quat;

    // Authored colors (light/material/ambient/fog, as picked in the editor) are sRGB, but lighting
    // is only correct in linear space. Convert once here, at the CPU->GPU boundary, so shaders can
    // assume everything they receive is already linear. Textures are handled separately by uploading
    // them with an sRGB internal format (see Texture usage hints / GLTexture). Uses the pow(2.2)
    // approximation; the inverse encode lives in the post-process resolve shader.
    inline Vector3f SrgbToLinear(const Vector3f& c) { return glm::pow(glm::max(c, Vector3f(0.f)), Vector3f(2.2f)); }
    inline Vector4f SrgbToLinear(const Vector4f& c) { return Vector4f(SrgbToLinear(Vector3f(c)), c.w); }

    namespace Math
    {
        // The axis-aligned bounds of a local box after an affine transform, without visiting its eight
        // corners.
        //
        // Writing the box as a centre plus half-extents, a transformed corner is
        // 'linear * centre + translation' plus a sum of the linear part's columns scaled by +/- each
        // extent. That sum is largest when every term takes its own sign, so the transformed extents
        // are abs(linear) * extents. The result is exactly the box the corner loop produces - it is the
        // extents that get the absolute value, not the corners, so this is *not* the "rotate the
        // min/max pair" shortcut, which really is wrong for anything off-axis.
        //
        // Affine only: the linear part is applied without a perspective divide, so a projection matrix
        // is as meaningless here as it is to the corner loop.
        inline void TransformBounds(const Matrix3f& linear, const Vector3f& translation,
                                    const Vector3f& localMin, const Vector3f& localMax,
                                    Vector3f& worldMin, Vector3f& worldMax)
        {
            const auto center = (localMin + localMax) * 0.5f;
            const auto extents = (localMax - localMin) * 0.5f;

            const auto worldCenter = linear * center + translation;

            const auto absLinear = Matrix3f(glm::abs(linear[0]), glm::abs(linear[1]), glm::abs(linear[2]));
            const auto worldExtents = absLinear * extents;

            worldMin = worldCenter - worldExtents;
            worldMax = worldCenter + worldExtents;
        }
    }
}