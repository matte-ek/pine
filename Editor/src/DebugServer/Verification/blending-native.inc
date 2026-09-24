    // Transparency end to end: a material's alpha reaching the shader, the blend pass drawing it
    // over what is behind it, and the draw list putting it in order.
    //
    // Read off the rendered frame rather than argued about. One pixel is sampled three times - with
    // the near cube opaque, semi-transparent, and gone - and a blend is the only thing that lands
    // between the other two readings. None of this is reachable over HTTP: nothing creates a
    // material there, and /edit cannot make one transparent.
    try
    {
        const auto check = [](const bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        auto* cube = Pine::Assets::Get<Pine::Model>("engine/primitive/cube");

        check(cube != nullptr, "Missing engine/primitive/cube");

        // Two saturated, complementary colours, so a blend of the two cannot be mistaken for either
        // of them. Ambient as well as diffuse: it keeps the reading away from zero on the faces the
        // light does not reach.
        const auto createMaterial = [&](const std::string& path, const Pine::Vector3f& color)
        {
            auto* material = dynamic_cast<Pine::Material*>(
                Pine::Assets::CreateAsset(Pine::AssetType::Material, path));

            check(material != nullptr, "Could not create the material " + path);

            material->SetDiffuseColor(color);
            material->SetAmbientColor(color);

            return material;
        };

        auto* backdropMaterial = createMaterial("engine/verify-blending-backdrop", Pine::Vector3f(0.f, 1.f, 0.f));
        auto* glassMaterial = createMaterial("engine/verify-blending-glass", Pine::Vector3f(1.f, 0.f, 0.f));

        const auto spawn = [&](const std::string& name, const Pine::Vector3f& position, Pine::Material* material)
        {
            auto* entity = Pine::Entities::Create(name);

            entity->GetTransform()->SetLocalPosition(position);

            auto* renderer = entity->AddComponent<Pine::ModelRenderer>();

            renderer->SetModel(cube);

            // An override material rather than one on the mesh, deliberately: that is the path the
            // scene processor used to miss when deciding which objects need blending, and a cube
            // primitive has no material of its own to edit anyway.
            renderer->SetOverrideMaterial(material);

            return entity;
        };

        // Lit, because ambient alone leaves the frame too dark to read a blend out of - and a lit
        // scene is what this is meant to be representative of.
        {
            auto* lightEntity = Pine::Entities::Create("Probe light");

            lightEntity->GetTransform()->SetLocalPosition(Pine::Vector3f(0.f, 0.f, 1.f));

            auto* light = lightEntity->AddComponent<Pine::Light>();

            light->SetLightType(Pine::LightType::PointLight);
            light->SetLightIntensity(6.f);
            light->SetRange(40.f);
        }

        // Straight down the editor camera's default view, one cube behind the other.
        auto* backdrop = spawn("Backdrop", Pine::Vector3f(0.f, 0.f, -8.f), backdropMaterial);
        auto* glass = spawn("Glass", Pine::Vector3f(0.f, 0.f, -4.f), glassMaterial);

        check(backdrop != nullptr && glass != nullptr, "Could not spawn the probe entities");

        // What the phases below carry between frames.
        static struct
        {
            Pine::Material* GlassMaterial = nullptr;
            Pine::Entity* Glass = nullptr;

            int Phase = 0;
            int Frame = 0;
            int NextPhaseFrame = 12;

            Pine::Vector4i Opaque = Pine::Vector4i(0);
            Pine::Vector4i Blended = Pine::Vector4i(0);
            Pine::Vector4i BackdropOnly = Pine::Vector4i(0);

            std::string OrderingError;
        } probe;

        probe.GlassMaterial = glassMaterial;
        probe.Glass = glass;

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

            // Several frames between phases: a material or entity changed now reaches the batch on
            // the next frame, and the frame after that is the first one that shows it.
            if (probe.Frame < probe.NextPhaseFrame)
            {
                return;
            }

            probe.NextPhaseFrame = probe.Frame + 12;

            // The level viewport's centre pixel, which is the near cube while it is there.
            const auto width = static_cast<int>(context->Size.x);
            const auto height = static_cast<int>(context->Size.y);

            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);

            context->FrameBuffer->Bind();
            context->FrameBuffer->ReadPixels(Pine::Vector2i(0), Pine::Vector2i(width, height),
                Pine::Graphics::ReadFormat::RGBA, Pine::Graphics::TextureDataFormat::UnsignedByte,
                pixels.size(), pixels.data());
            Pine::Graphics::GetGraphicsAPI()->BindFrameBuffer(nullptr);

            const std::size_t centre = (static_cast<std::size_t>(height / 2) * width + width / 2) * 4;
            const auto sample = Pine::Vector4i(pixels[centre], pixels[centre + 1], pixels[centre + 2], pixels[centre + 3]);

            // Printed every phase: a frame that renders nothing reads as black at the centre too,
            // and these are what tell the two apart when this fails.
            std::cout << "phase " << probe.Phase
                      << ": centre " << sample.r << "," << sample.g << "," << sample.b << "," << sample.a
                      << " viewport " << width << "x" << height
                      << " visible " << context->Statistics.VisibleObjectCount
                      << " draws " << context->Statistics.DrawCalls << std::endl;

            if (probe.Phase == 0)
            {
                probe.Opaque = sample;

                // The order the blend pass depends on, checked against the scene that is really
                // there. Here rather than later because the phases after this one make a cube
                // transparent and then remove it, and a list of one item cannot be out of order.
                const auto checkOrdering = [context]() -> std::string
                {
                    Pine::Rendering::ObjectBatchMap batch;

                    for (auto& modelRenderer : Pine::Components::Get<Pine::ModelRenderer>())
                    {
                        if (!modelRenderer.GetModel())
                        {
                            continue;
                        }

                        batch[{ modelRenderer.GetModel(), modelRenderer.GetOverrideMaterial() }]
                            .push_back({ &modelRenderer });
                    }

                    const auto viewPosition = context->SceneCamera->GetParent()->GetTransform()->GetPosition();

                    Pine::Rendering::DrawList list;

                    list.Build(batch, Pine::MaterialRenderingMode::Opaque, context->Visibility,
                        { Pine::Rendering::DrawOrder::BackToFront, viewPosition });

                    if (list.GetItems().size() < 2)
                    {
                        return "Expected both cubes in the draw list, got " + std::to_string(list.GetItems().size());
                    }

                    for (std::size_t i = 1; i < list.GetItems().size(); i++)
                    {
                        if (list.GetItems()[i - 1].Distance < list.GetItems()[i].Distance)
                        {
                            return "BackToFront produced an item further away than the one before it";
                        }
                    }

                    list.Build(batch, Pine::MaterialRenderingMode::Opaque, context->Visibility,
                        { Pine::Rendering::DrawOrder::FrontToBack, viewPosition });

                    for (std::size_t i = 1; i < list.GetItems().size(); i++)
                    {
                        if (list.GetItems()[i - 1].Distance > list.GetItems()[i].Distance)
                        {
                            return "FrontToBack produced an item nearer than the one before it";
                        }
                    }

                    return "";
                };

                probe.OrderingError = checkOrdering();

                probe.GlassMaterial->SetRenderingMode(Pine::MaterialRenderingMode::Transparent);
                probe.GlassMaterial->SetAlpha(0.5f);

                probe.Phase = 1;

                return;
            }

            if (probe.Phase == 1)
            {
                probe.Blended = sample;

                probe.Glass->SetActive(false);

                probe.Phase = 2;

                return;
            }

            probe.BackdropOnly = sample;

            std::cout << "centre pixel: opaque "
                      << probe.Opaque.r << "," << probe.Opaque.g << "," << probe.Opaque.b << "," << probe.Opaque.a
                      << "  blended "
                      << probe.Blended.r << "," << probe.Blended.g << "," << probe.Blended.b << "," << probe.Blended.a
                      << "  backdrop only "
                      << probe.BackdropOnly.r << "," << probe.BackdropOnly.g << "," << probe.BackdropOnly.b << "," << probe.BackdropOnly.a
                      << std::endl;

            try
            {
                const auto check = [](const bool condition, const std::string& message)
                {
                    if (!condition) throw std::runtime_error(message);
                };

                // Comfortably inside the two readings rather than near either: tone mapping and the
                // film grain move the exact value around, and what is being asserted is that the
                // pixel is a mix at all, not what a particular alpha resolves to.
                const auto between = [](const int value, const int a, const int b)
                {
                    return value > std::min(a, b) + 5 && value < std::max(a, b) - 5;
                };

                check(probe.Opaque.r > 60 && probe.Opaque.g < 40,
                    "The opaque near cube should read red");
                check(probe.BackdropOnly.g > 60 && probe.BackdropOnly.r < 40,
                    "With the near cube gone the backdrop should read green");

                check(between(probe.Blended.r, probe.Opaque.r, probe.BackdropOnly.r),
                    "Blended red is not between the near cube and the backdrop - nothing blended");
                check(between(probe.Blended.g, probe.Opaque.g, probe.BackdropOnly.g),
                    "Blended green is not between the near cube and the backdrop - nothing blended");

                // Neither cube has any blue, and the level viewport clears to a strong one, so blue
                // in the blend can only be the clear colour showing through the resolve.
                check(probe.Blended.b <= std::max(probe.Opaque.b, probe.BackdropOnly.b) + 5,
                    "Blended blue is above both readings - the clear colour shows through the blend");

                // The editor draws this image with blending on, so anything short of opaque lets
                // the panel behind it through.
                check(probe.Opaque.a == 255 && probe.Blended.a == 255 && probe.BackdropOnly.a == 255,
                    "The final image is not opaque everywhere");

                check(probe.OrderingError.empty(), probe.OrderingError);

                std::cout << "PASS: a Transparent material blends over what is behind it and nothing else, an\n"
                             "      override material reaches the blend pass, and the draw list orders both ways as asked.\n";
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
