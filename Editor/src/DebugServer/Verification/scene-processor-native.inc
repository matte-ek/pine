    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        const auto createLamp = [](const std::string& name, const Pine::Vector3f& position)
        {
            auto entity = Pine::Entities::Create(name);
            entity->GetTransform()->SetLocalPosition(position);

            auto light = entity->AddComponent<Pine::Light>();
            light->SetLightType(Pine::LightType::PointLight);

            return entity;
        };

        // Two lamps a hundred units apart, and a cube lit by whichever is nearer.
        auto west = createLamp("West lamp", {-50, 2, 0});
        auto east = createLamp("East lamp", {50, 2, 0});
        const auto westLight = west->GetComponent<Pine::Light>();
        const auto eastLight = east->GetComponent<Pine::Light>();

        auto cube = Pine::Entities::Create("Lit cube");
        cube->GetTransform()->SetLocalPosition({-45, 0, 0});
        auto renderer = cube->AddComponent<Pine::ModelRenderer>();
        renderer->SetModel(Pine::Assets::Get<Pine::Model>("engine/primitive/cube"));

        const auto nearestLight = [&]()
        {
            auto& slots = renderer->GetRenderingHintData().Lights;

            return slots.Index[Pine::Renderer3D::Specifications::ObjectLightSlots::POINT_LIGHT_OFFSET].Get();
        };

        // Every frame runs in the editor's mode, where the bug this guards against lived: movement
        // that no editor edit reported was never seen by the light slots.
        Pine::RenderManager::Run();
        check(nearestLight() == westLight, "The cube was not lit by the nearer lamp");

        // Moved the way a script or a physics body moves it, without flagging the entity.
        cube->GetTransform()->SetLocalPosition({45, 0, 0});
        Pine::RenderManager::Run();
        check(nearestLight() == eastLight, "The cube kept the light slots from where it started after moving");

        // A lamp carried by a moving parent, the way a player carries a flashlight. The cube stays
        // put and the west lamp arrives next to it.
        auto carrier = Pine::Entities::Create("Lamp carrier");
        carrier->AddChild(west);
        Pine::RenderManager::Run();
        check(nearestLight() == eastLight, "Parenting the lamp in place changed which lamp is nearest");

        carrier->GetTransform()->SetLocalPosition({97, 0, 0});
        Pine::RenderManager::Run();
        check(nearestLight() == westLight, "Moving a lamp's parent did not refresh the slots it competes for");

        // Removing a lamp hands its slot to the next nearest one.
        Pine::Entities::Delete(carrier);
        Pine::RenderManager::Run();
        check(nearestLight() == eastLight, "The cube kept a deleted lamp");

        // World bounds are kept while the transform and the model's bounds are unchanged, and
        // rebuilt when either changes, including through a parent.
        const auto& hintData = renderer->GetRenderingHintData();
        const auto boundsCentre = [&]()
        {
            return (hintData.BoundsMin + hintData.BoundsMax) * 0.5f;
        };
        const auto near = [](const Pine::Vector3f& a, const Pine::Vector3f& b)
        {
            return glm::length(a - b) < 1e-4f;
        };
        const auto hasMoved = [&]()
        {
            return hintData.BoundsMin != hintData.PreviousBoundsMin || hintData.BoundsMax != hintData.PreviousBoundsMax;
        };

        check(near(boundsCentre(), {45, 0, 0}), "The cube's bounds are not where the cube is");
        Pine::RenderManager::Run();
        check(!hasMoved(), "A cube that did not move was reported as moved");

        auto stand = Pine::Entities::Create("Cube stand");
        stand->AddChild(cube);
        stand->GetTransform()->SetLocalPosition({0, 10, 0});
        Pine::RenderManager::Run();
        check(near(boundsCentre(), {45, 10, 0}), "The cube's bounds did not follow its moved parent");
        check(hasMoved(), "A cube carried by its parent was not reported as moved");

        // A model with no meshes has empty bounds at its origin, so swapping it in has to rebuild
        // the bounds with the transform unchanged.
        auto emptyModel = new Pine::Model();
        emptyModel->SetPath("scene-processor-empty");
        Pine::Assets::Internal::RegisterAsset(emptyModel);
        renderer->SetModel(emptyModel);
        Pine::RenderManager::Run();
        check(hintData.BoundsMin == hintData.BoundsMax && near(hintData.BoundsMin, {45, 10, 0}),
            "Swapping the model did not rebuild the bounds");

        Pine::Entities::Delete(stand);
        Pine::Entities::Delete(east);

        std::cout << "PASS: light slots follow a moved object, a lamp carried by its parent and a deleted lamp;\n"
                     "      world bounds are kept while nothing changes and follow a moved parent and a swapped model.\n";
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }
