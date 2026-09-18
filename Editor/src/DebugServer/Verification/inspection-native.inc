    try
    {
        using nlohmann::json;
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };
        const auto query = [&](const json& body, int expected = 200)
        {
            Editor::DebugServer::Request request;
            request.Body = body.dump();
            const auto response = Editor::DebugServer::Inspection::Query(request);
            check(response.StatusCode == expected, response.Body.dump());
            check(response.Binary.size() <= 1024 * 1024, "Response exceeded byte cap");
            return response.Binary.empty() ? response.Body : json::parse(response.Binary);
        };

        // Before the first rendered frame: unsupported adapters, activation and
        // protected descendants are not expressible through the edit API.
        auto root = Pine::Entities::Create("Native inspection root");
        root->SetActive(false);
        auto child = root->CreateChild();
        child->SetName("Native inspection child");
        child->AddComponent<Pine::NativeScript>();
        auto light = child->AddComponent<Pine::Light>();
        light->SetActive(false);
        json filter = {{"hierarchy", {{"root", {{"id", root->GetId().ToString()}}}}}};
        auto state = query({{"filter", filter}, {"include", {{"properties", true}, {"worldTransform", true}}}});
        check(state["total"] == 1, "Default inspection excluded an inactive component");
        bool unsupported = false;
        for (const auto& component : state["entities"][0]["components"])
        {
            if (component["type"] == "NativeScript") unsupported = component["properties"].is_null();
        }
        check(unsupported, "Unsupported component did not retain null properties");
        filter["component"] = "NativeScript";
        check(query({{"filter", filter}})["total"] == 1, "Unsupported adapter prevented membership filtering");
        filter["component"] = "Light";
        filter["includeInactive"] = false;
        check(query({{"filter", filter}})["total"] == 0, "Inactive light matched active filter");
        light->SetActive(true);
        check(query({{"filter", filter}})["total"] == 1, "Inactive parent implicitly disabled its child");

        root->SetTemporary(true);
        check(query({{"filter", {{"name", {{"value", "Native inspection child"}}}}}})["total"] == 0,
            "Temporary descendant leaked into query");
        query({{"entities", {{{"id", child->GetId().ToString()}}}}}, 400);
        root->SetTemporary(false);
        Pine::Entities::Delete(root);

        // Byte limits must account for JSON escaping and UTF-8 replacement, and
        // explicit batches must never silently return only a prefix.
        auto first = Pine::Entities::Create(std::string(300000, '\n'));
        auto second = Pine::Entities::Create(std::string(300000, '\n'));
        json largeFilter = {{"name", {{"value", "\n"}, {"match", "contains"}}}};
        state = query({{"filter", largeFilter}});
        check(state["total"] == 2 && state["truncated"] == true && state["entities"].size() == 1,
            "Byte limit did not truncate at an entity boundary");
        query({{"entities", {{{"id", first->GetId().ToString()}}, {{"id", second->GetId().ToString()}}}}}, 413);
        first->SetName(std::string(600000, '\n'));
        query({{"entities", {{{"id", first->GetId().ToString()}}}}}, 413);
        query({{"filter", largeFilter}}, 413);
        first->SetName(std::string("invalid\xff", 8));
        state = query({{"entities", {{{"id", first->GetId().ToString()}}}}});
        check(state["entities"][0]["name"].get<std::string>() == "invalid\xef\xbf\xbd", "Invalid UTF-8 was not replaced");
        Pine::Entities::Delete(first);
        Pine::Entities::Delete(second);

        auto terrain = new Pine::Terrain();
        terrain->SetPath("inspection-ground");
        Pine::Assets::Internal::RegisterAsset(terrain);
        terrain->SetChunkQuads(8);
        terrain->SetChunkSize(8.f);
        terrain->Resize({0, 0}, {1, 1});
        terrain->SetSampleHeight({4, 4}, 12.f);
        auto ground = Pine::Entities::Create("Native inspection terrain");
        ground->GetTransform()->SetLocalPosition({10, 3, 20});
        ground->GetTransform()->SetLocalScale({2, 3, 4});
        ground->GetTransform()->SetLocalRotation(glm::angleAxis(glm::radians(90.f), Pine::Vector3f(0, 0, 1)));
        auto renderer = ground->AddComponent<Pine::TerrainRendererComponent>();
        renderer->SetTerrain(terrain);
        // Terrain stores quantized heights; allow its sub-centimeter encoding error.
        json spatial = {{"test", "bounds"}, {"radius", {{"center", {{"x", 14}, {"y", 15}, {"z", 24}}}, {"distance", 0.01}}}};
        filter = {{"component", "TerrainRenderer"}, {"spatial", spatial}};
        check(query({{"filter", filter}})["total"] == 1, "Fresh terrain bounds or translation semantics changed");
        renderer->SetActive(false);
        filter["includeInactive"] = false;
        check(query({{"filter", filter}})["total"] == 0, "Inactive terrain matched active-only bounds");
        ground->GetTransform()->SetLocalPosition({std::numeric_limits<float>::infinity(), 0, 0});
        filter["includeInactive"] = true;
        query({{"filter", filter}}, 400);
        Pine::Entities::Delete(ground);

        auto playing = Pine::Entities::Create("Inspection play probe");
        const auto id = playing->GetId().ToString();
        const auto generation = Pine::Entities::GetSceneGeneration();
        const json batch = {{"entities", {{{"id", id}}}}, {"sceneGeneration", generation}};
        PlayHandler::Play();
        query(batch);
        PlayHandler::Pause();
        query(batch);
        PlayHandler::Stop();
        query(batch, 409);
        auto restored = Pine::Entities::Find(std::string("Inspection play probe"));
        check(restored != nullptr, "Play probe did not restore");
        Pine::Entities::Delete(restored);
        std::cout << "PASS(native): inspection byte limits, activation, protected hierarchies, terrain, unsupported types and play lifecycle." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }
    Pine::Engine::Run();
