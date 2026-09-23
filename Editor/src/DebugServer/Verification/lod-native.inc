    // Model LOD end to end: the level the scene processor picks for each renderer as it moves away
    // from the reference camera, and what the level viewport then culls and draws.
    //
    // Needs a native probe because nothing over HTTP sets a model's LOD levels. Three copies of the
    // sphere primitive, all along the level camera's view direction:
    //
    //   A - scale 1, moved through every threshold.
    //   B - scale 3, so its distances count a third: it must keep detail three times further out.
    //   C - drawing one mesh by index, which LOD does not apply to: never swapped, never hidden.
    try
    {
        const auto check = [](const bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        auto* sphere = Pine::Assets::Get<Pine::Model>("engine/primitive/sphere");
        auto* cube = Pine::Assets::Get<Pine::Model>("engine/primitive/cube");

        check(sphere != nullptr && cube != nullptr, "Missing the sphere and cube primitives");

        // Sphere up close, cube from 10, nothing past 30.
        {
            std::vector<Pine::ModelLodLevel> levels(1);

            levels[0].LodModel = cube;
            levels[0].Distance = 10.f;

            sphere->SetLodLevels(levels);
            sphere->SetLodCullDistance(30.f);
        }

        const auto spawn = [&](const std::string& name)
        {
            auto* entity = Pine::Entities::Create(name);
            auto* renderer = entity->AddComponent<Pine::ModelRenderer>();

            renderer->SetModel(sphere);

            return entity;
        };

        static struct
        {
            Pine::Model* Sphere = nullptr;
            Pine::Model* Cube = nullptr;

            Pine::Entity* A = nullptr;
            Pine::Entity* B = nullptr;
            Pine::Entity* C = nullptr;

            int Phase = 0;
            int Frame = 0;
            int NextPhaseFrame = 12;

            std::uint64_t CloseVertexCount = 0;
            int CloseVisibleCount = 0;

            std::uint64_t SwappedVertexCount = 0;
            int SwappedVisibleCount = 0;
        } probe;

        probe.Sphere = sphere;
        probe.Cube = cube;
        probe.A = spawn("LOD probe A");
        probe.B = spawn("LOD probe B");
        probe.C = spawn("LOD probe C");

        probe.B->GetTransform()->SetLocalScale(Pine::Vector3f(3.f));
        probe.C->GetComponent<Pine::ModelRenderer>()->SetModelMeshIndex(0);

        Pine::RenderManager::AddRenderCallback([](Pine::RenderingContext*, const Pine::RenderStage stage, float)
        {
            if (stage != Pine::RenderStage::PostRender)
            {
                return;
            }

            auto* context = Editor::RenderHandler::GetLevelRenderingContext();

            if (context == nullptr || context->SceneCamera == nullptr || context->Size.x < 1.f)
            {
                return;
            }

            probe.Frame++;

            // Places the three copies along the camera's view, 'distance' world units out.
            const auto place = [context](Pine::Entity* entity, const float distance)
            {
                const auto* cameraTransform = context->SceneCamera->GetParent()->GetTransform();
                const auto forward = glm::normalize(cameraTransform->GetRotation() * Pine::Vector3f(0.f, 0.f, -1.f));

                entity->GetTransform()->SetLocalPosition(cameraTransform->GetPosition() + forward * distance);
            };

            const auto drawnModel = [](Pine::Entity* entity)
            {
                return entity->GetComponent<Pine::ModelRenderer>()->GetRenderingHintData().LodModel;
            };

            const auto describe = [](const Pine::Model* model) -> std::string
            {
                if (model == nullptr)
                {
                    return "hidden";
                }

                return model->GetPath();
            };

            try
            {
                const auto check = [](const bool condition, const std::string& message)
                {
                    if (!condition) throw std::runtime_error(message);
                };

                if (probe.Frame == 1)
                {
                    // With a camera on the game view as well, it would be the one measured from.
                    check(Pine::RenderManager::GetPrimaryRenderingContext()->SceneCamera == nullptr,
                        "The game view has a camera, so the level camera is not the LOD reference");

                    place(probe.A, 5.f);
                    place(probe.B, 20.f);
                    place(probe.C, 50.f);

                    return;
                }

                // Several frames between phases: a move now reaches the batch next frame, and the
                // statistics of the frame after that are the first to show it.
                if (probe.Frame < probe.NextPhaseFrame)
                {
                    return;
                }

                probe.NextPhaseFrame = probe.Frame + 12;

                std::cout << "phase " << probe.Phase
                          << ": A " << describe(drawnModel(probe.A))
                          << ", B " << describe(drawnModel(probe.B))
                          << ", C " << describe(drawnModel(probe.C))
                          << " | visible " << context->Statistics.VisibleObjectCount
                          << " culled " << context->Statistics.CulledObjectCount
                          << " vertices " << context->Statistics.VertexCount << std::endl;

                if (probe.Phase == 0)
                {
                    check(drawnModel(probe.A) == probe.Sphere, "A at 5 should draw the sphere itself");
                    check(drawnModel(probe.B) == probe.Sphere, "B at 20, scale 3, should draw the sphere itself");
                    check(drawnModel(probe.C) == probe.Sphere,
                        "C at 50 draws one mesh by index, and should keep the sphere past the cull distance");

                    probe.CloseVertexCount = context->Statistics.VertexCount;
                    probe.CloseVisibleCount = context->Statistics.VisibleObjectCount;

                    // Unscaled, 45 would be past the cull distance. At scale 3 it counts as 15.
                    place(probe.A, 15.f);
                    place(probe.B, 45.f);

                    probe.Phase = 1;

                    return;
                }

                if (probe.Phase == 1)
                {
                    check(drawnModel(probe.A) == probe.Cube, "A at 15 should draw the cube level");
                    check(drawnModel(probe.B) == probe.Cube,
                        "B at 45, scale 3, should count as 15 and draw the cube level");

                    check(context->Statistics.VisibleObjectCount == probe.CloseVisibleCount,
                        "Swapping levels changed how many objects the viewport sees");
                    check(context->Statistics.VertexCount < probe.CloseVertexCount,
                        "The viewport submitted no fewer vertices with two spheres swapped for cubes");

                    probe.SwappedVertexCount = context->Statistics.VertexCount;
                    probe.SwappedVisibleCount = context->Statistics.VisibleObjectCount;

                    place(probe.A, 40.f);

                    probe.Phase = 2;

                    return;
                }

                if (probe.Phase == 2)
                {
                    check(drawnModel(probe.A) == nullptr, "A at 40 is past the cull distance and should be hidden");
                    check(drawnModel(probe.B) == probe.Cube, "B should still draw the cube level");
                    check(drawnModel(probe.C) == probe.Sphere, "C should still draw the sphere");

                    check(context->Statistics.VisibleObjectCount == probe.SwappedVisibleCount - 1,
                        "The viewport should count A as culled once it is past the cull distance");
                    check(context->Statistics.VertexCount < probe.SwappedVertexCount,
                        "The viewport still submitted A's vertices after hiding it");

                    // Out of order, and the further level's model does not exist. The missing
                    // level is skipped, so the cube level still holds past it.
                    std::vector<Pine::ModelLodLevel> levels(2);

                    levels[0].LodModel = Pine::UId::New();
                    levels[0].Distance = 20.f;
                    levels[1].LodModel = probe.Cube;
                    levels[1].Distance = 10.f;

                    probe.Sphere->SetLodLevels(levels);

                    place(probe.A, 25.f);

                    probe.Phase = 3;

                    return;
                }

                check(drawnModel(probe.A) == probe.Cube,
                    "A at 25, with the level at 20 missing, should fall back to the cube level at 10");

                std::cout << "PASS: renderers draw the LOD level for their distance, scaled by their size, are\n"
                             "      culled past the cull distance, skip a missing level, and a renderer drawing\n"
                             "      one mesh by index keeps its model. The viewport's statistics follow.\n";
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
