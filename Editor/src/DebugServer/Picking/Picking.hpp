#pragma once

#include "../DebugServer.hpp"
#include "Pine/Rendering/RenderingContext.hpp"

namespace Editor::DebugServer::Picking
{
    // Called only at the observation's post-render boundary, before UI/queued writes.
    Response Capture(const Pine::RenderingContext& context, const Pine::Matrix4f& viewProjection,
        int width, int height, const nlohmann::json& frame);
    Response Pick(const Request& request);
    void Shutdown();
}
