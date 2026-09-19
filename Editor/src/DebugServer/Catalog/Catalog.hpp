#pragma once

#include "../DebugServer.hpp"

namespace Pine
{
    class Asset;
}

namespace Editor::DebugServer::Catalog
{
    // Resolves the ?path= / ?id= pair every asset endpoint accepts. Returns nullptr and fills
    // `error` with the reply to send, so the caller only has to forward it.
    Pine::Asset* Resolve(const Request& request, Response& error);

    Response List(const Request& request);
    Response Get(const Request& request);
    Response Summary(const Request& request);
}
