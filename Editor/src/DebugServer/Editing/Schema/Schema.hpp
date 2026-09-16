#pragma once

#include <cstddef>
#include <nlohmann/json.hpp>

namespace Editor::DebugServer::Editing::Schema
{
    nlohmann::json Request(std::size_t maxOperations);
    nlohmann::json Operations();
    nlohmann::json References();
}
