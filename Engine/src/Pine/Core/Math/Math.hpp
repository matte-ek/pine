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

    using Matrix4f = glm::mat4;

    using Quaternion = glm::quat;

    // Authored colors (light/material/ambient/fog, as picked in the editor) are sRGB, but lighting
    // is only correct in linear space. Convert once here, at the CPU->GPU boundary, so shaders can
    // assume everything they receive is already linear. Textures are handled separately by uploading
    // them with an sRGB internal format (see Texture usage hints / GLTexture). Uses the pow(2.2)
    // approximation; the inverse encode lives in the post-process resolve shader.
    inline Vector3f SrgbToLinear(const Vector3f& c) { return glm::pow(glm::max(c, Vector3f(0.f)), Vector3f(2.2f)); }
    inline Vector4f SrgbToLinear(const Vector4f& c) { return Vector4f(SrgbToLinear(Vector3f(c)), c.w); }
}