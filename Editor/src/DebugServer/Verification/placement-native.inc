    try
    {
        using nlohmann::json;
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };
        const auto edit = [&](const json& operation, int expected = 200)
        {
            Editor::DebugServer::Request request;
            request.Body = json{{"version", 1}, {"operations", {operation}}}.dump();
            const auto result = Editor::DebugServer::Editing::Edit(request);
            check(result.StatusCode == expected, result.Body.dump());
            return result.Body;
        };

        struct PlacementModel : Pine::Model
        {
            PlacementModel()
            {
                m_UId = Pine::UId::New();
                m_BoundingBoxMin = {-10, -20, -30};
                m_BoundingBoxMax = {20, 30, 40};
            }
        };
        auto model = new PlacementModel();
        model->SetPath("placement-native");
        Pine::Assets::Internal::RegisterAsset(model);
        model->CreateMesh()->SetAABB({-10, -20, -30}, {0, 0, 0});
        model->CreateMesh()->SetAABB({2, 3, 4}, {20, 30, 40});
        auto parent = Pine::Entities::Create("Native placement parent");
        auto entity = Pine::Entities::Create("Native placement");
        parent->AddChild(entity);
        const auto renderer = entity->AddComponent<Pine::ModelRenderer>();
        renderer->SetModel(model);
        renderer->SetModelMeshIndex(1);
        const auto context = Editor::RenderHandler::GetLevelRenderingContext();
        const auto active = context->Active;
        context->Active = false;

        const json operation = {
            {"op", "entity.place"}, {"target", {{"id", entity->GetId().ToString()}}},
            {"surface", {{"point", {{"x", 0}, {"y", 0}, {"z", 0}}},
                         {"normal", {{"x", 0}, {"y", 1}, {"z", 0}}}}},
            {"anchor", {{"type", "modelBounds"}}}, {"clearance", .25}
        };
        edit(operation);
        check(glm::length(entity->GetTransform()->GetPosition() - Pine::Vector3f(-11, -2.75f, -22)) < .0001f,
            "Placement used whole-model bounds instead of the selected mesh, or required a viewport");
        const auto before = entity->GetTransform()->GetLocalPosition();
        const auto history = Editor::DebugServer::Editing::History::Get({}).Body;
        parent->GetTransform()->SetLocalRotation(Pine::Quaternion(2, 0, 0, 0));
        edit(operation, 400);
        check(entity->GetTransform()->GetLocalPosition() == before, "Invalid ancestor caused partial placement");
        parent->GetTransform()->SetLocalRotation(Pine::Quaternion(1, 0, 0, 0));
        parent->GetTransform()->SetLocalPosition({std::numeric_limits<float>::infinity(), 0, 0});
        edit(operation, 400);
        parent->GetTransform()->SetLocalPosition({0, 0, 0});
        renderer->SetModelMeshIndex(10000);
        edit(operation, 400);
        renderer->SetModelMeshIndex(1);
        check(Editor::DebugServer::Editing::History::Get({}).Body == history,
            "Rejected placements changed history");

        auto reference = Pine::Entities::Create("Native placement reference");
        reference->GetTransform()->SetLocalPosition({50, 60, 70});
        const auto referenceRenderer = reference->AddComponent<Pine::ModelRenderer>();
        referenceRenderer->SetModel(model);
        referenceRenderer->SetModelMeshIndex(0);
        reference->SetActive(false);
        json relative = {
            {"op", "entity.place"}, {"target", {{"id", entity->GetId().ToString()}}},
            {"relativeTo", {{"id", reference->GetId().ToString()}}},
            {"boundsAlignment", {
                {"x", {{"target", "min"}, {"reference", "max"}}},
                {"y", {{"target", "min"}, {"reference", "min"}}},
                {"z", {{"target", "center"}, {"reference", "center"}}}
            }},
            {"offset", {{"space", "world"}, {"value", {{"x", .25}, {"y", 0}, {"z", 0}}}}}
        };
        edit(relative);
        check(glm::length(entity->GetTransform()->GetPosition() - Pine::Vector3f(48.25f, 37, 33)) < .0001f,
            "Relative placement ignored selected mesh bounds or inactive reference geometry");

        // Native entities may have repeated renderers. Include each selected mesh even when inactive.
        const auto extraRenderer = entity->AddComponent<Pine::ModelRenderer>();
        extraRenderer->SetModel(model);
        extraRenderer->SetModelMeshIndex(0);
        extraRenderer->SetActive(false);
        edit(relative);
        check(glm::length(entity->GetTransform()->GetPosition() - Pine::Vector3f(60.25f, 60, 50)) < .0001f,
            "Relative placement did not combine renderer boxes");
        const auto relativeHistory = Editor::DebugServer::Editing::History::Get({}).Body;
        reference->GetTransform()->SetLocalRotation(Pine::Quaternion(2, 0, 0, 0));
        edit(relative, 400);
        reference->GetTransform()->SetLocalRotation(Pine::Quaternion(1, 0, 0, 0));
        referenceRenderer->SetModelMeshIndex(10000);
        edit(relative, 400);
        referenceRenderer->SetModelMeshIndex(0);
        check(Editor::DebugServer::Editing::History::Get({}).Body == relativeHistory,
            "Invalid reference state changed history");
        // Aiming requires only a Transform and works without a visible viewport.
        const json aim = {
            {"op", "entity.aim"}, {"target", {{"id", entity->GetId().ToString()}}},
            {"point", {{"x", 20}, {"y", 30}, {"z", -40}}},
            {"forwardAxis", "-Z"}, {"upAxis", "+Y"}, {"up", {{"x", 0}, {"y", 1}, {"z", 0}}}
        };
        const auto aimedPosition = entity->GetTransform()->GetPosition();
        edit(aim);
        const auto expectedDirection = glm::normalize(Pine::Vector3f(20, 30, -40) - aimedPosition);
        check(glm::length(entity->GetTransform()->GetForward() - expectedDirection) < .0001f,
            "Aim direction is wrong with hidden viewport");
        check(entity->GetTransform()->GetPosition() == aimedPosition, "Aim moved entity");
        const auto aimedRotation = entity->GetTransform()->GetLocalRotation();
        const auto aimHistory = Editor::DebugServer::Editing::History::Get({}).Body;
        parent->GetTransform()->SetLocalRotation(Pine::Quaternion(2, 0, 0, 0));
        edit(aim, 400);
        parent->GetTransform()->SetLocalRotation(Pine::Quaternion(1, 0, 0, 0));
        parent->GetTransform()->SetLocalPosition({std::numeric_limits<float>::infinity(), 0, 0});
        edit(aim, 400);
        parent->GetTransform()->SetLocalPosition({0, 0, 0});
        check(entity->GetTransform()->GetLocalRotation() == aimedRotation,
            "Invalid ancestor caused partial aiming");
        check(Editor::DebugServer::Editing::History::Get({}).Body == aimHistory,
            "Rejected aim changed history");
        context->Active = active;
        Pine::Entities::Delete(parent);
        Pine::Entities::Delete(reference);
        Editor::Actions::ClearHistory();

        // The stopped-mode gate applies before resolution, even while the world is paused.
        PlayHandler::Play();
        edit(operation, 409);
        edit(relative, 409);
        edit(aim, 409);
        PlayHandler::Pause();
        edit(operation, 409);
        edit(relative, 409);
        edit(aim, 409);
        PlayHandler::Stop();
        std::cout << "PASS(placement): surface/relative placement and aiming, selected meshes, repeated/inactive renderers, hidden viewport, invalid transforms/meshes, unchanged history and play/pause rejection." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }
    Pine::Engine::Run();
