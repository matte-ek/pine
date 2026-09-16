#include "DebugServer.hpp"

// The threading rule this whole file exists to enforce:
//
//   The HTTP thread never touches engine state.
//
// Every Pine subsystem is global mutable state in an anonymous namespace with no locking, and
// OpenGL is main-thread-only. httplib runs each handler on its own thread. So an HTTP worker only
// parses its request, hands a job to Requests, blocks, and serializes whatever comes back.
// Request status and cancellation only inspect synchronized queue state on the HTTP thread.
//
// Pine's own Threading system is not usable for this: PumpMainThreadTasks() is only ever called
// from inside AwaitTaskResult/AwaitTaskPool, and Engine::Run() never pumps, so a background thread
// queueing a MainThread task and awaiting it would wait forever. Hence Requests' separate queue,
// drained once per frame from a render callback.

#include <cstdlib>
#include <optional>
#include <thread>
#include <vector>

#include <httplib.h>

#include "Pine/Core/Log/Log.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

#include "Endpoints/Endpoints.hpp"
#include "Observation/Observation.hpp"
#include "Requests/Requests.hpp"

namespace
{
    // Loopback only, deliberately: this is an unauthenticated control channel into a running editor.
    constexpr const char* m_ListenAddress = "127.0.0.1";
    constexpr int m_DefaultPort = 9002;

    httplib::Server m_Server;
    std::thread m_ListenerThread;

    // PINE_DEBUG_SERVER both enables the server and picks its port, so there is one knob rather than
    // two: unset means off, "1" means on at the default port, "9100" means on at 9100.
    std::optional<int> ResolveListenPort()
    {
        const char* configuration = std::getenv("PINE_DEBUG_SERVER");

        if (configuration == nullptr)
        {
            return std::nullopt;
        }

        const int port = std::atoi(configuration);

        // Anything that isn't a usable port number - including the "1" that just means "on" - falls
        // back to the default rather than refusing to start.
        if (port < 1024 || port > 65535)
        {
            return m_DefaultPort;
        }

        return port;
    }

    void WriteResponse(httplib::Response& response, const Editor::DebugServer::Response& result)
    {
        response.status = result.StatusCode;

        if (!result.Binary.empty())
        {
            response.set_content(reinterpret_cast<const char*>(result.Binary.data()),
                                 result.Binary.size(),
                                 result.ContentType);

            return;
        }

        // Pretty-printed on purpose. The consumer here is a person or an agent reading curl output,
        // not a browser, and the bandwidth is a loopback socket.
        //
        // The replace error handler matters: entity names and asset paths come out of level files as
        // raw bytes, and dump() throws on invalid UTF-8 by default. That would throw here, on the
        // HTTP thread, outside the handler's own try/catch.
        response.set_content(result.Body.dump(2, ' ', false, nlohmann::json::error_handler_t::replace),
                             "application/json");
    }

    void ObserveRenderedFrame(Pine::RenderingContext* context, const Pine::RenderStage stage, float)
    {
        Editor::DebugServer::Observation::OnRender(context, stage);
        if (stage == Pine::RenderStage::PostRender)
        {
            Editor::DebugServer::Requests::ResumeReads();
        }
    }

    void OnPineRender(Pine::RenderingContext*, const Pine::RenderStage stage, float)
    {
        // PostRender fires once per frame with a null context, after every rendering context is
        // done - so engine state is settled and nothing is halfway through a pass.
        if (stage != Pine::RenderStage::PostRender)
        {
            return;
        }

        Editor::DebugServer::Requests::Drain();
    }

