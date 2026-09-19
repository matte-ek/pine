    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        const auto near = [](const float value, const float expected, const float tolerance)
        {
            return std::abs(value - expected) <= tolerance;
        };

        // A mono 16-bit wave of a sine, written straight out. Mono because only a mono clip is
        // positioned in OpenAL, which is what the spatial half of this is about.
        const auto writeWave = [&](const std::string& path, const int sampleRate, const int frames)
        {
            const auto append = [](std::vector<char>& output, const void* data, std::size_t size)
            {
                const auto bytes = static_cast<const char*>(data);

                output.insert(output.end(), bytes, bytes + size);
            };

            const auto appendUInt32 = [&](std::vector<char>& output, std::uint32_t value)
            {
                append(output, &value, sizeof(value));
            };

            const auto appendUInt16 = [&](std::vector<char>& output, std::uint16_t value)
            {
                append(output, &value, sizeof(value));
            };

            std::vector<char> samples;

            for (int frame = 0; frame < frames; frame++)
            {
                const auto phase = 2.0 * 3.14159265358979 * 440.0 * frame / sampleRate;
                const auto value = static_cast<std::int16_t>(std::sin(phase) * 20000.0);

                append(samples, &value, sizeof(value));
            }

            std::vector<char> body;

            append(body, "WAVE", 4);
            append(body, "fmt ", 4);
            appendUInt32(body, 16);
            appendUInt16(body, 1);                                  // PCM
            appendUInt16(body, 1);                                  // mono
            appendUInt32(body, static_cast<std::uint32_t>(sampleRate));
            appendUInt32(body, static_cast<std::uint32_t>(sampleRate * 2));
            appendUInt16(body, 2);
            appendUInt16(body, 16);
            append(body, "data", 4);
            appendUInt32(body, static_cast<std::uint32_t>(samples.size()));

            body.insert(body.end(), samples.begin(), samples.end());

            std::vector<char> file;

            append(file, "RIFF", 4);
            appendUInt32(file, static_cast<std::uint32_t>(body.size()));

            file.insert(file.end(), body.begin(), body.end());

            std::ofstream stream(path, std::ios::binary);

            stream.write(file.data(), static_cast<std::streamsize>(file.size()));
        };

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

        // Stands in for the main loop: runs Audio::Update() over a stretch of real time, which is
        // what a clip needs in order to actually get anywhere. The null OpenAL backend mixes at the
        // real sample rate, so playback advances here exactly as it would on a sound card.
        const auto pump = [&](const double seconds)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);

            do
            {
                Pine::Audio::Update();

                std::this_thread::sleep_for(std::chrono::milliseconds(4));
            }
            while (std::chrono::steady_clock::now() < deadline);

            Pine::Audio::Update();
        };

        const auto heldVoice = [](Pine::AudioSource* source)
        {
            return source->GetPlaybackHintData().VoiceIndex;
        };

        check(Pine::Audio::HasInitializedAudioAPI(), "The probe needs an audio API; is ALSOFT_DRIVERS=null set?");

        // Application.cpp pauses the world before the main loop so the editor does not simulate on
        // open. Audio honours that by holding every sound still, which is part 7's business - the
        // rest of this needs the world running.
        Pine::World::SetPaused(false);

        constexpr int SampleRate = 44100;
        constexpr float ClipDuration = 0.25f;

        writeWave("projects/audio/content/tone.wav", SampleRate, static_cast<int>(SampleRate * ClipDuration));

        const auto clip = importAudio("projects/audio/content/tone.wav", "projects/audio/assets/tone");

        check(clip != nullptr && clip->GetBuffer() != nullptr, "The probe's clip did not import and upload");

        // Part 1 - the backend seam. Every IAudioSource setter has to reach OpenAL, since the whole
        // point of the rework is that nothing above this layer speaks to the device itself.
        {
            const auto source = dynamic_cast<Pine::Audio::ALSource*>(Pine::Audio::GetAudioAPI()->CreateSource());

            check(source != nullptr, "The audio API would not create a source");

            const auto id = source->GetId();

            source->SetBuffer(clip->GetBuffer());
            source->SetVolume(0.5f);
            source->SetPitch(1.5f);
            source->SetLooping(true);
            source->SetSpatial(true);
            source->SetPosition(Pine::Vector3f(1.f, 2.f, 3.f));
            source->SetAttenuation(4.f, 40.f, 2.f);

            ALint buffer = 0;
            ALint looping = 0;
            ALint relative = 0;
            ALfloat gain = 0.f;
            ALfloat pitch = 0.f;
            ALfloat position[3] = { 0.f, 0.f, 0.f };
            ALfloat referenceDistance = 0.f;
            ALfloat maxDistance = 0.f;
            ALfloat rolloff = 0.f;

            alGetSourcei(id, AL_BUFFER, &buffer);
            alGetSourcei(id, AL_LOOPING, &looping);
            alGetSourcei(id, AL_SOURCE_RELATIVE, &relative);
            alGetSourcef(id, AL_GAIN, &gain);
            alGetSourcef(id, AL_PITCH, &pitch);
            alGetSource3f(id, AL_POSITION, &position[0], &position[1], &position[2]);
            alGetSourcef(id, AL_REFERENCE_DISTANCE, &referenceDistance);
            alGetSourcef(id, AL_MAX_DISTANCE, &maxDistance);
            alGetSourcef(id, AL_ROLLOFF_FACTOR, &rolloff);

            check(buffer == static_cast<ALint>(*static_cast<std::uint32_t*>(clip->GetBuffer()->GetAudioIdentifier())),
                "SetBuffer did not bind the clip to the source");
            check(looping == AL_TRUE, "SetLooping did not reach the audio device");
            check(relative == AL_FALSE, "A spatial source was left relative to the listener");
            check(near(gain, 0.5f, 0.001f), "SetVolume did not reach the audio device");
            check(near(pitch, 1.5f, 0.001f), "SetPitch did not reach the audio device");
            check(near(position[0], 1.f, 0.001f) && near(position[1], 2.f, 0.001f) && near(position[2], 3.f, 0.001f),
                "SetPosition did not reach the audio device");
            check(near(referenceDistance, 4.f, 0.001f) && near(maxDistance, 40.f, 0.001f) && near(rolloff, 2.f, 0.001f),
                "SetAttenuation did not reach the audio device");

            // 2D playback is a source pinned to the listener, not a separate path, so switching it
            // has to both mark the source relative and bring it back to the origin.
            source->SetSpatial(false);

            alGetSourcei(id, AL_SOURCE_RELATIVE, &relative);
            alGetSource3f(id, AL_POSITION, &position[0], &position[1], &position[2]);

            check(relative == AL_TRUE, "A non-spatial source was not made relative to the listener");
            check(near(position[0], 0.f, 0.001f) && near(position[1], 0.f, 0.001f) && near(position[2], 0.f, 0.001f),
                "A non-spatial source was not moved onto the listener");

            Pine::Audio::GetAudioAPI()->DestroySource(source);
        }

        // Part 2 - the listener follows the active AudioListener, and the world falls silent when
        // there is not one. Nothing made an alListener* call before this rework.
        {
            const auto entity = Pine::Entities::Create("Listener");
            const auto listener = entity->AddComponent<Pine::AudioListener>();

            entity->GetTransform()->SetLocalPosition(Pine::Vector3f(5.f, 6.f, 7.f));
            entity->GetTransform()->SetEulerAngles(Pine::Vector3f(0.f, 90.f, 0.f));

            listener->SetVolume(0.75f);

            Pine::Audio::Update();

            ALfloat position[3] = { 0.f, 0.f, 0.f };
            ALfloat orientation[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
            ALfloat gain = 0.f;

            alGetListener3f(AL_POSITION, &position[0], &position[1], &position[2]);
            alGetListenerfv(AL_ORIENTATION, orientation);
            alGetListenerf(AL_GAIN, &gain);

            check(near(position[0], 5.f, 0.001f) && near(position[1], 6.f, 0.001f) && near(position[2], 7.f, 0.001f),
                "The listener did not follow its entity's transform");
            check(near(gain, 0.75f, 0.001f), "The listener's volume did not reach the audio device");

            // Yawed a quarter turn, so the forward half of AL_ORIENTATION should point down -X and
            // up should be untouched.
            check(near(orientation[0], -1.f, 0.01f) && near(orientation[2], 0.f, 0.01f),
                "The listener's orientation did not follow its entity's rotation");
            check(near(orientation[4], 1.f, 0.01f), "The listener's up vector reached the device wrong");

            // Removing the ears has to silence the device rather than leave them where they stood.
            entity->SetActive(false);

            Pine::Audio::Update();

            alGetListenerf(AL_GAIN, &gain);

            check(near(gain, 0.f, 0.001f), "A world with no active listener was left audible");

            entity->SetActive(true);
        }

        // Part 3 - a one-shot: it plays, its position advances, and when the clip runs out it stops
        // on its own and gives its voice back. The component's own state has to be corrected too,
        // since nothing called Stop().
        {
            const auto entity = Pine::Entities::Create("OneShot");
            const auto source = entity->AddComponent<Pine::AudioSource>();

            source->SetAudioFile(clip);

            check(source->GetPlaybackState() == Pine::Audio::PlaybackState::Stopped,
                "A new audio source did not start out stopped");
            check(heldVoice(source) == -1, "A stopped audio source was holding a voice");

            source->Play();

            pump(ClipDuration * 0.4f);

            check(source->IsPlaying(), "A source stopped before its clip ran out");
            check(heldVoice(source) != -1, "A playing source was not lent a voice");
            check(source->GetPlaybackPosition() > 0.f,
                "Playback position did not advance while the clip was playing");
            check(source->GetPlaybackPosition() < ClipDuration,
                "Playback position ran past the end of the clip");

            pump(ClipDuration);

            check(source->GetPlaybackState() == Pine::Audio::PlaybackState::Stopped,
                "A clip that ran out did not leave its source stopped");
            check(heldVoice(source) == -1, "A finished source kept its voice");
            check(source->GetPlaybackPosition() == 0.f, "A finished source did not rewind");

            Pine::Entities::Delete(entity);
        }

        // Part 4 - looping. The same clip, the same wait, and it is still going.
        {
            const auto entity = Pine::Entities::Create("Loop");
            const auto source = entity->AddComponent<Pine::AudioSource>();

            source->SetAudioFile(clip);
            source->SetLoop(true);
            source->Play();

            pump(ClipDuration * 2.5f);

            check(source->IsPlaying(), "A looping source stopped at the end of its clip");
            check(heldVoice(source) != -1, "A looping source lost its voice");

            Pine::Entities::Delete(entity);
        }

        // Part 5 - pause keeps its place, stop rewinds, and a seek survives being asked for while
        // the source holds no voice at all.
        {
            const auto entity = Pine::Entities::Create("Transport");
            const auto source = entity->AddComponent<Pine::AudioSource>();

            source->SetAudioFile(clip);
            source->SetLoop(true);
            source->Play();

            pump(ClipDuration * 0.4f);

            source->Pause();

            pump(0.05);

            const auto paused = source->GetPlaybackPosition();

            check(source->GetPlaybackState() == Pine::Audio::PlaybackState::Paused,
                "A paused source did not stay paused");
            check(paused > 0.f, "A paused source lost its place");
            check(heldVoice(source) != -1, "A paused source gave up its voice");

            pump(0.1);

            check(near(source->GetPlaybackPosition(), paused, 0.005f),
                "A paused source kept playing");

            source->Play();

            pump(0.05);

            check(source->GetPlaybackPosition() > paused, "A resumed source did not carry on");

            source->Stop();

            check(source->GetPlaybackPosition() == 0.f, "Stop did not rewind the source");

            Pine::Audio::Update();

            check(heldVoice(source) == -1, "A stopped source kept its voice");

            // Seeking while stopped, and therefore while holding no voice: the request has to be
            // waiting when a voice is finally lent, or "play this from halfway" cannot work.
            source->SetPlaybackPosition(ClipDuration * 0.75f);
            source->Play();

            pump(0.02);

            check(source->GetPlaybackPosition() > ClipDuration * 0.7f,
                "A seek asked for while the source held no voice was lost");

            Pine::Entities::Delete(entity);
        }

        // Part 6 - the two ways a source stops existing. Both have to hand the voice back: the ECS
        // runs no destructors, and a sound on a switched-off entity is never what was meant.
        {
            const auto entity = Pine::Entities::Create("Lifecycle");
            const auto source = entity->AddComponent<Pine::AudioSource>();

            source->SetAudioFile(clip);
            source->SetLoop(true);
            source->Play();

            pump(0.05);

            const auto voice = heldVoice(source);

            check(voice != -1, "A playing source was not lent a voice");

            entity->SetActive(false);

            Pine::Audio::Update();

            check(heldVoice(source) == -1, "A source on a disabled entity kept its voice");

            entity->SetActive(true);

            pump(0.05);

            check(source->IsPlaying() && heldVoice(source) != -1,
                "A source did not pick a voice back up when its entity was re-enabled");

            Pine::Entities::Delete(entity);

            // Nothing is left to hand the voice back by name, so the only way this can be true is
            // if destroying the component released it.
            Pine::Audio::Update();

            const auto probe = Pine::Entities::Create("LifecycleProbe");
            const auto reused = probe->AddComponent<Pine::AudioSource>();

            reused->SetAudioFile(clip);
            reused->Play();

            pump(0.05);

            check(heldVoice(reused) != -1, "The destroyed source's voice never came back to the pool");

            Pine::Entities::Delete(probe);
        }

        // Part 7 - voices are a budget. Asking more sources to play than there are voices must leave
        // the surplus silent rather than failing, and a source that gives its voice up must let one
        // of those surplus sources through.
        {
            constexpr int SourceCount = 48;

            std::vector<Pine::Entity*> entities;
            std::vector<Pine::AudioSource*> sources;

            for (int i = 0; i < SourceCount; i++)
            {
                const auto entity = Pine::Entities::Create("Crowd" + std::to_string(i));
                const auto source = entity->AddComponent<Pine::AudioSource>();

                source->SetAudioFile(clip);
                source->SetLoop(true);
                source->Play();

                entities.push_back(entity);
                sources.push_back(source);
            }

            pump(0.05);

            std::set<int> voices;

            for (const auto source : sources)
            {
                if (heldVoice(source) != -1)
                {
                    check(voices.insert(heldVoice(source)).second, "Two sources were lent the same voice");
                }
            }

            check(!voices.empty(), "Nothing was heard at all with the pool under pressure");
            check(static_cast<int>(voices.size()) < SourceCount,
                "More voices were handed out than the pool was ever meant to hold");

            const auto handedOut = voices.size();

            // Every source still wants to play, so a voice given back has to go straight to one of
            // the ones that has been waiting.
            Pine::AudioSource* silent = nullptr;

            for (const auto source : sources)
            {
                if (heldVoice(source) == -1)
                {
                    silent = source;
                    break;
                }
            }

            check(silent != nullptr, "Expected at least one source to be waiting for a voice");

            for (const auto source : sources)
            {
                if (heldVoice(source) != -1)
                {
                    source->Stop();
                    break;
                }
            }

            pump(0.05);

            check(heldVoice(silent) != -1, "A freed voice was not passed on to a source waiting for one");

            std::set<int> after;

            for (const auto source : sources)
            {
                if (heldVoice(source) != -1)
                {
                    check(after.insert(heldVoice(source)).second, "Two sources were lent the same voice");
                }
            }

            check(after.size() == handedOut, "The pool lost or invented voices under pressure");

            for (const auto entity : entities)
            {
                Pine::Entities::Delete(entity);
            }

            Pine::Audio::Update();
        }

        // Part 8 - a paused world holds every sound where it is, which is what makes leaving play
        // mode and going back in continue a clip instead of restarting it.
        {
            const auto entity = Pine::Entities::Create("Paused");
            const auto source = entity->AddComponent<Pine::AudioSource>();

            source->SetAudioFile(clip);
            source->SetLoop(true);
            source->Play();

            pump(0.05);

            Pine::World::SetPaused(true);

            pump(0.05);

            const auto held = source->GetPlaybackPosition();

            pump(0.1);

            check(near(source->GetPlaybackPosition(), held, 0.005f),
                "A paused world kept playing its sounds");
            check(source->IsPlaying(),
                "A paused world stopped its sounds rather than holding them");

            Pine::World::SetPaused(false);

            pump(0.05);

            check(source->GetPlaybackPosition() > held, "Unpausing the world did not resume playback");

            Pine::Entities::Delete(entity);
        }

        // Part 9 - none of the component's fields were serialized before this rework, so a saved
        // level had no idea what any of its sources were meant to play.
        {
            const auto entity = Pine::Entities::Create("Serialized");
            const auto source = entity->AddComponent<Pine::AudioSource>();

            source->SetAudioFile(clip);
            source->SetPlayOnStart(true);
            source->SetLoop(true);
            source->SetSpatial(false);
            source->SetVolume(0.4f);
            source->SetPitch(1.25f);
            source->SetReferenceDistance(3.f);
            source->SetMaxDistance(30.f);
            source->SetRolloffFactor(0.5f);

            const auto restoredEntity = Pine::Entities::Create("Restored");
            const auto restored = restoredEntity->AddComponent<Pine::AudioSource>();

            restored->LoadData(source->SaveData());

            check(restored->GetAudioFile() == clip, "An audio source lost its clip through a save and a load");
            check(restored->GetPlayOnStart(), "An audio source lost play-on-start");
            check(restored->GetLoop(), "An audio source lost its loop flag");
            check(!restored->GetSpatial(), "An audio source lost its spatial flag");
            check(near(restored->GetVolume(), 0.4f, 0.001f), "An audio source lost its volume");
            check(near(restored->GetPitch(), 1.25f, 0.001f), "An audio source lost its pitch");
            check(near(restored->GetReferenceDistance(), 3.f, 0.001f), "An audio source lost its reference distance");
            check(near(restored->GetMaxDistance(), 30.f, 0.001f), "An audio source lost its max distance");
            check(near(restored->GetRolloffFactor(), 0.5f, 0.001f), "An audio source lost its rolloff factor");

            // Play-on-start is what a level actually relies on, and OnSetup is where it is acted on.
            restored->OnSetup();

            check(restored->IsPlaying(), "Play-on-start did not start a restored source");

            const auto listenerEntity = Pine::Entities::Create("SerializedListener");
            const auto listener = listenerEntity->AddComponent<Pine::AudioListener>();

            listener->SetVolume(0.6f);

            const auto restoredListenerEntity = Pine::Entities::Create("RestoredListener");
            const auto restoredListener = restoredListenerEntity->AddComponent<Pine::AudioListener>();

            restoredListener->LoadData(listener->SaveData());

            check(near(restoredListener->GetVolume(), 0.6f, 0.001f),
                "An audio listener lost its volume through a save and a load");

            Pine::Entities::Delete(entity);
            Pine::Entities::Delete(restoredEntity);
            Pine::Entities::Delete(listenerEntity);
            Pine::Entities::Delete(restoredListenerEntity);
        }

        std::cout << "PASS(native): every IAudioSource setter reaching OpenAL, the listener following its\n"
                     "              entity and falling silent without one, a one-shot advancing and ending on\n"
                     "              its own, looping, pause/stop/seek, voices handed back by disabled and\n"
                     "              destroyed sources, a 48-source crowd staying inside the pool and passing a\n"
                     "              freed voice on, a paused world holding its sounds, and both components\n"
                     "              surviving a save and a load." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }

    std::_Exit(0);
