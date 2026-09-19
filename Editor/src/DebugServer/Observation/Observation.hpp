#pragma once

#include "../DebugServer.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

namespace Editor::DebugServer::Observation
{
    void OnRender(Pine::RenderingContext* context, Pine::RenderStage stage);

    // Identifies the frame that finished rendering most recently: server session, a monotonic frame
    // id, and the scene generation and debug mutation revision it was rendered at. Shared with the
    // capture routes so the "frame" object means the same thing wherever it appears.
    nlohmann::json FrameIdentity();
    Response Begin(const Request& request);
    Handler TrackMutation(Handler handler);
}
