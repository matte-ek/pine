    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        // Little-endian appenders, so the wave files below read as the layout they describe.
        const auto appendBytes = [](std::vector<char>& output, const void* data, std::size_t size)
        {
            const auto bytes = static_cast<const char*>(data);

            output.insert(output.end(), bytes, bytes + size);
        };

        const auto appendUInt32 = [&](std::vector<char>& output, std::uint32_t value)
        {
            appendBytes(output, &value, sizeof(value));
        };

        const auto appendUInt16 = [&](std::vector<char>& output, std::uint16_t value)
        {
            appendBytes(output, &value, sizeof(value));
        };

        const auto appendFourCC = [&](std::vector<char>& output, const char* value)
        {
            appendBytes(output, value, 4);
        };

        // Writes a 16-bit PCM wave file of a sine, one octave apart per channel so a channel read
        // into the wrong slot is visible. 'listChunkLength' puts a junk chunk of that size ahead of
        // the sample data: an odd length is the case a parser has to pad past, and losing that
        // alignment is what makes an otherwise valid file decode as noise.
        const auto writeWave = [&](
            const std::string& path,
            const int channels,
            const int sampleRate,
            const int frames,
            const int listChunkLength)
        {
            std::vector<char> samples;

            for (int frame = 0; frame < frames; frame++)
            {
                for (int channel = 0; channel < channels; channel++)
                {
                    const auto frequency = 440.0 * (channel + 1);
                    const auto phase = 2.0 * 3.14159265358979 * frequency * frame / sampleRate;

                    const auto value = static_cast<std::int16_t>(std::sin(phase) * 20000.0);

                    appendBytes(samples, &value, sizeof(value));
                }
            }

            std::vector<char> body;

            appendFourCC(body, "WAVE");

            appendFourCC(body, "fmt ");
            appendUInt32(body, 16);
            appendUInt16(body, 1);                                  // PCM
            appendUInt16(body, static_cast<std::uint16_t>(channels));
            appendUInt32(body, static_cast<std::uint32_t>(sampleRate));
            appendUInt32(body, static_cast<std::uint32_t>(sampleRate * channels * 2));
            appendUInt16(body, static_cast<std::uint16_t>(channels * 2));
            appendUInt16(body, 16);

            if (listChunkLength > 0)
            {
                appendFourCC(body, "LIST");
                appendUInt32(body, static_cast<std::uint32_t>(listChunkLength));

                body.insert(body.end(), listChunkLength, 'x');

                // RIFF pads an odd-length chunk to an even boundary.
                if (listChunkLength % 2 != 0)
                {
                    body.push_back('\0');
                }
            }

            appendFourCC(body, "data");
            appendUInt32(body, static_cast<std::uint32_t>(samples.size()));

            body.insert(body.end(), samples.begin(), samples.end());

            std::vector<char> file;

            appendFourCC(file, "RIFF");
            appendUInt32(file, static_cast<std::uint32_t>(body.size()));

            file.insert(file.end(), body.begin(), body.end());

            std::ofstream stream(path, std::ios::binary);

            stream.write(file.data(), static_cast<std::streamsize>(file.size()));
        };

        // Imports one source file and hands back the asset the importer registered, or nullptr.
        const auto importAudio = [&](const std::string& source, const std::string& enginePath)
        {
            auto context = Pine::Importer::CreateContext();

            Pine::Importer::AddFile(context, source, enginePath);
            Pine::Importer::Run(context);

            Pine::Asset* asset = nullptr;

            if (!context->Imports.empty() &&
                context->Imports.front()->ImportStatus == Pine::AssetImportStatus::Imported)
            {
                asset = context->Imports.front()->AssetPtr;
            }

            Pine::Importer::DeleteContext(context);

            return dynamic_cast<Pine::AudioFile*>(asset);
        };

        // What the audio device actually holds, rather than what the asset believes it sent.
        const auto bufferChannels = [&](const Pine::AudioFile* audioFile)
        {
            const auto buffer = audioFile->GetBuffer();

            check(buffer != nullptr, "A loaded audio asset has no buffer on the audio device");

            ALint channels = 0;

            alGetBufferi(*static_cast<std::uint32_t*>(buffer->GetAudioIdentifier()), AL_CHANNELS, &channels);

            return static_cast<int>(channels);
        };

        const auto bufferSize = [&](const Pine::AudioFile* audioFile)
        {
            ALint size = 0;

            alGetBufferi(*static_cast<std::uint32_t*>(audioFile->GetBuffer()->GetAudioIdentifier()), AL_SIZE, &size);

            return static_cast<std::size_t>(size);
        };

        const std::string content = "projects/audio/content/";

        constexpr int SampleRate = 44100;
        constexpr int Frames = 11025;

        // Kept for part 4, which saves this same clip again once its import samples are gone.
        Pine::AudioFile* stereoClip = nullptr;

        // Part 1 - a stereo clip through the whole pipeline: decoded at import, stored as PCM in
        // the '.passet', and uploaded to the audio device on load.
        {
            writeWave(content + "stereo.wav", 2, SampleRate, Frames, 0);

            const auto audioFile = importAudio(content + "stereo.wav", "projects/audio/assets/stereo");

            check(audioFile != nullptr, "A stereo wave file did not import");
            check(audioFile->GetState() == Pine::AssetState::Loaded, "An imported audio asset did not load");
            check(Pine::Assets::GetAssetByUId(audioFile->GetUId()) == audioFile,
                "An imported audio asset was not registered with the asset manager");

            stereoClip = audioFile;

            check(audioFile->GetFormat() == Pine::Audio::AudioFormat::Stereo16,
                "A stereo wave file was not stored as a stereo clip");
            check(audioFile->GetChannelCount() == 2, "A stereo clip reports the wrong channel count");
            check(audioFile->GetSampleRate() == SampleRate, "An imported clip lost its sample rate");
            check(audioFile->GetSampleCount() == Frames, "An imported clip lost samples");
            check(std::abs(audioFile->GetDuration() - 0.25f) < 0.001f, "An imported clip reports the wrong duration");
            check(audioFile->GetSampleDataSize() == static_cast<std::size_t>(Frames) * 2 * sizeof(std::int16_t),
                "An imported clip reports the wrong data size");

            check(bufferChannels(audioFile) == 2, "A stereo clip reached the audio device as mono");
            check(bufferSize(audioFile) == audioFile->GetSampleDataSize(),
                "The audio device holds a different number of bytes than the asset stored");
        }

        // Part 2 - the chunks a real wave file carries between its header and its samples. An odd
        // length is the one that has to be padded past.
        {
            writeWave(content + "padded.wav", 1, SampleRate, Frames, 37);

            const auto audioFile = importAudio(content + "padded.wav", "projects/audio/assets/padded");

            check(audioFile != nullptr, "A wave file with an odd-length chunk before its data did not import");
            check(audioFile->GetSampleCount() == Frames,
                "An odd-length chunk before the sample data lost the parser its alignment");
            check(audioFile->GetFormat() == Pine::Audio::AudioFormat::Mono16,
                "A mono wave file was not stored as a mono clip");
            check(bufferChannels(audioFile) == 1, "A mono clip reached the audio device as stereo");
        }

        // Part 3 - force mono. It changes what gets decoded and stored, so it only takes effect on
        // a re-import - which has to keep the asset's UId, since everything referencing a clip
        // references it by that.
        {
            writeWave(content + "spatial.wav", 2, SampleRate, Frames, 0);

            const auto audioFile = importAudio(content + "spatial.wav", "projects/audio/assets/spatial");

            check(audioFile != nullptr, "A stereo wave file did not import");
            check(audioFile->GetFormat() == Pine::Audio::AudioFormat::Stereo16, "A stereo clip imported as mono");

            const auto uid = audioFile->GetUId();

            audioFile->GetImportConfiguration().ForceMono = true;
            audioFile->ReImport();

            check(audioFile->GetFormat() == Pine::Audio::AudioFormat::Mono16,
                "Force mono did not fold a stereo clip down on re-import");
            check(audioFile->GetChannelCount() == 1, "A force-mono clip reports the wrong channel count");
            check(audioFile->GetSampleCount() == Frames, "Force mono changed how long the clip is");
            check(std::abs(audioFile->GetDuration() - 0.25f) < 0.001f, "Force mono changed the clip's duration");
            check(audioFile->GetUId() == uid, "Re-importing an audio asset minted a new UId");

            check(bufferChannels(audioFile) == 1, "A force-mono clip reached the audio device as stereo");
            check(bufferSize(audioFile) == static_cast<std::size_t>(Frames) * sizeof(std::int16_t),
                "A force-mono clip sent the audio device the wrong number of bytes");

            // The import settings ride along in the '.passet', so the next re-import uses the same
            // ones rather than silently reverting to stereo.
            check(audioFile->GetImportConfiguration().ForceMono, "Force mono did not survive the re-import");
        }

        // Part 4 - saving a clip that is already imported. The decoded samples only exist in memory
        // right after an import, so every later save has to carry the stored PCM across by reading
        // back the '.passet' it is replacing. Getting this wrong writes out a clip with no audio in
        // it, and nothing about the asset in memory would look wrong at the time.
        {
            const auto audioFile = stereoClip;

            check(audioFile != nullptr, "Could not find the imported stereo clip");

            const auto expected = audioFile->GetSampleDataSize();

            // Nothing has been imported since the asset was written, so this goes through the
            // read-back path rather than through the import samples.
            const auto saved = audioFile->Save();

            check(saved.size > expected,
                "Saving an already-imported audio asset dropped its samples");

            // And again, to make sure the save it just wrote is itself re-readable.
            audioFile->ReLoad();

            check(audioFile->GetSampleCount() == Frames && audioFile->GetFormat() == Pine::Audio::AudioFormat::Stereo16,
                "An audio asset did not survive a save and a load");
            check(bufferSize(audioFile) == expected,
                "An audio asset re-uploaded the wrong number of bytes after a save and a load");
        }

        // Part 5 - files that cannot be decoded. An import that fails has to fail, rather than
        // registering a silent asset that looks fine everywhere except when it is played.
        {
            std::ofstream(content + "truncated.wav", std::ios::binary) << "RIFF";

            check(importAudio(content + "truncated.wav", "projects/audio/assets/truncated") == nullptr,
                "A truncated wave file imported anyway");

            // A valid header describing an encoding there is no decoder for.
            {
                std::vector<char> file;

                appendFourCC(file, "RIFF");
                appendUInt32(file, 30);
                appendFourCC(file, "WAVE");
                appendFourCC(file, "fmt ");
                appendUInt32(file, 16);
                appendUInt16(file, 0x0011);     // IMA ADPCM
                appendUInt16(file, 1);
                appendUInt32(file, SampleRate);
                appendUInt32(file, SampleRate);
                appendUInt16(file, 1);
                appendUInt16(file, 4);

                std::ofstream stream(content + "adpcm.wav", std::ios::binary);

                stream.write(file.data(), static_cast<std::streamsize>(file.size()));
            }

            check(importAudio(content + "adpcm.wav", "projects/audio/assets/adpcm") == nullptr,
                "A compressed wave file this engine cannot decode imported anyway");

            std::ofstream(content + "notvorbis.ogg", std::ios::binary) << "OggS not really a vorbis stream";

            check(importAudio(content + "notvorbis.ogg", "projects/audio/assets/notvorbis") == nullptr,
                "A file that is not Ogg Vorbis imported anyway");

            check(!std::filesystem::exists("projects/audio/assets/truncated.passet") &&
                  !std::filesystem::exists("projects/audio/assets/adpcm.passet") &&
                  !std::filesystem::exists("projects/audio/assets/notvorbis.passet"),
                "A failed audio import wrote an asset anyway");
        }

        std::cout << "PASS(native): wave clips decoded to PCM at import and stored in the '.passet', "
                     "odd-length chunks\n              padded past, force mono folding a stereo clip "
                     "down on re-import while keeping its\n              UId, an already-imported clip "
                     "keeping its samples through a save and a load, and\n              files that cannot "
                     "be decoded failing rather than registering silent assets." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }

    std::_Exit(0);
