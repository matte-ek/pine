    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };
        const auto close = [](float value, float expected, float tolerance = 0.01f)
        {
            return std::abs(value - expected) <= tolerance;
        };

        // Part 1 - what only holds inside the process: the chunk meshes themselves, the dirty
        // bookkeeping that decides which get rebuilt, and the triangulation convention the mesh
        // generator shares with GetHeightAt. None of it is visible over HTTP. This terrain is
        // deliberately not the one the scene below renders, so the render path still has dirty
        // chunks to build and /terrain can prove that it did.
        {
            auto scratch = new Pine::Terrain();

            scratch->SetChunkQuads(8);
            scratch->SetChunkSize(8.f);
            scratch->Resize({ 0, 0 }, { 2, 2 });

            const auto quads = scratch->GetChunkQuads();

            // 6 indices per quad of the grid, plus the skirt: a ring of 4 * quads segments, each
            // two triangles. Expressed as a formula rather than a measured number so a level that
            // silently loses its skirt fails here instead of rendering a crack.
            const auto expectedIndices = [](const int levelQuads)
            {
                return static_cast<std::uint32_t>(6 * levelQuads * levelQuads + 24 * levelQuads);
            };

            for (const auto& chunk : scratch->GetChunks())
            {
                check(chunk.IsDirty, "A freshly built chunk should start dirty");
                check(scratch->GetChunkMesh(chunk, 0) == nullptr, "A chunk should carry no mesh before a rebuild");
            }

            scratch->RebuildDirtyChunkMeshes();

            for (const auto& chunk : scratch->GetChunks())
            {
                check(!chunk.IsDirty, "RebuildDirtyChunkMeshes left a chunk dirty");
                check(static_cast<int>(chunk.LodMeshes.size()) == scratch->GetLodCount(),
                    "RebuildDirtyChunkMeshes built the wrong number of detail levels");

                for (int level = 0; level < scratch->GetLodCount(); level++)
                {
                    const auto mesh = scratch->GetChunkMesh(chunk, level);

                    check(mesh != nullptr, "RebuildDirtyChunkMeshes left a detail level without a mesh");
                    check(mesh->HasElementBuffer(), "A chunk mesh should be indexed");
                    check(mesh->GetRenderCount() == expectedIndices(quads >> level),
                        "Detail level " + std::to_string(level) + " should carry 6 indices per quad plus a skirt, got "
                            + std::to_string(mesh->GetRenderCount()));

                    // The ground's indices come first and the skirt's after them, which is what
                    // lets a pass that must not let the skirt occlude anything - a shadow view -
                    // draw a prefix of the buffer rather than need a second mesh. If the two ever
                    // stop being contiguous in that order, this count silently starts cutting the
                    // ground short instead.
                    const int levelQuads = quads >> level;

                    check(scratch->GetChunkGroundIndexCount(level) ==
                              static_cast<std::uint32_t>(levelQuads) * levelQuads * 6,
                        "The ground index count of level " + std::to_string(level) + " is not 6 per quad");

                    check(scratch->GetChunkGroundIndexCount(level) < mesh->GetRenderCount(),
                        "The ground index count should stop short of the skirt the mesh ends with");

                    // The mesh is chunk-local and the bounds are terrain-local, so the two differ
                    // by the chunk's horizontal corner and by nothing else. The skirt hangs below
                    // the bounds and deliberately does not widen them - culling tests the ground,
                    // not the rim that hides the crack under it.
                    const auto corner = Pine::Vector3f(chunk.BoundsMin.x, 0.f, chunk.BoundsMin.z);

                    check(mesh->GetBoundingBoxMin() == chunk.BoundsMin - corner &&
                          mesh->GetBoundingBoxMax() == chunk.BoundsMax - corner,
                        "A chunk mesh's AABB does not match the chunk's bounds");
                }
            }

            // Levels exist only while halving leaves a whole number of quads and the result stays
            // above the coarseness floor, so the count follows the chunk's resolution rather than
            // being a constant every terrain pays for.
            check(scratch->GetLodCount() == 1, "An 8-quad chunk is already at the coarsest level and should have one");

            scratch->SetChunkQuads(64);

            check(scratch->GetLodCount() == Pine::Terrain::MaximumLodCount,
                "A 64-quad chunk should carry all four levels, got " + std::to_string(scratch->GetLodCount()));

            scratch->RebuildDirtyChunkMeshes();

            // A coarser level keeps every 2^l-th sample, so it is a quarter of the grid below it -
            // and its outer vertices are still the same samples, which is what lets two neighbours
            // at the same level meet exactly.
            for (const auto& chunk : scratch->GetChunks())
            {
                for (int level = 1; level < scratch->GetLodCount(); level++)
                {
                    check(scratch->GetChunkMesh(chunk, level)->GetRenderCount() < scratch->GetChunkMesh(chunk, level - 1)->GetRenderCount(),
                        "Detail level " + std::to_string(level) + " is not cheaper than the one below it");
                }

                check(scratch->GetChunkMesh(chunk, 99) == scratch->GetChunkMesh(chunk, scratch->GetLodCount() - 1),
                    "A level past the coarsest should clamp onto it rather than read past the array");
            }

            scratch->SetChunkQuads(8);
            scratch->Resize({ 0, 0 }, { 2, 2 });
            scratch->RebuildDirtyChunkMeshes();

            // An edited sample dirties the chunk it sits in - and, when it sits on a chunk edge,
            // the chunk on the other side of that edge too. Both read the same sample out of the
            // shared field, so both of their meshes are now stale.
            const auto dirtyChunks = [&]
            {
                std::vector<Pine::Vector2i> result;

                for (const auto& chunk : scratch->GetChunks())
                {
                    if (chunk.IsDirty) result.push_back(chunk.Coordinate);
                }

                return result;
            };

            check(scratch->SetSampleHeight({ 4, 4 }, 3.f), "Writing an interior sample was rejected");
            check(dirtyChunks() == std::vector<Pine::Vector2i>{ { 0, 0 } },
                "An interior sample should dirty exactly the chunk it sits in");

            scratch->RebuildDirtyChunkMeshes();

            check(scratch->SetSampleHeight({ 8, 4 }, 5.f), "Writing an edge sample was rejected");
            check(dirtyChunks() == (std::vector<Pine::Vector2i>{ { 0, 0 }, { 1, 0 } }),
                "A sample on a chunk edge should dirty the chunks on both sides of it");

            check(!scratch->SetSampleHeight({ 17, 0 }, 1.f), "A sample outside the field should be rejected");

            // The quad with its low corner at (0, 0) is split from (0, 1) to (1, 0). The first
            // triangle is therefore flat at 0 across its whole area while the second rises to the
            // far corner - which a bilinear read of the same four samples would not reproduce, so
            // this fails if the query ever stops agreeing with how the mesh is triangulated.
            scratch->SetSampleHeight({ 0, 0 }, 0.f);
            scratch->SetSampleHeight({ 1, 0 }, 0.f);
            scratch->SetSampleHeight({ 0, 1 }, 0.f);
            scratch->SetSampleHeight({ 1, 1 }, 60.f);

            const auto heightAt = [&](float x, float z)
            {
                const auto value = scratch->GetHeightAt(x, z);

                check(value.has_value(), "GetHeightAt found nothing inside the terrain");

                return *value;
            };

            check(close(heightAt(0.25f, 0.25f), 0.f), "The first triangle of a quad is not flat where it should be");
            check(close(heightAt(0.75f, 0.75f), 30.f, 0.05f), "The second triangle of a quad does not interpolate across its own plane");

            check(!scratch->GetHeightAt(-1.f, 0.f).has_value(), "A point off the terrain should have no height");

            scratch->Dispose();

            delete scratch;
        }

        // Part 2 - the scene the HTTP side looks at. It cannot be built over /edit, which has no
        // TerrainRenderer operation, and it deliberately uses no project assets: the engine's own
        // default material keeps the check independent of whatever textures a project happens to
        // ship, and an untextured surface shows the vertex normals directly.
        {
            auto terrain = dynamic_cast<Pine::Terrain*>(
                Pine::Assets::CreateAsset(Pine::AssetType::Terrain, "projects/terrain/assets/ground"));

            Pine::Assets::Internal::RegisterAsset(terrain);

            terrain->SetLayer(0, Pine::Assets::Get<Pine::Material>("engine/materials/default"));

            // Wider than the default grid so that standing on it puts several chunks inside the
            // finest level and the rest spread over the coarser ones. A terrain that fits inside
            // one switch distance draws entirely at one level, and would check nothing about LOD
            // or about the skirts that hide the seam between two levels.
            terrain->Resize({ 0, 0 }, { 8, 8 });
            terrain->GenerateFromNoise();

            auto terrainEntity = Pine::Entities::Create("Terrain");

            terrainEntity->AddComponent<Pine::TerrainRendererComponent>()->SetTerrain(terrain);

            auto sunEntity = Pine::Entities::Create("Sun");

            sunEntity->GetTransform()->SetEulerAngles(Pine::Vector3f(-50.f, 30.f, 0.f));

            auto sun = sunEntity->AddComponent<Pine::Light>();

            sun->SetLightType(Pine::LightType::Directional);
            sun->SetLightIntensity(1.f);
        }

        std::cout << "PASS(native): per-level chunk meshes and their skirts, index counts, mesh bounds, "
                     "level counts and clamping, dirty propagation across a shared edge and the quad "
                     "triangulation convention." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }

    Pine::Engine::Run();
