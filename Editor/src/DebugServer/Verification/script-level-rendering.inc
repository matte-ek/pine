    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        check(Pine::Script::Manager::HasGameAssembly(), "The probe's game assembly did not load");

        auto scriptAsset = dynamic_cast<Pine::CSharpScript*>(
            Pine::Assets::CreateAsset(Pine::AssetType::CSharpScript, "projects/levelrendering/assets/LevelRenderingTest"));

        check(scriptAsset != nullptr, "Failed to create the script asset");

        scriptAsset->SetTypeName("Game.LevelRenderingTest");

        Pine::Assets::Internal::RegisterAsset(scriptAsset);

        Pine::Script::Manager::ReloadScripts();

        check(scriptAsset->GetScriptData() != nullptr && scriptAsset->GetScriptData()->IsReady,
            "Game.LevelRenderingTest did not resolve against the game assembly");

        // The editor boots into an untitled level that was never registered, which is the case the
        // bindings have to find through World rather than the asset registry.
        auto activeLevel = Pine::World::GetActiveLevel();

        check(activeLevel != nullptr, "The editor has no active level");
        check(Pine::Assets::GetAssetByUId(activeLevel->GetUId()) != activeLevel,
            "The active level is registered; this probe expects the untitled one");

        // A second, registered level, which the script reaches by path. Its writes must land on it
        // and not on the active level.
        auto otherLevel = dynamic_cast<Pine::Level*>(
            Pine::Assets::CreateAsset(Pine::AssetType::Level, "projects/levelrendering/assets/other"));

        check(otherLevel != nullptr, "Failed to create the second level");

        Pine::Assets::Internal::RegisterAsset(otherLevel);

        check(otherLevel->GetPath() == "other",
            "The second level is registered under " + otherLevel->GetPath() + ", not the path the script uses");

        otherLevel->GetLevelSettings().FogDensity = 0.4f;

        // What the script has to read back. Different from every default, so a binding that
        // returns a fresh LevelSettings' value cannot pass.
        auto& settings = activeLevel->GetLevelSettings();

        settings.AmbientColor = Pine::Vector3f(0.1f, 0.2f, 0.3f);
        settings.FogColor = Pine::Vector4f(0.4f, 0.5f, 0.6f, 1.f);
        settings.FogDensity = 0.045f;
        settings.FogHeight = 2.5f;
        settings.FogHeightFalloff = 0.25f;
        settings.Exposure = 1.5f;
        settings.BloomThreshold = 2.f;
        settings.BloomIntensity = 0.75f;
        settings.GrainStrength = 0.125f;
        settings.VignetteStrength = 0.625f;

        const auto authored = settings;

        auto entity = Pine::Entities::Create("Driver");
        entity->AddComponent<Pine::ScriptComponent>()->SetScript(scriptAsset);

        PlayHandler::Play();

        const auto playing = [&](const char* name)
        {
            for (const auto candidate : Pine::Entities::GetList())
            {
                if (candidate->GetName() == name) return candidate;
            }

            throw std::runtime_error(std::string("The ") + name + " entity is gone");
        };

        auto liveScript = playing("Driver")->GetComponent<Pine::ScriptComponent>();

        check(liveScript != nullptr, "Play lost the script component");

        const auto readBack = [&]()
        {
            for (const auto field : scriptAsset->GetScriptData()->Fields)
            {
                if (field->GetName() == "ReadBackOk")
                {
                    Pine::ScriptFieldValue value;

                    check(field->ReadValue(*liveScript->GetScriptObjectHandle(), value) && value.Data.size() == 1,
                        "Could not read the ReadBackOk field back");

                    return value.Data.front() != std::byte{0};
                }
            }

            throw std::runtime_error("The script has no ReadBackOk field");
        };

        check(readBack(), "C# read back the wrong level rendering settings");

        // Every property the script wrote, on the level the renderer reads.
        check(settings.AmbientColor == Pine::Vector3f(0.05f, 0.04f, 0.03f), "An ambient colour written from C# did not land");
        check(settings.FogColor == Pine::Vector4f(0.02f, 0.02f, 0.03f, 1.f), "A fog colour written from C# did not land");
        check(settings.FogDensity == 0.12f, "A fog density written from C# did not land");

        // Both were written negative: a height is a position and lands as is, a falloff is clamped.
        check(settings.FogHeight == -4.f, "A negative fog height written from C# did not land");
        check(settings.FogHeightFalloff == 0.f, "A negative fog height falloff was not clamped to 0");

        check(settings.Exposure == 0.8f, "An exposure written from C# did not land");
        check(settings.BloomThreshold == 3.f, "A bloom threshold written from C# did not land");
        check(settings.BloomIntensity == 0.2f, "A bloom intensity written from C# did not land");
        check(settings.VignetteStrength == 0.9f, "A vignette strength written from C# did not land");

        // The script wrote a negative grain strength, which the binding clamps.
        check(settings.GrainStrength == 0.f, "A negative grain strength was not clamped to 0");

        check(otherLevel->GetLevelSettings().FogDensity == 0.7f, "A write to the second level did not land on it");

        // One rendered frame, so the values have gone through the passes that read them.
        Pine::RenderManager::Run();

        PlayHandler::Stop();

        // Stop has to put back what the level had before play, for every setting.
        check(settings.AmbientColor == authored.AmbientColor, "Stop did not restore the ambient colour");
        check(settings.FogColor == authored.FogColor, "Stop did not restore the fog colour");
        check(settings.FogDensity == authored.FogDensity, "Stop did not restore the fog density");
        check(settings.FogHeight == authored.FogHeight, "Stop did not restore the fog height");
        check(settings.FogHeightFalloff == authored.FogHeightFalloff, "Stop did not restore the fog height falloff");
        check(settings.Exposure == authored.Exposure, "Stop did not restore the exposure");
        check(settings.BloomThreshold == authored.BloomThreshold, "Stop did not restore the bloom threshold");
        check(settings.BloomIntensity == authored.BloomIntensity, "Stop did not restore the bloom intensity");
        check(settings.GrainStrength == authored.GrainStrength, "Stop did not restore the grain strength");
        check(settings.VignetteStrength == authored.VignetteStrength, "Stop did not restore the vignette strength");

        std::cout << "Level.Active.Rendering is readable and writable from C#, and play mode restores it." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;

        std::_Exit(1);
    }
