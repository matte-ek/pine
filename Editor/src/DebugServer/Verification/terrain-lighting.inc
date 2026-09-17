    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        // Part 1 - the state the frame is supposed to change, checked before one has run. A chunk
        // is born with no lights and no computed slots, so everything the HTTP side reads below is
        // something the render path put there rather than something the asset came with.
        {
            auto scratch = new Pine::Terrain();

            scratch->SetChunkQuads(8);
            scratch->SetChunkSize(8.f);
            scratch->Resize({ 0, 0 }, { 2, 2 });

            for (auto& chunk : scratch->GetChunks())
            {
                check(!chunk.LightSlots.HasComputedData, "A fresh chunk should not claim computed light slots");

                for (int slot = 0; slot < Pine::Renderer3D::Specifications::ObjectLightSlots::COUNT; slot++)
                {
                    check(chunk.LightSlots.Index[slot].Get() == nullptr, "A fresh chunk should have no light in any slot");
                }
            }

            // A chunk is lit at the centre of its box, so a chunk that changed shape has to pick
            // its lights again. Raised from UpdateChunkBounds, which is the one place that box
            // moves - the sculpting brush then gets this for free rather than having to remember.
            for (auto& chunk : scratch->GetChunks())
            {
                chunk.LightSlots.HasComputedData = true;
            }

            check(scratch->SetSampleHeight({ 4, 4 }, 3.f), "Writing an interior sample was rejected");

            check(!scratch->GetChunks().front().LightSlots.HasComputedData,
                "Changing a chunk's shape should invalidate the lights it was assigned");

            scratch->Dispose();

            delete scratch;
        }

        // Part 2 - the scene the HTTP side looks at.
        //
        // Flat ground with one square plateau on it, rather than the noise the render check uses.
        // The shadow this exists to find has to be attributable: on a noise terrain a dark patch
        // could be shading just as easily as shadow, and telling those apart in a screenshot is
        // exactly the judgement call a verification script should not be making.
        {
            auto terrain = dynamic_cast<Pine::Terrain*>(
                Pine::Assets::CreateAsset(Pine::AssetType::Terrain, "projects/terrain/assets/ground"));

            Pine::Assets::Internal::RegisterAsset(terrain);

            terrain->SetLayer(0, Pine::Assets::Get<Pine::Material>("engine/materials/default"));

            // One sample per world unit, and a terrain small enough to sit inside the cascades'
            // reach: shadows from the sun only cover a few tens of units around the camera, so a
            // terrain built at the default 64-unit chunks would put the plateau and its shadow on
            // opposite sides of the last cascade's far plane.
            terrain->SetChunkQuads(16);
            terrain->SetChunkSize(16.f);
            terrain->Resize({ 0, 0 }, { 4, 4 });

            // A square block in the middle of the 64-unit field, tall enough that its shadow is
            // longer than it is wide. The script recomputes nothing from these - it only needs the
            // ground around the block to be flat - but they are what makes the shadow large enough
            // to count pixels of.
            constexpr int plateauMin = 26;
            constexpr int plateauMax = 38;
            constexpr float plateauHeight = 8.f;

            for (int z = plateauMin; z <= plateauMax; z++)
            {
                for (int x = plateauMin; x <= plateauMax; x++)
                {
                    check(terrain->SetSampleHeight({ x, z }, plateauHeight), "The plateau fell outside the field");
                }
            }

            auto terrainEntity = Pine::Entities::Create("Terrain");

            terrainEntity->AddComponent<Pine::TerrainRendererComponent>()->SetTerrain(terrain);

            // Pitched past straight down so the light travels downwards and towards +z, which puts
            // the plateau's shadow on the side of it the camera below is looking from. A rotation
            // of p about x takes the light's own -z forward to (0, sin p, -cos p), so an elevation
            // of 35 degrees over the +z horizon is p = -145.
            auto sunEntity = Pine::Entities::Create("Sun");

            sunEntity->GetTransform()->SetEulerAngles(Pine::Vector3f(-145.f, 0.f, 0.f));

            auto sun = sunEntity->AddComponent<Pine::Light>();

            sun->SetLightType(Pine::LightType::Directional);
            sun->SetLightIntensity(1.f);
            sun->SetCastShadows(true);

            // Eight lamps for five point slots, so which five a chunk keeps is a real choice the
            // script can recompute. Spread over the terrain and deliberately off the grid lines, so
            // no two of them are the same distance from any chunk's centre and the expected order
            // is unambiguous.
            const Pine::Vector2f lampPositions[] = {
                {  7.f,  5.f }, { 51.f,  9.f }, { 13.f, 47.f }, { 57.f, 53.f },
                { 29.f, 19.f }, { 37.f, 61.f }, {  3.f, 31.f }, { 61.f, 25.f }
            };

            for (int i = 0; i < 8; i++)
            {
                auto lampEntity = Pine::Entities::Create("Lamp " + std::to_string(i));

                lampEntity->GetTransform()->SetLocalPosition(
                    Pine::Vector3f(lampPositions[i].x, 12.f, lampPositions[i].y));

                auto lamp = lampEntity->AddComponent<Pine::Light>();

                lamp->SetLightType(Pine::LightType::PointLight);
                lamp->SetLightIntensity(1.f);
                lamp->SetRange(24.f);
            }
        }

        std::cout << "PASS(native): a fresh chunk carries no lights and no computed slots, and "
                     "reshaping a chunk invalidates the ones it had." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }

    Pine::Engine::Run();
