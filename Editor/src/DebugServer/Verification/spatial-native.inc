    try
    {
        using nlohmann::json;
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };
        const auto query = [&](Pine::Entity* entity)
        {
            Editor::DebugServer::Request request;
            request.Body = json{{"entities", {{{"id", entity->GetId().ToString()}}}}}.dump();
            return Editor::DebugServer::Spatial::Query(request);
        };

        auto terrain = new Pine::Terrain();
        terrain->SetPath("ground");
        Pine::Assets::Internal::RegisterAsset(terrain);
        terrain->SetChunkQuads(8);
        terrain->SetChunkSize(8.f);
        terrain->Resize({-1, 0}, {2, 1});
        terrain->SetSampleHeight({0, 4}, 12.f);
        auto ground = Pine::Entities::Create("Spatial ground");
        ground->GetTransform()->SetLocalPosition({10, 3, 20});
        ground->GetTransform()->SetLocalScale({2, 3, 4});
        ground->GetTransform()->SetLocalRotation(glm::angleAxis(glm::radians(90.f), Pine::Vector3f(0, 0, 1)));
        ground->AddComponent<Pine::TerrainRendererComponent>()->SetTerrain(terrain);

        // No rendered frame yet, and no usable viewport: reads must still work. The
        // rotation and scale above deliberately must not affect terrain's translated bounds.
        const auto context = Editor::RenderHandler::GetLevelRenderingContext();
        const auto active = context->Active;
        const auto size = context->Size;
        context->Active = false;
        context->Size = {0, 0};
        const auto result = query(ground);
        check(result.StatusCode == 200, result.Body.dump());
        const auto bounds = result.Body["entities"][0]["bounds"];
        check(bounds["min"]["x"] == 2 && bounds["max"]["x"] == 18 &&
              bounds["min"]["z"] == 20 && bounds["max"]["z"] == 28, "Terrain bounds used rotation or scale");
        check(std::abs(bounds["max"]["y"].get<double>() - 15) < 0.01, "Fresh sculpt height was not measured");
        context->Active = active;
        context->Size = size;

        auto cube = Pine::Entities::Create("Native bounds probe");
        auto model = cube->AddComponent<Pine::ModelRenderer>();
        model->SetModel(Pine::Assets::Get<Pine::Model>("engine/primitive/cube"));
        model->SetModelMeshIndex(0);
        cube->GetTransform()->SetLocalScale({-2, 0, 3});
        const auto flat = query(cube);
        check(flat.StatusCode == 200, flat.Body.dump());
        check(flat.Body["entities"][0]["bounds"]["dimensions"] == json{{"x", 4}, {"y", 0}, {"z", 6}},
              "Selected mesh or negative/zero scale bounds are wrong");
        model->SetModelMeshIndex(100000);
        check(query(cube).StatusCode == 400, "Invalid mesh index was accepted");
        model->SetModelMeshIndex(0);
        cube->GetTransform()->SetLocalPosition({std::numeric_limits<float>::infinity(), 0, 0});
        check(query(cube).StatusCode == 400, "Non-finite bounds were accepted");
        Pine::Entities::Delete(cube);
        std::cout << "PASS(native): fresh terrain, hidden viewport, selected mesh, negative/zero scale and invalid state." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }
    Pine::Engine::Run();
