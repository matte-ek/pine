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
        const auto weightsAre = [&](const std::optional<Pine::Vector4f>& weights, const Pine::Vector4f& expected)
        {
            return weights.has_value() &&
                close(weights->x, expected.x) && close(weights->y, expected.y) &&
                close(weights->z, expected.z) && close(weights->w, expected.w);
        };

        // The share each layer takes at a sample, as a smooth bilinear field over the whole
        // terrain: layer 0 owns the (min, min) corner, 1 the far x corner, 2 the far z corner and
        // 3 the opposite one, with every point between them a mix of all four.
        //
        // A gradient rather than four painted quadrants, because it is what separates "the splat
        // map is being read" from "the splat map is being read at the right coordinate". Four
        // blocks would still look right if the lookup were scaled or offset wrongly - the blocks
        // would simply land somewhere else and nothing about the picture would say so. A field
        // that varies everywhere cannot survive that: it would come back tiled, clamped or
        // mirrored, and the script re-derives the expected value at a point rather than looking
        // for a shape.
        const auto bilinearWeights = [](const Pine::Vector2i sample, const Pine::Vector2i fieldSize)
        {
            const float u = static_cast<float>(sample.x) / static_cast<float>(fieldSize.x - 1);
            const float v = static_cast<float>(sample.y) / static_cast<float>(fieldSize.y - 1);

            return Pine::Vector4f((1.f - u) * (1.f - v), u * (1.f - v), (1.f - u) * v, u * v);
        };

        // Part 1 - the weight field and the coordinate it reaches the shader through. None of this
        // is visible over HTTP, and all of it is what the picture the script looks at is derived
        // from, so it is checked first and on its own.
        {
            auto scratch = new Pine::Terrain();

            scratch->SetChunkQuads(4);
            scratch->SetChunkSize(8.f);
            scratch->Resize({ 0, 0 }, { 2, 2 });

            const auto fieldSize = scratch->GetFieldSize();

            check(fieldSize == Pine::Vector2i(9, 9), "The scratch terrain is not the 9x9 field the rest of this assumes");

            // An unpainted terrain is entirely its first layer, not a surface with no layer at all.
            // The two would look identical on a terrain that has only layer 0 assigned and utterly
            // different on one that has four.
            for (int z = 0; z <= 8; z++)
            {
                for (int x = 0; x <= 8; x++)
                {
                    check(weightsAre(scratch->GetSampleWeights({ x, z }), { 1.f, 0.f, 0.f, 0.f }),
                        "A fresh terrain should be entirely layer 0");
                }
            }

            for (int layer = 0; layer < Pine::Terrain::MaximumLayerCount; layer++)
            {
                check(scratch->GetLayer(layer) == nullptr, "A fresh terrain should have no layer materials");
            }

            check(scratch->GetLayer(-1) == nullptr && scratch->GetLayer(Pine::Terrain::MaximumLayerCount) == nullptr,
                "A layer index outside the range should answer null rather than read past the array");

            // Written unnormalized, which is what a brush accumulating into one channel produces.
            check(scratch->SetSampleWeights({ 1, 1 }, { 2.f, 2.f, 0.f, 0.f }), "Writing an interior sample's weights was rejected");
            check(weightsAre(scratch->GetSampleWeights({ 1, 1 }), { 0.5f, 0.5f, 0.f, 0.f }),
                "Weights should be normalized on write");

            check(scratch->SetSampleWeights({ 2, 2 }, Pine::Vector4f(0.f)), "Writing all-zero weights was rejected");
            check(weightsAre(scratch->GetSampleWeights({ 2, 2 }), { 1.f, 0.f, 0.f, 0.f }),
                "All-zero weights should mean layer 0 rather than a sample with no layer");

            // Four independently rounded bytes land a step either side of 255, so an even split is
            // the case that catches a missing correction.
            check(scratch->SetSampleWeights({ 3, 3 }, Pine::Vector4f(1.f)), "Writing an even split was rejected");

            const auto even = scratch->GetSampleWeights({ 3, 3 });

            check(even.has_value() && close(even->x + even->y + even->z + even->w, 1.f, 0.005f),
                "A sample's weights should sum to one after being stored");

            check(!scratch->SetSampleWeights({ -1, 0 }, { 1.f, 0.f, 0.f, 0.f }),
                "A sample outside the field should be rejected");
            check(!scratch->GetSampleWeights({ 100, 0 }).has_value(),
                "A sample outside the field should have no weights");

            // The transform the shader applies to a chunk mesh's uv. A sample has to land on the
            // centre of its own texel: half a texel out in either direction and every weight the
            // ground shows is a blend of the wrong two samples.
            const auto transform = scratch->GetSplatTransform();

            const auto splatUv = [&](const Pine::Vector2i sample)
            {
                const auto uv = Pine::Vector2f(sample) * scratch->GetSampleSpacing();

                return Pine::Vector2f(uv.x * transform.x + transform.z, uv.y * transform.y + transform.w);
            };

            check(close(splatUv({ 0, 0 }).x, 0.5f / 9.f, 0.001f) && close(splatUv({ 0, 0 }).y, 0.5f / 9.f, 0.001f),
                "The first sample does not land on the centre of the first splat texel");
            check(close(splatUv({ 8, 8 }).x, 8.5f / 9.f, 0.001f) && close(splatUv({ 8, 8 }).y, 8.5f / 9.f, 0.001f),
                "The last sample does not land on the centre of the last splat texel");

            // Growing the terrain moves the weights with the heights. Painted ground coming back
            // bare would be as much of a loss as flattened ground, and both are addressed by
            // sample coordinate rather than by position in the field.
            check(scratch->SetSampleWeights({ 4, 4 }, { 0.f, 1.f, 0.f, 0.f }), "Writing the marker sample was rejected");

            scratch->Resize({ -1, -1 }, { 3, 3 });

            check(weightsAre(scratch->GetSampleWeights({ 4, 4 }), { 0.f, 1.f, 0.f, 0.f }),
                "A resize moved a sample's weights out from under its coordinate");
            check(weightsAre(scratch->GetSampleWeights({ -4, -4 }), { 1.f, 0.f, 0.f, 0.f }),
                "Ground added by a resize should start on layer 0");

            // Changing the sample density has no sensible mapping onto the existing weights, for
            // the same reason it has none onto the existing heights.
            scratch->SetChunkQuads(8);

            check(weightsAre(scratch->GetSampleWeights({ 4, 4 }), { 1.f, 0.f, 0.f, 0.f }),
                "Changing the resolution should reset the weights, as it resets the heights");

            scratch->Dispose();

            delete scratch;
        }

        // Part 2 - what survives being written out and read back. The layers are stored as a list
        // of UIds and the weights next to the heights, so a terrain that drops either would look
        // correct for the whole session that painted it and wrong on the next start.
        {
            auto material = dynamic_cast<Pine::Material*>(
                Pine::Assets::CreateAsset(Pine::AssetType::Material, "projects/terrain/assets/round-trip"));

            Pine::Assets::Internal::RegisterAsset(material);

            auto original = new Pine::Terrain();

            original->SetChunkQuads(4);
            original->SetChunkSize(8.f);
            original->Resize({ 0, 0 }, { 1, 1 });
            original->SetLayer(2, material);

            check(original->SetSampleWeights({ 1, 2 }, { 0.f, 0.25f, 0.75f, 0.f }), "Writing the round-trip sample was rejected");

            auto reloaded = dynamic_cast<Pine::Terrain*>(Pine::Asset::Load(original->Save()));

            check(reloaded != nullptr, "A saved terrain did not load back");
            check(reloaded->GetLayer(2) == material, "A saved terrain lost the material of layer 2");
            check(reloaded->GetLayer(0) == nullptr, "A saved terrain invented a material for an empty layer");
            check(weightsAre(reloaded->GetSampleWeights({ 1, 2 }), { 0.f, 0.25f, 0.75f, 0.f }),
                "A saved terrain lost its layer weights");

            reloaded->Dispose();
            original->Dispose();

            delete reloaded;
            delete original;
        }

        // Part 3 - the scene the HTTP side looks at.
        //
        // Flat ground on purpose: every fragment then takes the same light, so a colour difference
        // anywhere in the frame is the layer blend and nothing else. On sloped ground the shading
        // and the blend would be mixed together in every pixel and neither could be read off.
        {
            // Pure primaries plus white, which is what lets the script name the layer a pixel is
            // mostly made of. They are untextured, so the diffuse colour reaches the blend
            // directly rather than through whatever a project's textures happen to look like.
            const Pine::Vector3f layerColors[] = {
                { 1.f, 0.f, 0.f },
                { 0.f, 1.f, 0.f },
                { 0.f, 0.f, 1.f },
                { 1.f, 1.f, 1.f }
            };

            auto terrain = dynamic_cast<Pine::Terrain*>(
                Pine::Assets::CreateAsset(Pine::AssetType::Terrain, "projects/terrain/assets/ground"));

            Pine::Assets::Internal::RegisterAsset(terrain);

            for (int layer = 0; layer < Pine::Terrain::MaximumLayerCount; layer++)
            {
                auto material = dynamic_cast<Pine::Material*>(Pine::Assets::CreateAsset(
                    Pine::AssetType::Material, "projects/terrain/assets/layer-" + std::to_string(layer)));

                Pine::Assets::Internal::RegisterAsset(material);

                material->SetDiffuseColor(layerColors[layer]);

                terrain->SetLayer(layer, material);
            }

            // Four chunks across, so the gradient crosses several chunk edges and also varies
            // within every one of them - "four layers blend across a chunk" is the thing this unit
            // is for, and a terrain of one chunk would not show it.
            terrain->SetChunkQuads(16);
            terrain->SetChunkSize(16.f);
            terrain->Resize({ 0, 0 }, { 4, 4 });

            const auto fieldSize = terrain->GetFieldSize();

            for (int z = 0; z < fieldSize.y; z++)
            {
                for (int x = 0; x < fieldSize.x; x++)
                {
                    check(terrain->SetSampleWeights({ x, z }, bilinearWeights({ x, z }, fieldSize)),
                        "Painting the weight field fell outside the terrain");
                }
            }

            auto terrainEntity = Pine::Entities::Create("Terrain");

            terrainEntity->AddComponent<Pine::TerrainRendererComponent>()->SetTerrain(terrain);

            // Straight down, so every part of the flat ground is lit identically and the only
            // thing left varying across the frame is the blend.
            auto sunEntity = Pine::Entities::Create("Sun");

            sunEntity->GetTransform()->SetEulerAngles(Pine::Vector3f(-90.f, 0.f, 0.f));

            auto sun = sunEntity->AddComponent<Pine::Light>();

            sun->SetLightType(Pine::LightType::Directional);
            sun->SetLightIntensity(1.f);
            sun->SetCastShadows(false);
        }

        std::cout << "PASS(native): unpainted ground is layer 0, weights normalize on write and "
                     "survive a resize, a sample lands on its own splat texel, and layers and "
                     "weights both survive a save and load." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }

    Pine::Engine::Run();
