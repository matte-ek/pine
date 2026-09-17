    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        // Part 1 - the layout bounds. Both setters are driven straight from unclamped editor
        // fields, so a grid too large to hold, or large enough to overflow the int its field size
        // is worked out in, has to be refused rather than allocated.
        {
            auto scratch = new Pine::Terrain();

            scratch->SetChunkQuads(8);
            scratch->SetChunkSize(8.f);
            scratch->Resize({ 0, 0 }, { 2, 2 });

            const auto fieldSize = scratch->GetFieldSize();

            scratch->Resize({ 0, 0 }, { 4096, 4096 });
            check(scratch->GetChunkCount() == Pine::Vector2i(2, 2),
                "An oversized terrain resize was applied");

            // Chunks enough that the quad count across the axis overflows an int, which is the
            // case that used to wrap into a field size that looked perfectly reasonable.
            scratch->Resize({ 0, 0 }, { 400000000, 1 });
            check(scratch->GetChunkCount() == Pine::Vector2i(2, 2),
                "A terrain resize whose quad count overflows an int was applied");

            scratch->Resize({ 1000000000, 0 }, { 2, 2 });
            check(scratch->GetChunkOrigin() == Pine::Vector2i(0, 0),
                "A terrain origin far outside the coordinate range was applied");

            scratch->SetChunkQuads(8192);
            check(scratch->GetChunkQuads() == 8,
                "An oversized terrain resolution change was applied");

            check(scratch->GetFieldSize() == fieldSize,
                "A refused terrain layout change resized the field anyway");

            // A bound rather than a ban: a large grid that can still be held is accepted.
            scratch->Resize({ -8, -8 }, { 16, 16 });
            check(scratch->GetChunkCount() == Pine::Vector2i(16, 16),
                "A terrain resize well inside the bounds was refused");

            scratch->Dispose();

            delete scratch;
        }

        // Part 2 - the height range, which is only the encoding the samples are stored in, so
        // changing it re-encodes the whole field and the ground has to stay where the author put
        // it while that happens.
        {
            auto scratch = new Pine::Terrain();

            scratch->SetChunkQuads(8);
            scratch->SetChunkSize(8.f);
            scratch->Resize({ 0, 0 }, { 1, 1 });

            scratch->SetSampleHeight({ 2, 3 }, 10.f);
            scratch->SetSampleHeight({ 4, 5 }, -20.f);

            const auto raised = scratch->GetSampleHeight({ 2, 3 });
            const auto lowered = scratch->GetSampleHeight({ 4, 5 });

            check(raised.has_value() && lowered.has_value(), "A sculpted sample read back as nothing");

            // Four times the range, so every sample lands on a different stored value than the one
            // it held and a re-encode that quietly did nothing cannot pass by holding still.
            scratch->SetHeightRange(-256.f, 256.f);

            check(scratch->GetHeightMin() == -256.f && scratch->GetHeightMax() == 256.f,
                "A valid terrain height range change was refused");

            // One step of the wider encoding is 512/65535, so a height this close is the same
            // height rather than a sample that moved.
            check(std::abs(*scratch->GetSampleHeight({ 2, 3 }) - *raised) < 0.01f,
                "Widening the height range moved a raised sample");
            check(std::abs(*scratch->GetSampleHeight({ 4, 5 }) - *lowered) < 0.01f,
                "Widening the height range moved a lowered sample");
            check(std::abs(*scratch->GetSampleHeight({ 0, 0 })) < 0.01f,
                "Widening the height range moved a sample that was left flat");

            // Narrowing clamps whatever falls outside, which is the documented cost of storing a
            // height as a fraction of a range.
            scratch->SetHeightRange(-5.f, 5.f);

            check(std::abs(*scratch->GetSampleHeight({ 2, 3 }) - 5.f) < 0.01f,
                "Narrowing the height range did not clamp a sample above it");
            check(std::abs(*scratch->GetSampleHeight({ 4, 5 }) + 5.f) < 0.01f,
                "Narrowing the height range did not clamp a sample below it");

            scratch->Dispose();

            delete scratch;
        }

        // Part 3 - the noise bands through a save and a load. They are written as a list of data
        // blocks rather than as BandCount * four flat fields, so what this checks is that a band's
        // four values come back on the band they were written on.
        {
            auto scratch = new Pine::Terrain();

            scratch->SetChunkQuads(8);
            scratch->SetChunkSize(8.f);
            scratch->Resize({ -1, -1 }, { 2, 2 });

            Pine::TerrainNoiseSettings noise;

            noise.Seed = 4242;

            // A different value in every field of every band, so a band read back off by one - or
            // a field read into its neighbour - shows up rather than matching by luck.
            for (int index = 0; index < Pine::TerrainNoiseSettings::BandCount; index++)
            {
                noise.Bands[index].CoordinateScale = 0.01f * static_cast<float>(index + 1);
                noise.Bands[index].Octaves = 2 + index;
                noise.Bands[index].Scale = 3.f + static_cast<float>(index);
                noise.Bands[index].Cutoff = -5.f + static_cast<float>(index);
            }

            scratch->SetNoiseSettings(noise);
            scratch->GenerateFromNoise();

            const auto heights = scratch->GetHeightField();

            // ReLoad() writes the asset out and reads it straight back in, which is the whole
            // format in both directions.
            scratch->ReLoad();

            const auto& reloaded = scratch->GetNoiseSettings();

            check(reloaded.Seed == 4242, "The noise seed did not survive a save and a load");

            for (int index = 0; index < Pine::TerrainNoiseSettings::BandCount; index++)
            {
                const auto& band = reloaded.Bands[index];

                check(std::abs(band.CoordinateScale - 0.01f * static_cast<float>(index + 1)) < 1e-6f,
                    "A noise band's coordinate scale did not survive a save and a load");
                check(band.Octaves == 2 + index,
                    "A noise band's octave count did not survive a save and a load");
                check(std::abs(band.Scale - (3.f + static_cast<float>(index))) < 1e-6f,
                    "A noise band's scale did not survive a save and a load");
                check(std::abs(band.Cutoff - (-5.f + static_cast<float>(index))) < 1e-6f,
                    "A noise band's cutoff did not survive a save and a load");
            }

            check(scratch->GetHeightField() == heights,
                "The height field did not survive a save and a load");
            check(scratch->GetChunkQuads() == 8 &&
                  scratch->GetChunkCount() == Pine::Vector2i(2, 2) &&
                  scratch->GetChunkOrigin() == Pine::Vector2i(-1, -1),
                "The layout did not survive a save and a load");

            scratch->Dispose();

            delete scratch;
        }

        // Part 4 - a file whose layout cannot be indexed. Asset::Load keeps the registered asset
        // when LoadAssetData refuses it, so a terrain that is already loaded and drawing has to
        // come through a refused load exactly as it was rather than half updated - its layout
        // describing the new file while its field still holds the old one's samples.
        {
            auto terrain = dynamic_cast<Pine::Terrain*>(
                Pine::Assets::CreateAsset(Pine::AssetType::Terrain, "projects/terrain/assets/refused"));

            check(terrain != nullptr, "Could not create a terrain asset");

            // CreateAsset does not do this, and without it the asset is invisible to the lookup by
            // UId that decides whether a load is replacing something.
            Pine::Assets::Internal::RegisterAsset(terrain);

            terrain->SetChunkQuads(8);
            terrain->SetChunkSize(8.f);
            terrain->Resize({ 0, 0 }, { 2, 2 });
            terrain->GenerateFromNoise();

            const auto heights = terrain->GetHeightField();
            const auto fieldSize = terrain->GetFieldSize();

            auto data = terrain->Save();

            // Fields are written by name with the value straight after it, so a saved terrain can
            // be broken the way a corrupt one would be broken.
            const auto findValue = [&](const char* name)
            {
                const std::string needle(name);

                const auto begin = reinterpret_cast<char*>(data.data);
                const auto end = begin + data.size;
                const auto at = std::search(begin, end, needle.begin(), needle.end());

                check(at != end, std::string("Could not find '") + name + "' in a saved terrain");
                check(static_cast<std::size_t>(end - at) > needle.size() + sizeof(std::uint64_t),
                    std::string("A saved terrain ends inside '") + name + "'");

                return at + needle.size();
            };

            // Read back before anything is changed, so a change to the format fails here as itself
            // rather than further down as a load that should have been refused.
            auto quadsAt = findValue("ChunkQuads");

            std::int32_t quads = 0;

            std::memcpy(&quads, quadsAt, sizeof(quads));

            check(quads == 8, "A saved terrain's ChunkQuads is not where this probe expects it");

            // Across 2x2 chunks this is far more quads than an axis may span, and the product
            // overflows the int GetFieldSize() works it out in.
            const std::int32_t brokenQuads = 400000000;

            std::memcpy(quadsAt, &brokenQuads, sizeof(brokenQuads));

            // Asset::Load hands back the asset it already has when the timestamps match, so the
            // data has to look like a newer version of the file for LoadAssetData to run at all.
            auto timeAt = findValue("Time");

            std::uint64_t time = 0;

            std::memcpy(&time, timeAt, sizeof(time));

            time += 1;

            std::memcpy(timeAt, &time, sizeof(time));

            check(Pine::Asset::Load(data) == nullptr,
                "A terrain whose layout cannot be indexed loaded anyway");

            check(terrain->GetChunkQuads() == 8,
                "A refused terrain load changed the chunk quads anyway");
            check(terrain->GetFieldSize() == fieldSize,
                "A refused terrain load changed the field size anyway");
            check(terrain->GetHeightField() == heights,
                "A refused terrain load changed the height field anyway");

            // The layout and the field still agree, which is the whole point: every accessor
            // bounds-checks against the layout and then indexes the field.
            const auto corner = terrain->GetSampleHeight(terrain->GetSampleMax());

            check(corner.has_value(),
                "A refused terrain load left the terrain unable to read its own far corner");
        }

        std::cout << "PASS(native): oversized and overflowing terrain layouts refused while a large "
                     "supportable one is\n              accepted, a height range change keeping the "
                     "ground where it was and clamping\n              what no longer fits, the noise "
                     "bands and the layout surviving a save and a load,\n              and a file whose "
                     "layout cannot be indexed leaving the loaded terrain as it was." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }

    std::_Exit(0);
