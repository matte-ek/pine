#pragma once

#include "../DebugServer.hpp"

namespace Editor::DebugServer::Requests
{
    // Setup runs before the listener starts. The session is immutable thereafter and
    // shared with observation tokens; request records survive scene replacement.
    void Setup();
    void Shutdown();
    const std::string& GetSession();

    // HTTP workers only: these operations touch the synchronized request registry,
    // never engine state. Dispatch waits at most until the original request deadline.
    Response Dispatch(const std::string& path, Handler handler, Request request,
                      bool mutation, const std::string& identity, const std::string& session);
    Response Status(const std::string& identity, const std::string& session);
    Response Cancel(const std::string& identity, const std::string& session);

    // Main thread only. Deferred reads resume after rendering, before UI mutations;
    // new requests execute at the end of the frame.
    void Drain();
    void ResumeReads();
}
