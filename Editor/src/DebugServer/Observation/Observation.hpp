#pragma once

#include "../DebugServer.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

namespace Editor::DebugServer::Observation
{
    void OnRender(Pine::RenderingContext* context, Pine::RenderStage stage);
    Response Begin(const Request& request);
    Handler TrackMutation(Handler handler);
}
