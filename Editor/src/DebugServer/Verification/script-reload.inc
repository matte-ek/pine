    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        check(Pine::Script::Manager::HasGameAssembly(), "The probe's game assembly did not load");

        const auto registerScript = [&](const char* path, const char* typeName)
        {
            auto asset = dynamic_cast<Pine::CSharpScript*>(
                Pine::Assets::CreateAsset(Pine::AssetType::CSharpScript, path));

            check(asset != nullptr, std::string("Failed to create the script asset for ") + typeName);

            asset->SetTypeName(typeName);

            Pine::Assets::Internal::RegisterAsset(asset);

            return asset;
        };

        // Four classes rather than one, because a reload has to let go of every class it resolved
        // and not merely the last. Two of them derive from each other, which is the case where one
        // class' reflection reaches into another's.
        auto holderAsset = registerScript("projects/scriptreload/assets/Holder", "Game.Holder");
        auto derivedAsset = registerScript("projects/scriptreload/assets/DerivedHolder", "Game.DerivedHolder");
        auto tickerAsset = registerScript("projects/scriptreload/assets/Ticker", "Game.Ticker");
        auto throwerAsset = registerScript("projects/scriptreload/assets/Thrower", "Game.Thrower");

        Pine::Script::Manager::ReloadScripts();

        const auto ready = [&](const Pine::CSharpScript* asset)
        {
            return asset->GetScriptData() != nullptr && asset->GetScriptData()->IsReady;
        };

        check(ready(holderAsset) && ready(derivedAsset) && ready(tickerAsset) && ready(throwerAsset),
            "Not every probe script resolved against the game assembly");

        // The scene the scripts reach into: an entity with a component to hold, and one script
        // component per class.
        Pine::Entities::Create("Watched")->AddComponent<Pine::Camera>()->SetActive(true);

        Pine::Entities::Create("Ticker")->AddComponent<Pine::ScriptComponent>()->SetScript(tickerAsset);
        Pine::Entities::Create("Holder")->AddComponent<Pine::ScriptComponent>()->SetScript(holderAsset);
        Pine::Entities::Create("Derived")->AddComponent<Pine::ScriptComponent>()->SetScript(derivedAsset);
        Pine::Entities::Create("Thrower")->AddComponent<Pine::ScriptComponent>()->SetScript(throwerAsset);

        const auto entityNamed = [&](const char* name)
        {
            for (const auto candidate : Pine::Entities::GetList())
            {
                if (candidate->GetName() == name) return candidate;
            }

            throw std::runtime_error(std::string("The ") + name + " entity is gone");
        };

        const auto scriptOn = [&](const char* name)
        {
            const auto component = entityNamed(name)->GetComponent<Pine::ScriptComponent>();

            check(component != nullptr, std::string("The ") + name + " entity lost its script component");

            return component;
        };

        // Only a running world dispatches OnStart, which is where each script picks up the mirrors
        // it then holds across every reload below.
        PlayHandler::Play();

        const auto tick = [&](int frames)
        {
            for (int frame = 0; frame < frames; frame++)
            {
                Pine::Script::Manager::OnUpdate(1.f / 60.f);
                Pine::Script::Manager::OnRender(1.f / 60.f);
            }
        };

        tick(4);

        // What a script reported back through one of its own int fields the last time it ran.
        const auto reported = [&](const char* entity, const char* field)
        {
            const auto component = scriptOn(entity);
            const auto scriptData = component->GetScript()->GetScriptData();

            for (const auto candidate : scriptData->Fields)
            {
                if (candidate->GetName() != field)
                {
                    continue;
                }

                Pine::ScriptFieldValue value;

                check(candidate->ReadValue(*component->GetScriptObjectHandle(), value)
                    && value.Data.size() == sizeof(int),
                    std::string("Could not read ") + entity + "." + field + " back");

                int result = 0;

                std::memcpy(&result, value.Data.data(), sizeof(result));

                return result;
            }

            throw std::runtime_error(std::string(entity) + " has no " + field + " field");
        };

        check(reported("Holder", "Acquired") == 1,
            "The holder script did not pick up the entity, component, asset and peer script it holds");
        check(reported("Derived", "Acquired") == 1,
            "The derived holder script did not pick up what it holds");

        // OnRender comes from the renderer rather than from World, so it is checked through the
        // real call site once, on top of the direct dispatch tick() does.
        {
            const auto rendersBefore = reported("Ticker", "Renders");

            Pine::RenderManager::Run();

            check(reported("Ticker", "Renders") == rendersBefore + 1,
                "RenderManager::Run did not dispatch OnRender to a running script exactly once");
        }

        // Pine.Core.GameAssembly.Unload says this when the context it just unloaded is still alive
        // two collections later. It is the only outward sign of a reload that leaked, so the probe
        // reads the engine's own log for it rather than inferring anything from memory alone.
        std::uint64_t seenSequence = 0;

        const auto leakReported = [&]()
        {
            std::string found;

            for (const auto& message : Pine::Log::GetLogSnapshot())
            {
                if (message.Sequence <= seenSequence)
                {
                    continue;
                }

                seenSequence = message.Sequence;

                if (message.Message.find("did not unload") != std::string::npos)
                {
                    found = message.Message;
                }
            }

            return found;
        };

        leakReported();

        const auto residentKilobytes = []()
        {
            std::ifstream statm("/proc/self/statm");

            long total = 0;
            long resident = 0;

            statm >> total >> resident;

            return resident * (sysconf(_SC_PAGESIZE) / 1024);
        };

        const auto startingMemory = residentKilobytes();

        // Thirty in a row, which is a morning of iterating on a script. A single missed reference
        // makes the first one fail; a reference that accumulates - a cache that grows, a handle
        // taken per instance - takes several before it shows.
        constexpr int reloadCount = 30;

        for (int iteration = 1; iteration <= reloadCount; iteration++)
        {
            Pine::Script::Manager::ReloadGameAssembly();

            const auto leak = leakReported();

            check(leak.empty(), "Reload " + std::to_string(iteration) + " leaked its assembly: " + leak);

            // The rebuilt instances have to be reachable and runnable, not merely present: a
            // reload that quietly stopped dispatching would otherwise pass every check below.
            tick(4);

            check(reported("Holder", "Acquired") == 1,
                "The holder script lost what it holds after reload " + std::to_string(iteration));
            check(reported("Ticker", "Ticks") > 0,
                "A script stopped running after reload " + std::to_string(iteration));
            check(reported("Ticker", "Renders") > 0,
                "A script stopped rendering after reload " + std::to_string(iteration));
            check(reported("Thrower", "Attempts") > 0,
                "The throwing script stopped running after reload " + std::to_string(iteration));

            // Something for the scene to have changed between reloads, so no two of them see the
            // same set of mirrors.
            auto spawned = Pine::Entities::Create("Spawned");

            spawned->AddComponent<Pine::ScriptComponent>()->SetScript(tickerAsset);

            tick(1);

            check(Pine::Entities::Delete(spawned), "Failed to delete the spawned entity");
        }

        const auto endingMemory = residentKilobytes();

        std::cout << "Resident set " << startingMemory << " kB before " << reloadCount
                  << " reloads, " << endingMemory << " kB after (" << (endingMemory - startingMemory)
                  << " kB)." << std::endl;

        // Stopping play restores the level from the snapshot taken on Play, which destroys and
        // rebuilds every script instance through a different path than a reload does. A reload
        // after that is the one place the two interact.
        PlayHandler::Stop();

        // A stopped editor renders every frame but must not run scripts in it.
        {
            const auto rendersBefore = reported("Ticker", "Renders");

            Pine::RenderManager::Run();

            check(reported("Ticker", "Renders") == rendersBefore,
                "RenderManager::Run dispatched OnRender with play mode stopped");
        }

        Pine::Script::Manager::ReloadGameAssembly();

        const auto leakAfterStop = leakReported();

        check(leakAfterStop.empty(), "A reload after stopping play leaked its assembly: " + leakAfterStop);

        check(scriptOn("Holder")->GetScriptObjectHandle()->IsValid(),
            "A reload after stopping play left a script component without a managed object");

        std::cout << reloadCount << " hot reloads in a row unloaded the game assembly every time."
                  << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;

        std::_Exit(1);
    }
