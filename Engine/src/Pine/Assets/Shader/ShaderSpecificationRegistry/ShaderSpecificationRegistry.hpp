#pragma once

#include <string>

namespace Pine::ShaderSpecificationRegistry
{

    // Registers a constant that every shader is compiled with, as "#define <name> <value>". It is
    // meant for numbers shared between C++ and GLSL, such as array sizes and binding points, so each
    // one is written in a single place. A UBO array size bumped on one side only would otherwise
    // read out of bounds with nothing reporting it.
    //
    // Engine shaders compile as they load, so everything must be registered before the first shader
    // is loaded. Rendering::Internal::RegisterShaderSpecifications is where the engine does that.
    void Register(const std::string& name, int value);

    // The #define lines for every registered specification. Shader calls this each time it compiles,
    // and the first call closes the registry: a later Register() asserts, since the shaders compiled
    // so far would be missing its define.
    std::string GetDefines();

}
