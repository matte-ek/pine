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

        Pine::Entities::Delete(cube);
        Pine::Entities::Delete(east);

        std::cout << "PASS: light slots follow a moved object, a lamp carried by its parent and a deleted lamp.\n";
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }
