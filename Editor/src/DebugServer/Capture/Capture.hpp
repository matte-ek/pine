#pragma once

#include "../DebugServer.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

namespace Editor::DebugServer::Capture
{
    // Renders an asset on its own, with no scene and no viewport involved.
    Response Preview(const Request& request);

    // Renders the live scene from a caller-supplied pose, into the debug server's own rendering
    // context, without touching the editor camera or needing a viewport tab to be open.
    Response Scene(const Request& request);

    // Arms the next queued scene capture. Must be called once per frame at PreRender, before the
    // rendering contexts run.
    void OnRender(Pine::RenderingContext* context, Pine::RenderStage stage);

    void Shutdown();
}
