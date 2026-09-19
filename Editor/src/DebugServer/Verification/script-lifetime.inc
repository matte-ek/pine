    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        check(Pine::Script::Manager::HasGameAssembly(), "The probe's game assembly did not load");

        auto scriptAsset = dynamic_cast<Pine::CSharpScript*>(
            Pine::Assets::CreateAsset(Pine::AssetType::CSharpScript, "projects/scriptlifetime/assets/LifetimeTest"));

        check(scriptAsset != nullptr, "Failed to create the script asset");

        scriptAsset->SetTypeName("Game.LifetimeTest");

        Pine::Assets::Internal::RegisterAsset(scriptAsset);

        Pine::Script::Manager::ReloadScripts();

        check(scriptAsset->GetScriptData() != nullptr && scriptAsset->GetScriptData()->IsReady,
            "Game.LifetimeTest did not resolve against the game assembly");

        // The entity the script holds on to, and the component it holds through it. Both are
        // destroyed out from under the script halfway through, which is the whole point.
        auto watched = Pine::Entities::Create("Watched");

        watched->AddComponent<Pine::Camera>()->SetActive(true);

        Pine::Entities::Create("Host")->AddComponent<Pine::ScriptComponent>()->SetScript(scriptAsset);

        // OnStart is where the script picks the two up, and only a running world dispatches it.
        PlayHandler::Play();

        const auto playing = [&](const char* name)
        {
            for (const auto candidate : Pine::Entities::GetList())
            {
                if (candidate->GetName() == name) return candidate;
            }

            throw std::runtime_error(std::string("The ") + name + " entity is gone");
        };

        auto liveScript = playing("Host")->GetComponent<Pine::ScriptComponent>();

        check(liveScript != nullptr, "Play lost the script component");

        // What the script saw the last time it looked, reported back through its own public fields.
        const auto reported = [&](const char* name)
        {
            for (const auto field : scriptAsset->GetScriptData()->Fields)
            {
                if (field->GetName() != name)
                {
                    continue;
                }

                Pine::ScriptFieldValue value;

                check(field->ReadValue(*liveScript->GetScriptObjectHandle(), value) && value.Data.size() == sizeof(int),
                    std::string("Could not read the ") + name + " field back");

                int result = 0;

                std::memcpy(&result, value.Data.data(), sizeof(result));

                return result;
            }

            throw std::runtime_error(std::string("The script has no ") + name + " field");
        };

        check(reported("WatchedSeen") == 1, "The script did not find the entity and camera it watches");
        check(reported("WatchedValid") == 1, "A live entity did not read as valid from C#");
        check(reported("CameraActive") == 1, "A live camera did not read as active from C#");
        check(reported("SelfValid") == 1, "The script's own entity did not read as valid from C#");

        // The only part of an entity's identity that is not exercised by simply reaching through
        // the mirror: its UId crosses as a value rather than as the array-slot id everything else
        // is addressed by, so nothing else here would notice it arriving empty.
        check(reported("SelfIdValid") == 1, "An entity's UId did not survive the crossing into C#");

        // Both mirrors are still referenced by the script, so they outlive the engine objects they
        // stand for. Whether the script can tell rests on the valid flag the object factory clears
        // as it lets each one go.
        check(Pine::Entities::Delete(playing("Watched")), "Failed to delete the watched entity");

        Pine::Script::Manager::OnUpdate(0.f);

        check(reported("WatchedValid") == 0,
            "A destroyed entity still reads as valid from C#; its mirror was released without being invalidated");
        check(reported("CameraActive") == 0,
            "A destroyed camera still answers from C#; its mirror was released without being invalidated");
        check(reported("SelfValid") == 1, "Destroying one entity invalidated another entity's mirror");

        std::cout << "A destroyed entity and component read as gone from a script still holding them."
                  << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;

        std::_Exit(1);
    }
