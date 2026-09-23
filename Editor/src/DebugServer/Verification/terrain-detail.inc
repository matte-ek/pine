    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        const auto paintEverything = [](Pine::Terrain* terrain, const Pine::Vector4f& weights)
        {
            const auto sampleMin = terrain->GetSampleMin();
            const auto sampleMax = terrain->GetSampleMax();

            for (int z = sampleMin.y; z <= sampleMax.y; z++)
            {
                for (int x = sampleMin.x; x <= sampleMax.x; x++)
                {
                    terrain->SetSampleWeights({ x, z }, weights);
                }
            }
        };

        const auto findChunk = [](Pine::Terrain* terrain, const Pine::Vector2i coordinate) -> const Pine::TerrainChunk&
        {
            for (const auto& chunk : terrain->GetChunks())
            {
                if (chunk.Coordinate == coordinate)
                {
                    return chunk;
                }
            }

            throw std::runtime_error("The probe asked for a chunk the terrain does not have");
        };

        const auto samePlacements = [](const std::vector<Pine::TerrainDetailInstance>& a,
                                       const std::vector<Pine::TerrainDetailInstance>& b)
        {
            if (a.size() != b.size())
            {
                return false;
            }

            for (std::size_t i = 0; i < a.size(); i++)
            {
                if (a[i].Position != b[i].Position || a[i].Scale != b[i].Scale || a[i].Yaw != b[i].Yaw)
                {
                    return false;
                }
            }

            return true;
        };

        const auto contains = [](const std::vector<Pine::TerrainDetailInstance>& set, const Pine::TerrainDetailInstance& instance)
        {
            for (const auto& candidate : set)
            {
                if (candidate.Position == instance.Position && candidate.Scale == instance.Scale && candidate.Yaw == instance.Yaw)
                {
                    return true;
                }
            }

            return false;
        };

        // Part 1 - placement. 2x2 chunks of 8 quads at 8 units, so one sample per unit and 64
        // square units a chunk. A density of 1 is then 64 candidates a chunk, every one of them
        // kept where the layer is painted at full weight.
        {
            auto scratch = new Pine::Terrain();

            scratch->SetChunkQuads(8);
            scratch->SetChunkSize(8.f);
            scratch->Resize({ 0, 0 }, { 2, 2 });

            check(scratch->GetDetailTypes().empty(), "A fresh terrain should have no detail types");
            check(scratch->GenerateDetailInstances(findChunk(scratch, { 0, 0 }), 0).empty(),
                "A detail type index the terrain does not have should generate nothing");

            Pine::TerrainDetailType grass;

            grass.Layer = 1;
            grass.Density = 1.f;
            grass.ScaleMin = 0.5f;
            grass.ScaleMax = 1.5f;

            scratch->SetDetailTypes({ grass });

            // Unpainted ground is entirely layer 0, so a layer-1 detail grows nowhere yet.
            for (const auto& chunk : scratch->GetChunks())
            {
                check(scratch->GenerateDetailInstances(chunk, 0).empty(), "Detail grew on ground its layer is not painted on");
            }

            paintEverything(scratch, { 0.f, 1.f, 0.f, 0.f });

            for (const auto& chunk : scratch->GetChunks())
            {
                const auto placements = scratch->GenerateDetailInstances(chunk, 0);

                check(placements.size() == 64, "Full weight at density 1 should keep all 64 candidates of an 8x8 chunk, kept " +
                    std::to_string(placements.size()));

                const auto low = Pine::Vector2f(chunk.Coordinate) * 8.f;

                for (const auto& placement : placements)
                {
                    check(placement.Position.x >= low.x && placement.Position.x < low.x + 8.f &&
                          placement.Position.z >= low.y && placement.Position.z < low.y + 8.f,
                        "A placement landed outside its own chunk");

                    const auto ground = scratch->GetHeightAt(placement.Position.x, placement.Position.z);

                    check(ground.has_value() && std::abs(ground.value() - placement.Position.y) < 0.0001f,
                        "A placement is not standing on the ground");

                    check(placement.Scale >= 0.5f && placement.Scale <= 1.5f, "A placement's scale is outside the detail type's range");
                    check(placement.Yaw >= 0.f && placement.Yaw < 6.2832f, "A placement's yaw is outside one turn");
                }
            }

            // Placements stand on sculpted ground too, not only on flat ground.
            check(scratch->SetSampleHeight({ 3, 3 }, 2.f), "Raising a sample was rejected");

            for (const auto& placement : scratch->GenerateDetailInstances(findChunk(scratch, { 0, 0 }), 0))
            {
                const auto ground = scratch->GetHeightAt(placement.Position.x, placement.Position.z);

                check(ground.has_value() && std::abs(ground.value() - placement.Position.y) < 0.0001f,
                    "A placement is not standing on sculpted ground");
            }

            const auto& chunkA = findChunk(scratch, { 0, 0 });
            const auto& chunkB = findChunk(scratch, { 1, 0 });
            const auto& chunkC = findChunk(scratch, { 1, 1 });

            const auto before = scratch->GenerateDetailInstances(chunkA, 0);

            check(samePlacements(before, scratch->GenerateDetailInstances(chunkA, 0)),
                "The same terrain generated different placements twice in a row");

            // Unpainting a corner patch of chunk (0, 0) may only remove placements - the ones that
            // survive must be exactly where they were. A 5x5 patch, so the ~20 square units it
            // strips are sure to have held some.
            for (int z = 0; z <= 4; z++)
            {
                for (int x = 0; x <= 4; x++)
                {
                    check(scratch->SetSampleWeights({ x, z }, { 1.f, 0.f, 0.f, 0.f }), "Unpainting a sample was rejected");
                }
            }

            const auto afterUnpaint = scratch->GenerateDetailInstances(chunkA, 0);

            check(afterUnpaint.size() < before.size(), "Unpainting part of a chunk did not remove anything from it");

            for (const auto& placement : afterUnpaint)
            {
                check(contains(before, placement), "Repainting moved a placement instead of only removing some");
            }

            // Part 1b - the revisions a renderer caches placements against.
            const auto revisionA = chunkA.DetailRevision;
            const auto revisionB = chunkB.DetailRevision;
            const auto revisionC = chunkC.DetailRevision;

            // Sample (12, 12) is inside chunk (1, 1) only.
            check(scratch->SetSampleWeights({ 12, 12 }, { 1.f, 0.f, 0.f, 0.f }), "Painting an interior sample was rejected");

            check(chunkC.DetailRevision != revisionC, "Repainting chunk (1, 1) did not change its detail revision");
            check(chunkA.DetailRevision == revisionA && chunkB.DetailRevision == revisionB,
                "Repainting chunk (1, 1) changed the revision of chunks it does not cover");

            // Sample (8, 3) is on the edge between chunks (0, 0) and (1, 0), so it belongs to both.
            check(scratch->SetSampleWeights({ 8, 3 }, { 1.f, 0.f, 0.f, 0.f }), "Painting an edge sample was rejected");

            check(chunkA.DetailRevision != revisionA && chunkB.DetailRevision != revisionB,
                "A repaint on a chunk edge should change the revision of the chunks on both sides");

            const auto revisionBeforeHeight = chunkC.DetailRevision;

            check(scratch->SetSampleHeight({ 12, 13 }, 1.f), "Raising an interior sample was rejected");
            check(chunkC.DetailRevision != revisionBeforeHeight, "Moving the ground did not change the detail revision");

            // A weight rectangle, which is what the editor's brush writes.
            const Pine::TerrainSampleRect rect = { { 10, 10 }, { 11, 11 } };
            const auto revisionBeforeRect = chunkC.DetailRevision;

            check(scratch->SetSampleWeightRect(rect, scratch->GetSampleWeightRect(rect)), "Writing a weight rectangle back was refused");
            check(chunkC.DetailRevision != revisionBeforeRect, "Writing a weight rectangle did not change the detail revision");

            const auto revisionsBeforeTypes = std::vector<std::uint64_t>{
                chunkA.DetailRevision, chunkB.DetailRevision, chunkC.DetailRevision };

            scratch->SetDetailTypes(scratch->GetDetailTypes());

            check(chunkA.DetailRevision != revisionsBeforeTypes[0] &&
                  chunkB.DetailRevision != revisionsBeforeTypes[1] &&
                  chunkC.DetailRevision != revisionsBeforeTypes[2],
                "Setting the detail types should change every chunk's revision");

            // Revisions come from one counter, so a second terrain never repeats one - otherwise a
            // renderer holding placements for the first could mistake them for the second's.
            auto other = new Pine::Terrain();

            for (const auto& otherChunk : other->GetChunks())
            {
                for (const auto& chunk : scratch->GetChunks())
                {
                    check(otherChunk.DetailRevision != chunk.DetailRevision, "Two terrains handed out the same detail revision");
                }
            }

            other->Dispose();
            delete other;

            // Out-of-range values are pulled back rather than stored.
            Pine::TerrainDetailType broken;

            broken.Layer = 9;
            broken.Density = -3.f;
            broken.ScaleMin = 2.f;
            broken.ScaleMax = 1.f;
            broken.DrawDistance = -5.f;

            scratch->SetDetailTypes({ broken });

            const auto& sanitized = scratch->GetDetailTypes().front();

            check(sanitized.Layer == Pine::Terrain::MaximumLayerCount - 1, "A layer past the last channel was not clamped onto it");
            check(sanitized.Density == 0.f, "A negative density was not clamped to zero");
            check(sanitized.ScaleMax >= sanitized.ScaleMin, "An inverted scale range was not put in order");
            check(sanitized.DrawDistance == 0.f, "A negative draw distance was not clamped to zero");
            check(scratch->GenerateDetailInstances(findChunk(scratch, { 0, 0 }), 0).empty(), "Zero density still generated placements");

            scratch->Dispose();
            delete scratch;
        }

        // Part 2 - what survives being written out and read back.
        {
            auto cube = Pine::Assets::Get<Pine::Model>("engine/primitive/cube");

            check(cube != nullptr, "The engine cube is missing");

            auto original = new Pine::Terrain();

            Pine::TerrainDetailType fern;

            fern.DetailModel = cube;
            fern.Layer = 2;
            fern.Density = 0.25f;
            fern.ScaleMin = 0.75f;
            fern.ScaleMax = 1.25f;
            fern.DrawDistance = 33.f;

            original->SetDetailTypes({ fern, Pine::TerrainDetailType() });

            auto reloaded = dynamic_cast<Pine::Terrain*>(Pine::Asset::Load(original->Save()));

            check(reloaded != nullptr, "A saved terrain did not load back");
            check(reloaded->GetDetailTypes().size() == 2, "A saved terrain did not keep both detail types");

            const auto& loaded = reloaded->GetDetailTypes().front();

            check(loaded.DetailModel.Get() == cube, "A saved terrain lost its detail model");
            check(loaded.Layer == 2 && loaded.Density == 0.25f && loaded.ScaleMin == 0.75f &&
                  loaded.ScaleMax == 1.25f && loaded.DrawDistance == 33.f,
                "A saved terrain changed a detail type's values");
            check(reloaded->GetDetailTypes().back().DetailModel.Get() == nullptr,
                "A detail type saved without a model came back with one");

            reloaded->Dispose();
            original->Dispose();

            delete reloaded;
            delete original;
        }

        // Part 3 - the scene the HTTP side looks at. Flat ground in two dark layers, the x < 32
        // half painted into layer 1, and bright magenta cubes growing on layer 1 only. The script
        // checks where the cubes land on screen, using placements this prints.
        {
            auto terrain = dynamic_cast<Pine::Terrain*>(
                Pine::Assets::CreateAsset(Pine::AssetType::Terrain, "projects/terrain/assets/ground"));

            Pine::Assets::Internal::RegisterAsset(terrain);

            for (int layer = 0; layer < 2; layer++)
            {
                auto material = dynamic_cast<Pine::Material*>(Pine::Assets::CreateAsset(
                    Pine::AssetType::Material, "projects/terrain/assets/layer-" + std::to_string(layer)));

                Pine::Assets::Internal::RegisterAsset(material);

                material->SetDiffuseColor(Pine::Vector3f(0.12f));

                terrain->SetLayer(layer, material);
            }

            terrain->SetChunkQuads(16);
            terrain->SetChunkSize(16.f);
            terrain->Resize({ 0, 0 }, { 4, 4 });

            const auto fieldSize = terrain->GetFieldSize();

            for (int z = 0; z < fieldSize.y; z++)
            {
                for (int x = 0; x < fieldSize.x; x++)
                {
                    const bool isPainted = x < 32;

                    terrain->SetSampleWeights({ x, z }, isPainted ? Pine::Vector4f(0.f, 1.f, 0.f, 0.f)
                                                                  : Pine::Vector4f(1.f, 0.f, 0.f, 0.f));
                }
            }

            // Discard and two-sided, which is what real foliage uses, so the scene exercises the
            // detail shader's Discard version and the culling switch rather than the plain path.
            auto detailMaterial = dynamic_cast<Pine::Material*>(Pine::Assets::CreateAsset(
                Pine::AssetType::Material, "projects/terrain/assets/detail-magenta"));

            Pine::Assets::Internal::RegisterAsset(detailMaterial);

            detailMaterial->SetDiffuseColor({ 1.f, 0.f, 1.f });
            detailMaterial->SetRenderingMode(Pine::MaterialRenderingMode::Discard);
            detailMaterial->SetRenderFace(Pine::MaterialRenderFace::Both);

            // Only in memory, and only in the probe's disposable data directory.
            auto cube = Pine::Assets::Get<Pine::Model>("engine/primitive/cube");

            for (const auto mesh : cube->GetMeshes())
            {
                mesh->SetMaterial(detailMaterial);
            }

            Pine::TerrainDetailType detail;

            detail.DetailModel = cube;
            detail.Layer = 1;
            detail.Density = 0.1f;
            detail.ScaleMin = 0.6f;
            detail.ScaleMax = 0.6f;
            detail.DrawDistance = 150.f;

            terrain->SetDetailTypes({ detail });

            std::size_t total = 0;
            std::vector<Pine::TerrainDetailInstance> samples;

            for (const auto& chunk : terrain->GetChunks())
            {
                const auto placements = terrain->GenerateDetailInstances(chunk, 0);

                total += placements.size();

                for (const auto& placement : placements)
                {
                    // Well inside the painted half and away from the rim, so no sampled block can
                    // catch unpainted ground or the terrain's edge.
                    if (placement.Position.x > 4.f && placement.Position.x < 26.f &&
                        placement.Position.z > 6.f && placement.Position.z < 58.f && samples.size() < 8)
                    {
                        samples.push_back(placement);
                    }
                }
            }

            check(total > 100, "The scene grew too little detail to look at: " + std::to_string(total));
            check(samples.size() == 8, "Not enough placements inside the painted half to sample");

            std::cout << "DETAIL_TOTAL " << total << std::endl;

            // The top of each sampled cube, which is what a camera looking down sees.
            const float top = cube->GetBoundingBoxMax().y * detail.ScaleMax;

            for (const auto& sample : samples)
            {
                std::cout << "DETAIL_SAMPLE " << sample.Position.x << " " << sample.Position.y + top << " "
                          << sample.Position.z << std::endl;
            }

            auto terrainEntity = Pine::Entities::Create("Terrain");

            terrainEntity->AddComponent<Pine::TerrainRendererComponent>()->SetTerrain(terrain);

            auto sunEntity = Pine::Entities::Create("Sun");

            sunEntity->GetTransform()->SetEulerAngles(Pine::Vector3f(-90.f, 0.f, 0.f));

            auto sun = sunEntity->AddComponent<Pine::Light>();

            sun->SetLightType(Pine::LightType::Directional);
            sun->SetLightIntensity(1.f);
            sun->SetCastShadows(false);
        }

        std::cout << "PASS(native): detail grows only where its layer is painted, stands on the ground, "
                     "is deterministic and only loses placements to a repaint, revisions follow every "
                     "edit and are unique across terrains, and detail types survive a save and load." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }

    Pine::Engine::Run();