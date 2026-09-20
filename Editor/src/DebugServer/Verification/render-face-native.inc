    // MaterialRenderFace end to end: Both reaching the rasterizer, and the shader lighting the
    // face it keeps.
    //
    // The camera sits inside a large cube, so every wall it looks at presents its back face - the
    // one situation where culling is the only thing deciding whether geometry exists. A second,
    // small cube in front of the camera is the control: an ordinary front face, lit by the same
    // light, read at the same moment.
    //
    // None of this is reachable over HTTP - nothing creates a material there, and /edit cannot set
    // a render face - so it is built here and read off the rendered frame.
    try
    {
        const auto check = [](const bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        // Written before the field existed, so it is also the back-compatibility case: a '.passet'
        // with no RenderFace in it has to leave the default alone rather than read a zero out of
        // the missing field's place.
        auto* storedMaterial = Pine::Assets::Get<Pine::Material>("engine/materials/default");

        check(storedMaterial != nullptr, "Missing engine/materials/default");
        check(storedMaterial->GetRenderFace() == Pine::MaterialRenderFace::Default,
            "A material saved before RenderFace existed did not load as Default");

        // And the field survives a save and the load of what that save wrote.
        storedMaterial->SetRenderFace(Pine::MaterialRenderFace::Both);
        storedMaterial->ReLoad();

        check(storedMaterial->GetRenderFace() == Pine::MaterialRenderFace::Both,
            "RenderFace did not survive a save and reload");

        storedMaterial->SetRenderFace(Pine::MaterialRenderFace::Default);
        storedMaterial->ReLoad();

        auto* cube = Pine::Assets::Get<Pine::Model>("engine/primitive/cube");

        check(cube != nullptr, "Missing engine/primitive/cube");

        // No ambient at all: the whole question about the far side of a kept face is whether it
        // receives diffuse light, and an ambient term would answer it wrongly either way.
        const auto createMaterial = [&](const std::string& path)
        {
            auto* material = dynamic_cast<Pine::Material*>(
                Pine::Assets::CreateAsset(Pine::AssetType::Material, path));

            check(material != nullptr, "Could not create the material " + path);

            material->SetDiffuseColor(Pine::Vector3f(1.f, 0.f, 0.f));
            material->SetAmbientColor(Pine::Vector3f(0.f, 0.f, 0.f));

            return material;
        };

        auto* roomMaterial = createMaterial("engine/verify-render-face-room");
        auto* referenceMaterial = createMaterial("engine/verify-render-face-reference");

        const auto spawn = [&](const std::string& name,
                               const Pine::Vector3f& position,
                               const float scale,
                               Pine::Material* material)
        {
            auto* entity = Pine::Entities::Create(name);

            entity->GetTransform()->SetLocalPosition(position);
            entity->GetTransform()->SetLocalScale(Pine::Vector3f(scale));

            auto* renderer = entity->AddComponent<Pine::ModelRenderer>();

            renderer->SetModel(cube);
            renderer->SetOverrideMaterial(material);

            return entity;
        };

        // Placed around the editor camera on the first rendered frame, once there is a camera to
        // read - see the callback. The walls have to be close to it for the same reason the light
        // does: this reads absolute brightness, and a point light falls off with 1/d^2.
        auto* room = spawn("Room", Pine::Vector3f(0.f), 12.f, roomMaterial);
        auto* reference = spawn("Reference", Pine::Vector3f(0.f), 1.f, referenceMaterial);

        check(room != nullptr && reference != nullptr, "Could not spawn the probe entities");

        auto* lightEntity = Pine::Entities::Create("Probe light");

        {
            auto* light = lightEntity->AddComponent<Pine::Light>();

            light->SetLightType(Pine::LightType::PointLight);
            light->SetLightIntensity(40.f);
            light->SetRange(100.f);
        }

        static struct
        {
            Pine::Material* RoomMaterial = nullptr;

            Pine::Entity* Room = nullptr;
            Pine::Entity* Reference = nullptr;
            Pine::Entity* Light = nullptr;

            int Phase = 0;
            int Frame = 0;
            int NextPhaseFrame = 12;

            Pine::Vector3i CulledWall = Pine::Vector3i(0);
            Pine::Vector3i CulledReference = Pine::Vector3i(0);
            Pine::Vector3i KeptWall = Pine::Vector3i(0);
            Pine::Vector3i KeptReference = Pine::Vector3i(0);
        } probe;

        probe.RoomMaterial = roomMaterial;
        probe.Room = room;
        probe.Reference = reference;
        probe.Light = lightEntity;

        Pine::RenderManager::AddRenderCallback([](Pine::RenderingContext*, const Pine::RenderStage stage, float)
        {
            // PostRender runs once per frame, after every context, and is handed a null context -
            // so the one being read is fetched rather than compared against.
            if (stage != Pine::RenderStage::PostRender)
            {
                return;
            }

            auto* context = Editor::RenderHandler::GetLevelRenderingContext();

            if (context == nullptr || context->FrameBuffer == nullptr || context->Size.x < 1.f)
            {
                return;
            }

            probe.Frame++;

            // Put the scene around the camera the saved layout gave this viewport, rather than
            // guessing where that is: the room has to contain it, the reference has to be in front
            // of it, and the light has to be near both of them.
            if (probe.Frame == 1)
            {
                const auto* cameraTransform = context->SceneCamera->GetParent()->GetTransform();
                const auto cameraPosition = cameraTransform->GetPosition();

                probe.Room->GetTransform()->SetLocalPosition(cameraPosition);
                probe.Light->GetTransform()->SetLocalPosition(cameraPosition);
                probe.Reference->GetTransform()->SetLocalPosition(
                    cameraPosition + cameraTransform->GetForward() * 3.f);

                std::cout << "camera at " << cameraPosition.x << "," << cameraPosition.y
                          << "," << cameraPosition.z << std::endl;
            }

            // Several frames between phases: a material changed now reaches the batch on the next
            // frame, and the frame after that is the first one that shows it.
            if (probe.Frame < probe.NextPhaseFrame)
            {
                return;
            }

            probe.NextPhaseFrame = probe.Frame + 12;

            const auto width = static_cast<int>(context->Size.x);
            const auto height = static_cast<int>(context->Size.y);

            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);

            context->FrameBuffer->Bind();
            context->FrameBuffer->ReadPixels(Pine::Vector2i(0), Pine::Vector2i(width, height),
                Pine::Graphics::ReadFormat::RGBA, Pine::Graphics::TextureDataFormat::UnsignedByte,
                pixels.size(), pixels.data());
            Pine::Graphics::GetGraphicsAPI()->BindFrameBuffer(nullptr);

            const auto read = [&](const int x, const int y)
            {
                const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4;

                return Pine::Vector3i(pixels[offset], pixels[offset + 1], pixels[offset + 2]);
            };

            // The centre is the reference cube; a corner sees past it to the room's far wall.
            const auto reference = read(width / 2, height / 2);
            const auto wall = read(width / 6, height / 6);

            std::cout << "phase " << probe.Phase
                      << ": wall " << wall.r << "," << wall.g << "," << wall.b
                      << "  reference " << reference.r << "," << reference.g << "," << reference.b
                      << "  viewport " << width << "x" << height
                      << "  visible " << context->Statistics.VisibleObjectCount
                      << "  draws " << context->Statistics.DrawCalls << std::endl;

            if (probe.Phase == 0)
            {
                probe.CulledWall = wall;
                probe.CulledReference = reference;

                probe.RoomMaterial->SetRenderFace(Pine::MaterialRenderFace::Both);

                probe.Phase = 1;

                return;
            }

            probe.KeptWall = wall;
            probe.KeptReference = reference;

            try
            {
                const auto check = [](const bool condition, const std::string& message)
                {
                    if (!condition) throw std::runtime_error(message);
                };

                check(probe.CulledReference.r > 60 && probe.CulledReference.g < 40,
                    "The reference cube's front face should read lit red - the probe scene is wrong");

                check(probe.CulledWall.r < 30,
                    "With RenderFace::Default the camera should see straight through the room's walls");

                check(probe.KeptWall.r > 60 && probe.KeptWall.g < 40,
                    "With RenderFace::Both the room's inside wall should be drawn and lit red");

                // The wall is further from the light than the reference is, so this is not an
                // equality - but an unflipped normal leaves it with no diffuse term at all, and
                // with no ambient to fall back on that is the difference between this and black.
                check(probe.KeptWall.r > probe.CulledWall.r + 40,
                    "The kept face is barely brighter than the background - it is drawn but unlit");

                check(probe.KeptReference.r > 60,
                    "Turning a material two-sided should not change how an ordinary front face reads");

                std::cout << "PASS: RenderFace::Both keeps the face the pass would have culled, and the\n"
                             "      shader lights it as the near side's mirror instead of leaving it dark.\n";
            }
            catch (const std::exception& exception)
            {
                std::cerr << "FAIL: " << exception.what() << std::endl;
                std::cout.flush();
                std::_Exit(1);
            }

            std::cout.flush();
            std::_Exit(0);
        });

        Pine::Engine::Run();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }
