#include "ShaderSpecificationRegistry.hpp"

#include <algorithm>
#include <cassert>
#include <vector>

#include <fmt/format.h>

namespace
{
    struct ShaderSpecification
    {
        std::string Name;
        int Value;
    };

    std::vector<ShaderSpecification> m_Specifications;

    bool m_HasBeenRead = false;
}

void Pine::ShaderSpecificationRegistry::Register(const std::string& name, const int value)
{
    assert(!m_HasBeenRead && "Shader specifications must be registered before the first shader compiles.");

    const auto isAlreadyRegistered = std::any_of(m_Specifications.begin(), m_Specifications.end(),
        [&name](const ShaderSpecification& specification) { return specification.Name == name; });

    assert(!isAlreadyRegistered && "Shader specification registered twice.");

    m_Specifications.push_back({ name, value });
}

std::string Pine::ShaderSpecificationRegistry::GetDefines()
{
    m_HasBeenRead = true;

    std::string defines;

    for (const auto& specification : m_Specifications)
    {
        defines += fmt::format("#define {} {}\n", specification.Name, specification.Value);
    }

    return defines;
}
