    try
    {
        using nlohmann::json;
        namespace Queries = Editor::DebugServer::Spatial::Queries;
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };
        const auto vector = [](const Pine::Vector3f& value)
        {
            return json{{"x", value.x}, {"y", value.y}, {"z", value.z}};
        };
        const auto invoke = [&](auto handler, const json& body, int status = 200)
        {
            Editor::DebugServer::Request request;
            request.Body = body.dump();
            const auto result = handler(request);
            check(result.StatusCode == status, result.Body.dump());
            return result.Body;
        };
        const auto ray = [&](Pine::Vector3f origin, Pine::Vector3f direction)
        {
            return json{{"origin", vector(origin)}, {"direction", vector(direction)}, {"maxDistance", 100}};
        };

        const auto context = Editor::RenderHandler::GetLevelRenderingContext();
        const auto active = context->Active;
        const auto size = context->Size;
        context->Active = false;
        context->Size = {0, 0};

        auto ground = Pine::Entities::Find("Spatial ground");
        const auto terrain = ground->GetComponent<Pine::TerrainRendererComponent>()->GetTerrain();
        auto down = ray({10.25f, 40, 24.25f}, {0, -3, 0});
        auto hit = invoke(Queries::Raycast, down).at("hit");
        check(hit["entity"] == ground->GetId().ToString() && hit["geometry"] == "terrain-surface", "Terrain was not hit");
        const auto expectedHeight = *terrain->GetHeightAt(.25f, 4.25f) + 3;
        check(std::abs(hit["position"]["y"].get<float>() - expectedHeight) < .001f, "Terrain ray did not use live height field");
        const auto terrainNormal = Editor::DebugServer::Editing::Values::Vector3(hit["normal"]);
        check(std::abs(glm::length(terrainNormal) - 1.f) < .001f && terrainNormal.y > 0 &&
            std::abs(terrainNormal.x) > .1f, "Terrain normal is not the sloped face normal");
        terrain->SetSampleHeight({0, 4}, 20);
        hit = invoke(Queries::Raycast, down).at("hit");
        check(hit["position"]["y"].get<float>() > expectedHeight + 3, "Terrain ray was stale after sculpting");
        const auto below = invoke(Queries::Raycast, ray({10.25f, -40, 24.25f}, {0, 1, 0})).at("hit");
        check(below["normal"]["y"].get<float>() < 0, "Back-side terrain normal did not oppose ray");
        down["maxDistance"] = 1;
        check(invoke(Queries::Raycast, down)["hit"].is_null(), "Terrain ignored maximum distance");

        // Two side triangles enclose an opening. One is indexed, the other isn't.
        struct DoorwayModel : Pine::Model
        {
            DoorwayModel()
            {
                m_UId = Pine::UId::New();
                m_BoundingBoxMin = {-2, -1, 0};
                m_BoundingBoxMax = {2, 1, 0};
            }
        };
        auto model = new DoorwayModel();
        model->SetPath("spatial-triangles");
        Pine::Assets::Internal::RegisterAsset(model);
        float left[] = {-2, -1, 0, -.5f, -1, 0, -1.25f, 1, 0};
        float right[] = {.5f, -1, 0, 2, -1, 0, 1.25f, 1, 0};
        auto indexed = model->CreateMesh();
        indexed->SetVertices(left, sizeof(left));
        std::uint32_t indices[] = {0, 1, 2};
        indexed->SetIndices(indices, sizeof(indices));
        indexed->SetAABB({-2, -1, 0}, {-.5f, 1, 0});
        auto plain = model->CreateMesh();
        plain->SetVertices(right, sizeof(right));
        plain->SetAABB({.5f, -1, 0}, {2, 1, 0});
        auto parent = Pine::Entities::Create("Ray parent");
        auto entity = Pine::Entities::Create("Ray wall");
        parent->AddChild(entity);
        parent->GetTransform()->SetLocalPosition({-30, 10, 0});
        parent->GetTransform()->SetLocalRotation(glm::angleAxis(glm::radians(37.f), Pine::Vector3f(0, 1, 0)));
        entity->GetTransform()->SetLocalPosition({2, 3, 4});
        entity->GetTransform()->SetLocalScale({-2, 3, 1});
        const auto renderer = entity->AddComponent<Pine::ModelRenderer>();
        renderer->SetModel(model);
        const auto transform = entity->GetTransform();
        const auto point = [&](Pine::Vector3f local)
        {
            return transform->GetPosition() + transform->GetRotation() * (local * transform->GetScale());
        };
        const auto normal = transform->GetRotation() * Pine::Vector3f(0, 0, 1);
        auto leftRay = ray(point({-1.25f, 0, 0}) + normal * 5.f, -normal * 2.f);
        const auto rightRay = ray(point({1.25f, 0, 0}) + normal * 5.f, -normal);
        hit = invoke(Queries::Raycast, leftRay).at("hit");
        check(hit["entity"] == entity->GetId().ToString() && hit["meshIndex"] == 0, "Indexed parented triangle missed");
        check(std::abs(hit["distance"].get<float>() - 5) < .001f, "Ray direction was not normalized");
        check(glm::length(Editor::DebugServer::Editing::Values::Vector3(hit["normal"]) - normal) < .001f,
            "Mirrored rotated face normal is wrong");
        check(invoke(Queries::Raycast, rightRay)["hit"]["meshIndex"] == 1, "Non-indexed triangle missed");
        check(invoke(Queries::Raycast, ray(point({0, 0, 0}) + normal * 5.f, -normal))["hit"].is_null(),
            "Ray hit the doorway's enclosing box");
        const auto doorwayCenter = point({0, 0, 0});
        const auto opening = json{{"bounds", {{"min", vector(doorwayCenter - Pine::Vector3f(.1f))},
            {"max", vector(doorwayCenter + Pine::Vector3f(.1f))}}}};
        check(invoke(Queries::Overlap, opening)["total"] == 1, "Doorway bounds did not enclose the opening");
        renderer->SetModelMeshIndex(0);
        check(invoke(Queries::Raycast, rightRay)["hit"].is_null(), "MeshIndex was ignored");
        auto bounds = Editor::DebugServer::Spatial::Bounds{};
        Editor::DebugServer::Spatial::AddModelBounds(entity, bounds);
        json overlap = {{"bounds", {{"min", vector(bounds.Min)}, {"max", vector(bounds.Max)}}}};
        check(invoke(Queries::Overlap, overlap)["total"] == 1, "Rotated bounds did not overlap");

        renderer->SetActive(false);
        check(invoke(Queries::Raycast, leftRay)["hit"].is_null(), "Inactive component was hit");
        check(invoke(Queries::Overlap, overlap)["total"] == 0, "Inactive component overlapped");
        leftRay["includeInactive"] = true;
        check(!invoke(Queries::Raycast, leftRay)["hit"].is_null(), "Explicit inactive inclusion failed");
        leftRay.erase("includeInactive");
        renderer->SetActive(true);
        parent->SetTemporary(true);
        check(invoke(Queries::Raycast, leftRay)["hit"].is_null(), "Temporary descendant was hit");
        parent->SetTemporary(false);
        leftRay["exclude"] = {{{"id", parent->GetId().ToString()}}};
        check(invoke(Queries::Raycast, leftRay)["hit"].is_null(), "Hierarchy exclusion failed");
        leftRay.erase("exclude");

        // Readback follows GPU updates, does not depend on cached bounds, and leaves
        // VAO, vertex/index and copy-read bindings exactly as the caller left them.
        for (int i = 2; i < 9; i += 3) left[i] = 1;
        indexed->UpdateVertices(left, sizeof(left));
        plain->GetVertexArray()->Bind();
        const auto bindingState = []
        {
            std::array<GLint, 4> state{};
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state[0]);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state[1]);
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &state[2]);
            glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &state[3]);
            return state;
        };
        const auto bindings = bindingState();
        hit = invoke(Queries::Raycast, leftRay).at("hit");
        check(std::abs(hit["distance"].get<float>() - 4) < .001f, "Vertex update was not read back");
        check(bindings == bindingState(), "Readback changed graphics bindings");
        float readback = 0;
        check(!indexed->GetVertexArray()->ReadElementArrayBuffer(&readback, sizeof(float), sizeof(indices)),
            "Out-of-range index readback was accepted");
        check(!plain->GetVertexArray()->ReadElementArrayBuffer(&readback, sizeof(float)), "Missing index buffer was read");
        indices[2] = 999;
        indexed->SetIndices(indices, sizeof(indices));
        invoke(Queries::Raycast, leftRay, 400);
        indices[2] = 2;
        indexed->SetIndices(indices, sizeof(indices));
        entity->GetTransform()->SetLocalScale({0, 0, 0});
        check(invoke(Queries::Raycast, leftRay)["hit"].is_null(), "Degenerate triangle was hit");
        renderer->SetModelMeshIndex(10000);
        invoke(Queries::Raycast, leftRay, 400);
        renderer->SetModelMeshIndex(0);
        entity->GetTransform()->SetLocalScale({1, 1, 1});
        entity->GetTransform()->SetLocalPosition({std::numeric_limits<float>::infinity(), 0, 0});
        invoke(Queries::Raycast, leftRay, 400);
        Pine::Entities::Delete(parent);

        // Limits reject the whole query before an oversized readback/triangle scan.
        auto large = Pine::Entities::Create("Ray limits");
        auto largeRenderer = large->AddComponent<Pine::ModelRenderer>();
        largeRenderer->SetModel(model);
        largeRenderer->SetModelMeshIndex(0);
        {
            std::vector<std::uint32_t> manyIndices(3 * (2 * 1024 * 1024 + 1), 0);
            indexed->SetIndices(manyIndices.data(), manyIndices.size() * sizeof(std::uint32_t));
        }
        check(invoke(Queries::Raycast, down, 409)["error"].get<std::string>().find("triangle tests") != std::string::npos,
            "Triangle work limit was not reported");
        largeRenderer->SetModelMeshIndex(1);
        {
            std::vector<float> manyVertices(3 * (64 * 1024 * 1024 / sizeof(Pine::Vector3f) + 1), 0);
            plain->SetVertices(manyVertices.data(), manyVertices.size() * sizeof(float));
        }
        check(invoke(Queries::Raycast, down, 409)["error"].get<std::string>().find("64 MiB") != std::string::npos,
            "Readback memory limit was not reported");
        down["exclude"] = {{{"id", large->GetId().ToString()}}};
        invoke(Queries::Raycast, down);
        down.erase("exclude");
        Pine::Entities::Delete(large);

        PlayHandler::Play();
        invoke(Queries::Raycast, down, 409);
        invoke(Queries::Overlap, overlap, 409);
        PlayHandler::Pause();
        invoke(Queries::Raycast, down, 409);
        invoke(Queries::Overlap, overlap, 409);
        PlayHandler::Stop();
        context->Active = active;
        context->Size = size;
        std::cout << "PASS(intersections): terrain contact/normals, rotated parented meshes, holes, readback freshness/bindings, filters and play rejection." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }
    Pine::Engine::Run();
