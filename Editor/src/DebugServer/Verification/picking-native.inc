    try
    {
        using nlohmann::json;
        namespace Picking = Editor::DebugServer::Picking;
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };
        const json frame = {{"sceneGeneration", Pine::Entities::GetSceneGeneration()}, {"id", 1}};
        Pine::RenderingContext context;
        context.Visibility.Reset(4096);
        const auto viewProjection = glm::perspective(glm::radians(60.f), 1.f, .1f, 100.f)
            * glm::lookAt(Pine::Vector3f(0, 0, 5), Pine::Vector3f(0), Pine::Vector3f(0, 1, 0));
        const auto pick = [&](const json& metadata, int x, int y)
        {
            Editor::DebugServer::Request request;
            request.Body = json{{"capture", metadata.at("capture")}, {"pixel", {{"x", x}, {"y", y}}}}.dump();
            const auto result = Picking::Pick(request);
            check(result.StatusCode == 200, result.Body.dump());
            return result.Body.at("hit");
        };

        // Two triangles share a bounding box with a hole between them. One uses an
        // index buffer and the other does not. Picking the enclosing box would fail.
        auto model = new Pine::Model();
        model->SetPath("picking-native");
        Pine::Assets::Internal::RegisterAsset(model);
        float left[] = {-2, -1, 0, -0.5f, -1, 0, -1.25f, 1, 0};
        float right[] = {0.5f, -1, 0, 2, -1, 0, 1.25f, 1, 0};
        auto mesh = model->CreateMesh();
        mesh->SetVertices(left, sizeof(left));
        std::uint32_t indices[] = {0, 1, 2};
        mesh->SetIndices(indices, sizeof(indices));
        mesh = model->CreateMesh();
        mesh->SetVertices(right, sizeof(right));
        auto entity = Pine::Entities::Create("Picking triangles");
        auto renderer = entity->AddComponent<Pine::ModelRenderer>();
        renderer->SetModel(model);
        entity->GetTransform()->OnRender(0);
        context.Visibility.Set(renderer->GetInternalId());

        auto capture = Picking::Capture(context, viewProjection, 101, 101, frame);
        check(capture.StatusCode == 200, capture.Body.dump());
        check(pick(capture.Body, 50, 50).is_null(), "Triangle gap was picked as a bounding box");
        check(pick(capture.Body, 28, 50).at("meshIndex") == 0, "Indexed triangle was not identified");
        check(pick(capture.Body, 72, 50).at("meshIndex") == 1, "Non-indexed triangle was not identified");
        renderer->SetModelMeshIndex(1);
        capture = Picking::Capture(context, viewProjection, 101, 101, frame);
        check(pick(capture.Body, 28, 50).is_null(), "MeshIndex did not exclude mesh zero");
        check(pick(capture.Body, 72, 50).at("meshIndex") == 1, "Selected mesh identity changed");

        renderer->SetActive(false);
        capture = Picking::Capture(context, viewProjection, 101, 101, frame);
        check(pick(capture.Body, 72, 50).is_null(), "Inactive component was picked");
        renderer->SetActive(true);
        entity->SetTemporary(true);
        capture = Picking::Capture(context, viewProjection, 101, 101, frame);
        check(pick(capture.Body, 72, 50).is_null(), "Temporary editor entity was picked");
        entity->SetTemporary(false);

        // Slots beyond a half float's exact consecutive range still need exact IDs.
        // Repeated triangles lie outside the view; only the final mesh covers centre.
        renderer->SetModelMeshIndex(-1);
        for (int index = 2; index < 2051; ++index)
        {
            auto extra = model->CreateMesh();
            float vertices[] = {20, -1, 0, 22, -1, 0, 21, 1, 0};
            extra->SetVertices(vertices, sizeof(vertices));
        }
        auto last = model->CreateMesh();
        float centre[] = {-1, -1, 0, 1, -1, 0, 0, 1, 0};
        last->SetVertices(centre, sizeof(centre));
        capture = Picking::Capture(context, viewProjection, 101, 101, frame);
        check(pick(capture.Body, 50, 50).at("meshIndex") == 2051, "Large mesh identity lost precision");

        // Resource limits and singular cameras fail explicitly, before drawing.
        check(Picking::Capture(context, viewProjection, 4096, 4096, frame).StatusCode == 409,
            "Oversized picking capture was accepted");
        check(Picking::Capture(context, Pine::Matrix4f(0.f), 10, 10, frame).StatusCode == 409,
            "Singular camera was accepted");
        renderer->SetModelMeshIndex(0);
        const auto large = Picking::Capture(context, viewProjection, 2048, 1024, frame);
        check(large.StatusCode == 200, large.Body.dump());
        check(Picking::Capture(context, viewProjection, 2048, 1024, frame).StatusCode == 200,
            "Second large capture failed");
        Editor::DebugServer::Request evicted;
        evicted.Body = json{{"capture", large.Body.at("capture")}, {"pixel", {{"x", 0}, {"y", 0}}}}.dump();
        check(Picking::Pick(evicted).StatusCode == 409, "Memory budget did not evict an old capture");
        PlayHandler::Play();
        check(Picking::Capture(context, viewProjection, 101, 101, frame).StatusCode == 409,
            "Playing capture was accepted");
        PlayHandler::Pause();
        check(Picking::Capture(context, viewProjection, 101, 101, frame).StatusCode == 409,
            "Paused capture was accepted");
        PlayHandler::Stop();
        // Stop restores the scene from its snapshot, so reacquire its identities.
        for (const auto candidate : Pine::Entities::GetList())
        {
            if (candidate->GetName() == "Picking triangles")
            {
                Pine::Entities::Delete(candidate);
                break;
            }
        }
        Picking::Shutdown();
        // Compare whole PNGs across picking/non-picking frames. Animated film grain
        // would change those pixels even if the picking pass touched no GPU state.
        Pine::World::GetActiveLevel()->GetLevelSettings().GrainStrength = 0.f;
        std::cout << "PASS(native): triangle holes, indexed/non-indexed meshes, MeshIndex, activation, editor entities, large IDs, limits and play rejection." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }
    Pine::Engine::Run();
