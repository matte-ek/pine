#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace Editor::DebugServer
{
    enum class Method
    {
        Get,
        Post
    };

    // A request reduced to what endpoints actually need, rather than httplib's own type. httplib.h
    // is ~23k lines and both CMake targets glob every source file, so it is included in exactly one
    // translation unit and never reaches a header.
    struct Request
    {
        std::unordered_map<std::string, std::string> Parameters;

        std::string Body;
    };

    struct Response
    {
        int StatusCode = 200;

        nlohmann::json Body;

        // Set instead of Body for replies that are not JSON, such as the PNG endpoint. When this is
        // non-empty it is sent verbatim as ContentType and Body is ignored.
        std::vector<std::uint8_t> Binary;
        std::string ContentType = "application/json";
    };

    using Handler = std::function<Response(const Request&)>;

    void Setup();
    void Shutdown();

    // Registers an endpoint. The handler is always invoked on the main thread between frames, so it
    // may touch engine state freely - see the threading note at the top of DebugServer.cpp.
    //
    // Only valid during Endpoints::Register(); routes are handed to the HTTP library as they are
    // added, and the listener starts immediately afterwards.
    void AddRoute(Method method, const std::string& path, Handler handler);

    // The common "that request didn't work" reply, so every endpoint words it the same way.
    Response Error(int statusCode, const std::string& message);
}
