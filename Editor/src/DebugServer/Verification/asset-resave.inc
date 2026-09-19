    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        // An asset type whose bulk data lives on the GPU or the audio device once loaded keeps no
        // second copy of it in system memory, so saving one that was not just imported has to
        // recover that data from the '.passet' it is replacing. When that recovery silently finds
        // nothing, the save still succeeds and still writes - over the only good copy. Every check
        // below is a round trip through Asset::ReLoad(), which is Save() followed by a load of what
        // it produced, so a save that dropped its payload cannot pass by leaving memory untouched.

        // Part 1 - a texture's mips.
        {
            const auto texture = Pine::Assets::Get<Pine::Texture2D>("editor/icons/audio");

            check(texture != nullptr, "Could not find the texture this probe re-saves");
            check(texture->GetState() == Pine::AssetState::Loaded, "The texture this probe re-saves is not loaded");

            const auto width = texture->GetWidth();
            const auto height = texture->GetHeight();
            const auto mipmapLevels = texture->GetMipmapLevels();
            const auto format = texture->GetFormat();
            const auto compressionFormat = texture->GetCompressionFormat();

            check(mipmapLevels > 1, "The texture this probe re-saves has no mips to lose");

            texture->ReLoad();

            check(texture->GetMipmapLevels() == mipmapLevels,
                "Re-saving a texture that was not just imported dropped its mips");
            check(texture->GetWidth() == width && texture->GetHeight() == height,
                "Re-saving a texture changed its dimensions");
            check(texture->GetFormat() == format && texture->GetCompressionFormat() == compressionFormat,
                "Re-saving a texture changed how its pixels are stored");

            // And from the file rather than from memory, since that is what the next run reads.
            texture->ReLoad();

            check(texture->GetMipmapLevels() == mipmapLevels,
                "A texture lost its mips on the second re-save");
        }

        // Part 2 - a model's geometry.
        {
            const auto model = Pine::Assets::Get<Pine::Model>("engine/primitive/sphere");

            check(model != nullptr, "Could not find the model this probe re-saves");
            check(model->GetState() == Pine::AssetState::Loaded, "The model this probe re-saves is not loaded");

            const auto meshCount = model->GetMeshes().size();
            const auto boundsMin = model->GetBoundingBoxMin();
            const auto boundsMax = model->GetBoundingBoxMax();

            check(meshCount > 0, "The model this probe re-saves has no meshes to lose");

            model->ReLoad();

            check(model->GetMeshes().size() == meshCount,
                "Re-saving a model that was not just imported dropped its meshes");
            check(model->GetBoundingBoxMin() == boundsMin && model->GetBoundingBoxMax() == boundsMax,
                "Re-saving a model changed its bounds");

            model->ReLoad();

            check(model->GetMeshes().size() == meshCount,
                "A model lost its meshes on the second re-save");
        }

        // Part 3 - an audio clip's samples. Imported here rather than taken from the engine assets,
        // since none of those are audio.
        {
            std::vector<char> file;

            const auto u32 = [&](std::uint32_t value)
            {
                const auto bytes = reinterpret_cast<const char*>(&value);
                file.insert(file.end(), bytes, bytes + sizeof(value));
            };

            const auto u16 = [&](std::uint16_t value)
            {
                const auto bytes = reinterpret_cast<const char*>(&value);
                file.insert(file.end(), bytes, bytes + sizeof(value));
            };

            const auto fourCC = [&](const char* value)
            {
                file.insert(file.end(), value, value + 4);
            };

            constexpr int SampleRate = 44100;
            constexpr int Frames = 11025;

            std::vector<char> samples;

            for (int frame = 0; frame < Frames; frame++)
            {
                const auto value = static_cast<std::int16_t>(
                    std::sin(2.0 * 3.14159265358979 * 440.0 * frame / SampleRate) * 20000.0);

                const auto bytes = reinterpret_cast<const char*>(&value);

                samples.insert(samples.end(), bytes, bytes + sizeof(value));
            }

            fourCC("RIFF");
            u32(static_cast<std::uint32_t>(36 + samples.size()));
            fourCC("WAVE");
            fourCC("fmt ");
            u32(16);
            u16(1);
            u16(1);
            u32(SampleRate);
            u32(SampleRate * 2);
            u16(2);
            u16(16);
            fourCC("data");
            u32(static_cast<std::uint32_t>(samples.size()));

            file.insert(file.end(), samples.begin(), samples.end());

            {
                std::ofstream stream("projects/resave/content/tone.wav", std::ios::binary);

                stream.write(file.data(), static_cast<std::streamsize>(file.size()));
            }

            auto context = Pine::Importer::CreateContext();

            Pine::Importer::AddFile(context, "projects/resave/content/tone.wav", "projects/resave/assets/tone");
            Pine::Importer::Run(context);

            const auto imported = context->Imports.front()->ImportStatus == Pine::AssetImportStatus::Imported
                ? context->Imports.front()->AssetPtr : nullptr;

            Pine::Importer::DeleteContext(context);

            const auto clip = dynamic_cast<Pine::AudioFile*>(imported);

            check(clip != nullptr, "The audio clip this probe re-saves did not import");

            const auto sampleCount = clip->GetSampleCount();
            const auto sampleRate = clip->GetSampleRate();
            const auto format = clip->GetFormat();

            check(sampleCount == Frames, "The imported clip has the wrong length");

            // The import samples were consumed writing the '.passet', so this is already the
            // recovery path.
            clip->ReLoad();

            check(clip->GetSampleCount() == sampleCount && clip->GetSampleRate() == sampleRate &&
                  clip->GetFormat() == format,
                "Re-saving an audio clip that was not just imported dropped its samples");

            clip->ReLoad();

            check(clip->GetSampleCount() == sampleCount,
                "An audio clip lost its samples on the second re-save");
        }

        std::cout << "PASS(native): a texture's mips, a model's meshes and bounds, and an audio "
                     "clip's samples all\n              surviving repeated saves of an asset that "
                     "was not just imported - the path that\n              has to recover them from "
                     "the '.passet' it is about to overwrite." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }

    std::_Exit(0);