    void RegisterRoute(Editor::DebugServer::Method method, const std::string& path,
                       Editor::DebugServer::Handler handler, bool mutation)
    {
        auto dispatch = [path, handler, mutation](const httplib::Request& request, httplib::Response& response)
        {
            // Duplicate query/header values are ambiguous and must not alias another
            // payload in the retry registry. Identity comparison uses decoded query values.
            Editor::DebugServer::Request input;
            for (const auto& [name, value] : request.params)
            {
                if (!input.Parameters.emplace(name, value).second)
                {
                    WriteResponse(response, Editor::DebugServer::Error(400, "Duplicate query parameter."));
                    return;
                }
            }
            for (const auto* header : { "Idempotency-Key", "X-Pine-Session" })
            {
                if (request.has_header(header) &&
                    (request.get_header_value_count(header) != 1 || request.get_header_value(header).empty()))
                {
                    WriteResponse(response, Editor::DebugServer::Error(400, "Retry headers must have one nonempty value."));
                    return;
                }
            }
            input.Body = request.body;
            WriteResponse(response, Editor::DebugServer::Requests::Dispatch(
                path, handler, std::move(input), mutation,
                request.get_header_value("Idempotency-Key"), request.get_header_value("X-Pine-Session")));
        };

        if (method == Editor::DebugServer::Method::Get)
        {
            m_Server.Get(path, dispatch);
        }
        else
        {
            m_Server.Post(path, dispatch);
        }
    }

    void RegisterRequestRoutes()
    {
        // These handlers deliberately bypass the main-thread queue. Status and
        // cancellation must remain available while the editor is stalled.
        auto control = [](bool cancel, const httplib::Request& request, httplib::Response& response)
        {
            if (request.params.size() > 1 ||
                (!request.params.empty() && !request.has_param("id")) ||
                request.get_header_value_count("X-Pine-Session") > 1 ||
                request.has_header("Idempotency-Key") || !request.body.empty() ||
                (request.has_param("id") && request.get_param_value("id").empty()))
            {
                WriteResponse(response, Editor::DebugServer::Error(400,
                    "Expected only ?id= and X-Pine-Session, without a body or retry key."));
                return;
            }
            const auto id = request.get_param_value("id");
            const auto session = request.get_header_value("X-Pine-Session");
            WriteResponse(response, cancel
                ? Editor::DebugServer::Requests::Cancel(id, session)
                : Editor::DebugServer::Requests::Status(id, session));
        };
        m_Server.Get("/requests", [control](const auto& request, auto& response)
        {
            control(false, request, response);
        });
        m_Server.Post("/requests/cancel", [control](const auto& request, auto& response)
        {
            control(true, request, response);
        });
    }
}

void Editor::DebugServer::AddRoute(const Method method, const std::string& path, Handler handler)
{
    RegisterRoute(method, path, std::move(handler), false);
}

void Editor::DebugServer::AddMutationRoute(const std::string& path, Handler handler)
{
    RegisterRoute(Method::Post, path, Observation::TrackMutation(std::move(handler)), true);
}

Editor::DebugServer::Response Editor::DebugServer::Error(const int statusCode, const std::string& message)
{
    nlohmann::json body;

    body["error"] = message;

    return { statusCode, body };
}

void Editor::DebugServer::SetupRenderObservation()
{
    if (ResolveListenPort().has_value())
    {
        Pine::RenderManager::AddRenderCallback(ObserveRenderedFrame);
    }
}

void Editor::DebugServer::Setup()
{
    const auto port = ResolveListenPort();

    if (!port.has_value())
    {
        return;
    }

    Requests::Setup();
    RegisterRequestRoutes();

    Endpoints::Register();

    Pine::RenderManager::AddRenderCallback(OnPineRender);

    m_ListenerThread = std::thread([listenPort = *port]
    {
        if (!m_Server.listen(m_ListenAddress, listenPort))
        {
            PError(fmt::format("Debug server failed to listen on {}:{}.", m_ListenAddress, listenPort));
        }
    });

    // Safe against a failed bind: httplib marks itself decommissioned when it cannot take the
    // port, so this returns rather than spinning, and the editor still boots.
    m_Server.wait_until_ready();

    if (!m_Server.is_running())
    {
        // listen() has already logged why. Join the finished thread here rather than going through
        // Shutdown(), which would report a stop that never happened.
        m_ListenerThread.join();

        Requests::Shutdown();

        return;
    }

    PInfo(fmt::format("Debug server listening on http://{}:{}", m_ListenAddress, *port));
}

void Editor::DebugServer::Shutdown()
{
    if (!m_ListenerThread.joinable())
    {
        return;
    }

    // Release waiting workers before stop() joins them.
    Requests::Shutdown();

    m_Server.stop();

    m_ListenerThread.join();

    PInfo("Debug server stopped.");
}
