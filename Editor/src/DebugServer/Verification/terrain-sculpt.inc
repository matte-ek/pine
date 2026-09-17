    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        // Part 1 - the ray march, which is what turns a cursor into a point to sculpt at. It is
        // plain CPU maths over the height field, so it is checked here rather than over HTTP: the
        // debug server has no cursor, and a picking bug would otherwise only show up as the brush
        // landing somewhere other than where the author clicked.
        {
            auto scratch = new Pine::Terrain();

            scratch->SetChunkQuads(16);
            scratch->SetChunkSize(16.f);
            scratch->Resize({ 0, 0 }, { 2, 2 });
            scratch->GenerateFromNoise();

            const float extent = 32.f;

            // Straight down, over the whole terrain including its rim and its corners. The march
            // has to agree with GetHeightAt, because the mesh, the collision and the brush are all
            // the same surface or none of them are.
            for (int step = 0; step <= 64; step++)
            {
                const float along = extent * static_cast<float>(step) / 64.f;

                for (const auto& point : { Pine::Vector2f(along, 0.f), Pine::Vector2f(0.f, along),
                                           Pine::Vector2f(along, extent), Pine::Vector2f(extent, along),
                                           Pine::Vector2f(along, along), Pine::Vector2f(along, extent - along) })
                {
                    const auto expected = scratch->GetHeightAt(point.x, point.y);

                    check(expected.has_value(), "GetHeightAt answered nothing inside the terrain");

                    const auto hit = scratch->Raycast({ point.x, 400.f, point.y }, { 0.f, -1.f, 0.f });

                    check(hit.has_value(), "A ray straight down at a point on the terrain missed it");
                    check(std::abs(hit->Position.y - *expected) < 0.01f,
                        "A downward ray hit a different height than GetHeightAt reports");
                    check(std::abs(hit->Position.x - point.x) < 0.001f && std::abs(hit->Position.z - point.y) < 0.001f,
                        "A downward ray moved horizontally");
                    check(std::abs((400.f - hit->Position.y) - hit->Distance) < 0.01f,
                        "A hit's reported distance does not match where it landed");
                }
            }

            // Obliquely, from a ring of directions, aimed a little under the surface so the ray
            // crosses the ground rather than grazing it. Each of these traverses many cells, which
            // is what exercises the march rather than a single cell's triangles. It may legitimately
            // meet a ridge before the point it was aimed at, so what is checked is that wherever it
            // landed is on the surface.
            for (int step = 0; step < 32; step++)
            {
                const float angle = static_cast<float>(step) * 0.196f;
                const auto surface = scratch->GetHeightAt(extent * 0.5f, extent * 0.5f);

                const Pine::Vector3f target = { extent * 0.5f, *surface - 0.5f, extent * 0.5f };
                const Pine::Vector3f origin = target +
                    Pine::Vector3f(std::cos(angle) * 30.f, 25.f, std::sin(angle) * 30.f);

                const auto hit = scratch->Raycast(origin, target - origin);

                check(hit.has_value(), "An oblique ray aimed into the terrain missed it");

                const auto expected = scratch->GetHeightAt(hit->Position.x, hit->Position.z);

                check(expected.has_value() && std::abs(hit->Position.y - *expected) < 0.02f,
                    "An oblique ray landed somewhere that is not on the surface");
            }

            // Exactly at a grid vertex, which is the corner four quads and eight triangles share.
            // Without an edge tolerance every one of those eight rejects it and the ray falls
            // through the ground - and clicking a round coordinate on flat ground does exactly this.
            for (int sample = 1; sample < 31; sample++)
            {
                const auto position = Pine::Vector2f(sample, sample) * scratch->GetSampleSpacing();
                const auto surface = scratch->GetHeightAt(position.x, position.y);

                const Pine::Vector3f vertex = { position.x, *surface, position.y };
                const Pine::Vector3f origin = vertex + Pine::Vector3f(18.f, 15.f, 7.f);

                check(scratch->Raycast(origin, vertex - origin).has_value(),
                    "A ray aimed exactly at a grid vertex found no ground");
            }

            // Misses are answers, not failures: a brush dragged past the edge queries off the
            // terrain on most frames.
            check(!scratch->Raycast({ extent + 10.f, 400.f, extent * 0.5f }, { 0.f, -1.f, 0.f }).has_value(),
                "A ray outside the terrain's footprint hit something");
            check(!scratch->Raycast({ extent * 0.5f, 400.f, extent * 0.5f }, { 0.f, 1.f, 0.f }).has_value(),
                "A ray pointing away from the terrain hit it");
            check(!scratch->Raycast({ -10.f, 200.f, extent * 0.5f }, { 1.f, 0.f, 0.f }).has_value(),
                "A ray running flat above the terrain hit it");
            check(!scratch->Raycast({ extent * 0.5f, 0.f, extent * 0.5f }, { 0.f, 0.f, 0.f }).has_value(),
                "A zero-length direction produced a hit");

            // From below: an editor camera that has dropped under the ground still has to be able
            // to pick the surface it is looking up at.
            {
                const auto expected = scratch->GetHeightAt(extent * 0.5f, extent * 0.5f);
                const auto hit = scratch->Raycast({ extent * 0.5f, -400.f, extent * 0.5f }, { 0.f, 1.f, 0.f });

                check(hit.has_value() && std::abs(hit->Position.y - *expected) < 0.01f,
                    "A ray from below did not meet the underside of the ground");
            }

            scratch->Dispose();

            delete scratch;
        }

        // Part 2 - the rectangle accessors the brush and its undo record are built on. What matters
        // here is that a rectangle read and written back is bit-for-bit what it was, because that
        // is the whole of "undo restores the ground exactly".
        {
            auto scratch = new Pine::Terrain();

            scratch->SetChunkQuads(8);
            scratch->SetChunkSize(8.f);
            scratch->Resize({ -1, -1 }, { 2, 2 });
            scratch->GenerateFromNoise();

            const Pine::Vector2i low = { -3, -2 };
            const Pine::Vector2i high = { 5, 6 };

            const Pine::TerrainSampleRect rect = { low, high };

            // One sample, immediately outside the rectangle's low corner, so that every write below
            // can be checked for reaching past the edge it was given.
            const Pine::TerrainSampleRect neighbourRect = { { low.x - 1, low.y }, { low.x - 1, low.y } };

            const auto before = scratch->GetSampleHeightRect(rect);
            const auto neighbour = scratch->GetSampleHeightRect(neighbourRect);

            check(before.size() == 9 * 9, "A 9x9 rectangle did not read 81 samples");

            auto modified = before;

            for (auto& sample : modified)
            {
                sample = 1234;
            }

            check(scratch->SetSampleHeightRect(rect, modified), "Writing a rectangle back was refused");
            check(scratch->GetSampleHeightRect(rect) == modified, "A rectangle read back differently than written");
            check(scratch->GetSampleHeightRect(neighbourRect) == neighbour,
                "Writing a rectangle changed a sample outside it");

            check(scratch->SetSampleHeightRect(rect, before), "Restoring a rectangle was refused");
            check(scratch->GetSampleHeightRect(rect) == before,
                "A restored rectangle does not match the samples that were read");

            // Out of bounds is refused rather than clamped: a caller silently getting a smaller
            // rectangle than it asked for would misalign every row of its undo record.
            check(scratch->GetSampleHeightRect({ { low.x - 8, low.y }, high }).empty(),
                "A rectangle reaching outside the field was read anyway");
            check(!scratch->SetSampleHeightRect(rect, { 1, 2, 3 }),
                "A rectangle was written with the wrong amount of data");

            // The same rectangle over the weight field, which is what a paint stroke reads and its
            // undo record restores. A sample of it is four bytes rather than one value, so every
            // row expression here is a different one from the height version above and none of
            // them is checked by it.
            constexpr int layers = Pine::Terrain::MaximumLayerCount;

            const auto weightsBefore = scratch->GetSampleWeightRect(rect);
            const auto weightNeighbour = scratch->GetSampleWeightRect(neighbourRect);

            check(weightsBefore.size() == 9 * 9 * layers,
                "A 9x9 weight rectangle did not read one weight per layer per sample");

            auto painted = weightsBefore;

            // Entirely layer 1, which is what a paint stroke held long enough converges on.
            for (std::size_t sample = 0; sample < painted.size(); sample += layers)
            {
                painted[sample] = 0;
                painted[sample + 1] = 255;
            }

            check(scratch->SetSampleWeightRect(rect, painted), "Writing a weight rectangle back was refused");
            check(scratch->GetSampleWeightRect(rect) == painted,
                "A weight rectangle read back differently than written");
            check(scratch->GetSampleWeightRect(neighbourRect) == weightNeighbour,
                "Writing a weight rectangle changed a sample outside it");

            // The single-sample accessor has to see what the rectangle wrote: the brush writes
            // rectangles, while everything that reads one sample - the debug server, the editor -
            // goes through the other one, and the two index the field differently.
            const auto corner = scratch->GetSampleWeights(low);

            check(corner.has_value() && corner->y > 0.99f && corner->x < 0.01f,
                "A sample read back as a different layer than the rectangle painted it");

            // Weights are a texture, not geometry. A paint stroke that moved the ground would be
            // undone by the wrong command and would re-cook collision for nothing.
            check(scratch->GetSampleHeightRect(rect) == before,
                "Writing layer weights moved the ground");

            check(scratch->SetSampleWeightRect(rect, weightsBefore), "Restoring a weight rectangle was refused");
            check(scratch->GetSampleWeightRect(rect) == weightsBefore,
                "A restored weight rectangle does not match the bytes that were read");

            check(scratch->GetSampleWeightRect({ { low.x - 8, low.y }, high }).empty(),
                "A weight rectangle reaching outside the field was read anyway");
            check(!scratch->SetSampleWeightRect(rect, { 1, 2, 3 }),
                "A weight rectangle was written with the wrong amount of data");

            scratch->Dispose();

            delete scratch;
        }

        // Part 3 - the scene the HTTP side sculpts.
        //
        // Deliberately flat: every check below is about how much the ground moved and whether undo
        // put it back, and starting from noise would mean measuring a brush against a surface that
        // already varies by more than the brush moves it.
        {
            auto terrain = dynamic_cast<Pine::Terrain*>(
                Pine::Assets::CreateAsset(Pine::AssetType::Terrain, "projects/terrain/assets/ground"));

            Pine::Assets::Internal::RegisterAsset(terrain);

            terrain->SetLayer(0, Pine::Assets::Get<Pine::Material>("engine/materials/default"));

            // Layer 1 is what the paint strokes below hand the ground over to, and it is green so
            // that the picture can be read without disturbing the brush ring's check further down:
            // that one counts strongly warm pixels, and painted ground has to stay clear of them.
            auto paintedMaterial = dynamic_cast<Pine::Material*>(
                Pine::Assets::CreateAsset(Pine::AssetType::Material, "projects/terrain/assets/painted"));

            Pine::Assets::Internal::RegisterAsset(paintedMaterial);

            paintedMaterial->SetDiffuseColor({ 0.f, 1.f, 0.f });

            terrain->SetLayer(1, paintedMaterial);

            // One sample per world unit over a 64-unit field, so that a radius in world units and a
            // radius in samples are the same number and the script's arithmetic stays readable.
            terrain->SetChunkQuads(16);
            terrain->SetChunkSize(16.f);
            terrain->Resize({ 0, 0 }, { 4, 4 });

            auto terrainEntity = Pine::Entities::Create("Terrain");

            terrainEntity->AddComponent<Pine::TerrainRendererComponent>()->SetTerrain(terrain);

            auto sunEntity = Pine::Entities::Create("Sun");

            sunEntity->GetTransform()->SetEulerAngles(Pine::Vector3f(-145.f, 0.f, 0.f));

            auto sun = sunEntity->AddComponent<Pine::Light>();

            sun->SetLightType(Pine::LightType::Directional);
            sun->SetLightIntensity(1.f);
            sun->SetCastShadows(true);
        }

        // Part 4 - the brush overlay's shader version, end to end.
        //
        // The whole chain is invisible when it breaks: '#shader version' has to register the
        // version, CompileShaderVersion has to turn it into a #define, and the #ifdef in the
        // fragment source has to be what brings the uniform into existence. Any link failing leaves
        // a shader that compiles perfectly and simply never draws a ring, which no screenshot of a
        // terrain would obviously fail on.
        //
        // So it is checked by the one thing that can only be true if every link held: the brush
        // uniform exists in the variant and does not exist in the plain one.
        {
            const auto shader = Pine::Assets::Get<Pine::Shader>("engine/shaders/3d/terrain");

            check(shader != nullptr, "The terrain shader is not loaded");

            constexpr auto brushVersion = static_cast<Pine::ShaderVersion>(
                Pine::Renderer3D::Specifications::ShaderVersions::Terrain::Brush);

            // Compiled from the asset exactly as it was loaded, before anything re-imports it.
            // '#shader version' is only read while processing the *source*, so a .passet written
            // before the directive existed carries no version at all - and a shader with no version
            // registered still compiles, silently, into a program the macro never reached. Checking
            // this first is what says the shipped asset carries the version rather than only the
            // source file the developer happens to have.
            check(shader->HasShaderVersion(0), "The terrain shader has no default version compiled");
            check(shader->CompileShaderVersion(brushVersion), "The terrain shader's brush version did not compile");
            check(shader->HasShaderVersion(brushVersion), "The brush version compiled but was not registered");

            const auto plain = shader->GetProgram(0);
            const auto brush = shader->GetProgram(brushVersion);

            check(plain != brush, "The brush version handed back the same program as the default one");
            check(brush->GetUniformVariable("brushRing") != nullptr,
                "The brush version has no 'brushRing' uniform - the VERSION_BRUSH macro never reached the source");
            check(plain->GetUniformVariable("brushRing") == nullptr,
                "The default version carries the brush uniform, so a built game would pay for it");

            // Both versions still have to be usable terrain shaders, not just distinct programs.
            check(plain->GetUniformVariable("splatTransform") != nullptr,
                "The default terrain program lost its splat transform");
            check(brush->GetUniformVariable("splatTransform") != nullptr,
                "The brush terrain program lost its splat transform");

            // And again after re-importing the source, which is what a shader file saved while the
            // editor is open goes through. Registered once however many times that happens: a
            // second copy of the entry would become a second #define, and redefining a macro is an
            // error in GLSL, so the shader would survive the first save and fail the second.
            check(Pine::Importer::ShaderImporter::Import(shader), "Re-importing the terrain shader failed");
            check(Pine::Importer::ShaderImporter::Import(shader), "Re-importing the terrain shader twice failed");

            check(shader->CompileShaderVersion(brushVersion),
                "The brush version stopped compiling after the shader was re-imported twice");

            check(shader->GetProgram(brushVersion)->GetUniformVariable("brushRing") != nullptr,
                "The brush uniform disappeared after re-importing the shader");
        }

        // Part 5 - the cursor ray and the pick it feeds, which is how a click becomes a place to
        // sculpt. Nothing over HTTP can move a mouse, and a sign or a transpose wrong in the
        // unprojection puts the brush somewhere other than under the cursor - a bug that only a
        // hand on a mouse would ever notice. So it is checked against a camera aimed at ground
        // whose height is known by construction.
        {
            auto terrain = Pine::Assets::Get<Pine::Terrain>("ground");

            check(terrain != nullptr, "The scene's terrain is not registered under 'ground'");

            // Half the terrain raised to a step, so that where a ray lands decides what height
            // it comes back with. On ground that is flat everywhere, a picker that ignored the
            // height field and simply intersected the y = 0 plane would pass everything below.
            constexpr float stepHeight = 6.f;

            for (int z = terrain->GetSampleMin().y; z <= terrain->GetSampleMax().y; z++)
            {
                for (int x = 32; x <= terrain->GetSampleMax().x; x++)
                {
                    check(terrain->SetSampleHeight({ x, z }, stepHeight), "The pick step fell outside the field");
                }
            }

            auto cameraEntity = Pine::Entities::Create("Pick Camera");
            auto camera = cameraEntity->AddComponent<Pine::Camera>();

            const Pine::Vector2f viewport = { 800.f, 600.f };

            camera->SetOverrideAspectRatio(viewport.x / viewport.y);
            camera->SetFarPlane(500.f);

            // Looking down at the raised half, tilted well off vertical so the ray is not axis
            // aligned - an axis aligned one would survive several of the mistakes this is looking
            // for. A rotation of p about x takes the camera's own -z forward to (0, sin p, -cos p),
            // so -60 degrees looks downwards and towards -z: the camera stands on the +z side, and
            // from there lower on the screen is further along +z, nearer to it.
            cameraEntity->GetTransform()->SetLocalPosition({ 40.f, 60.f, 72.f });
            cameraEntity->GetTransform()->SetEulerAngles({ -60.f, 0.f, 0.f });

            camera->OnRender(0.f);

            namespace Sculpting = Editor::TerrainSculpting;

            const auto centreRay = Sculpting::BuildCursorRay(camera, viewport * 0.5f, viewport);

            check(centreRay.has_value(), "No ray through the centre of the viewport");

            const auto forward = cameraEntity->GetTransform()->GetForward();

            check(glm::length(centreRay->Direction - forward) < 0.01f,
                "The ray through the centre of the viewport does not point where the camera does");

            const auto centrePick = Sculpting::PickTerrain(centreRay->Origin, centreRay->Direction);

            check(centrePick.has_value(), "A ray through the centre of the viewport met no terrain");
            check(centrePick->Terrain == terrain, "The pick found a terrain that is not the one in the scene");

            // Where the camera's forward meets the top of the step, worked out from the camera's
            // own position and direction rather than from anything the pick did. It pins the whole
            // landing point, not just its height, so a ray that is off by a rotation lands
            // somewhere this does not accept.
            const auto expected = centreRay->Origin +
                forward * ((centreRay->Origin.y - stepHeight) / -forward.y);

            check(glm::length(centrePick->LocalPosition - expected) < 0.1f,
                "A ray through the centre of the viewport did not land where the camera is pointing");

            // Off-centre rays have to move the hit the way the cursor moved. The offsets are small
            // enough that the hit stays on the raised half rather than running off the terrain.
            const auto rightRay = Sculpting::BuildCursorRay(camera, { viewport.x * 0.62f, viewport.y * 0.5f }, viewport);

            check(rightRay.has_value(), "No ray through a point right of the centre");

            const auto rightPick = Sculpting::PickTerrain(rightRay->Origin, rightRay->Direction);

            check(rightPick.has_value(), "A ray right of centre met no terrain");
            check(rightPick->LocalPosition.x > centrePick->LocalPosition.x + 1.f,
                "Moving the cursor right did not move the hit right");

            const auto lowerRay = Sculpting::BuildCursorRay(camera, { viewport.x * 0.5f, viewport.y * 0.62f }, viewport);

            check(lowerRay.has_value(), "No ray through a point below the centre");

            const auto lowerPick = Sculpting::PickTerrain(lowerRay->Origin, lowerRay->Direction);

            check(lowerPick.has_value(), "A ray below centre met no terrain");

            // Lower on the screen is nearer the camera, which from here is further along +z. A
            // flipped y gets this exactly backwards.
            check(lowerPick->LocalPosition.z > centrePick->LocalPosition.z + 1.f,
                "Moving the cursor down the screen did not move the hit towards the camera");

            check(!Sculpting::BuildCursorRay(nullptr, viewport * 0.5f, viewport).has_value(),
                "A null camera produced a ray");
            check(!Sculpting::BuildCursorRay(camera, viewport * 0.5f, { 0.f, 0.f }).has_value(),
                "A viewport with no size produced a ray");

            // Put the ground back, so the HTTP side still starts from a flat terrain.
            for (int z = terrain->GetSampleMin().y; z <= terrain->GetSampleMax().y; z++)
            {
                for (int x = 32; x <= terrain->GetSampleMax().x; x++)
                {
                    check(terrain->SetSampleHeight({ x, z }, 0.f), "Restoring the pick step failed");
                }
            }

            Pine::Entities::Delete(cameraEntity);
        }

        // The overlay the picture below looks for. Re-applied every frame from a pre-render
        // callback, because the Level viewport clears it at the end of every frame it is not
        // sculpting in - which, with no cursor to drive it, is every frame here.
        Pine::RenderManager::AddRenderCallback([](Pine::RenderingContext*, const Pine::RenderStage stage, float)
        {
            if (stage != Pine::RenderStage::PreRender)
            {
                return;
            }

            // By the path the asset registered under, which is project-relative - not the file
            // path it was created with.
            const auto terrain = Pine::Assets::Get<Pine::Terrain>("ground");

            if (terrain == nullptr)
            {
                return;
            }

            Pine::Rendering::TerrainRenderer::BrushOverlay overlay;

            overlay.Terrain = terrain->GetUId();
            // A corner of the terrain none of the strokes reach, so the ring sits on flat ground
            // and the script can tell its colour from the ground's without modelling either.
            overlay.Centre = { 52.f, 8.f };
            overlay.Radius = 8.f;
            overlay.RingWidth = 1.2f;

            Pine::Rendering::TerrainRenderer::SetBrushOverlay(overlay);
        });

        std::cout << "PASS(native): the ray march agrees with GetHeightAt straight down, obliquely "
                     "and at shared vertices,\n              misses answer empty, a height and a "
                     "weight rectangle each survive a read and a write\n              unchanged, "
                     "the brush shader version carries a uniform the default one does not,\n     "
                     "         and a cursor ray lands where the cursor points." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }

    Pine::Engine::Run();
