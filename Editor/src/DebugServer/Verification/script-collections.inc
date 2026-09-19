    try
    {
        // The tag the lookup goes looking for, on two of the five entities below.
        constexpr std::uint64_t Tag = 4;

        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        check(Pine::Script::Manager::HasGameAssembly(), "The probe's game assembly did not load");

        auto scriptAsset = dynamic_cast<Pine::CSharpScript*>(
            Pine::Assets::CreateAsset(Pine::AssetType::CSharpScript, "projects/scriptcollections/assets/CollectionTest"));

        check(scriptAsset != nullptr, "Failed to create the script asset");

        scriptAsset->SetTypeName("Game.CollectionTest");

        Pine::Assets::Internal::RegisterAsset(scriptAsset);

        Pine::Script::Manager::ReloadScripts();

        check(scriptAsset->GetScriptData() != nullptr && scriptAsset->GetScriptData()->IsReady,
            "Game.CollectionTest did not resolve against the game assembly");

        // The scene the script reads: a host with two children and a tag, and one more entity
        // carrying the same tag so that a tag lookup has to find both and nothing else.
        auto host = Pine::Entities::Create("Host");

        host->SetTags(Tag);

        host->AddChild(Pine::Entities::Create("ChildA"));
        host->AddChild(Pine::Entities::Create("ChildB"));

        Pine::Entities::Create("Tagged")->SetTags(Tag);
        Pine::Entities::Create("Untagged");

        host->AddComponent<Pine::ScriptComponent>()->SetScript(scriptAsset);

        // Every entity the world holds while the script runs, which is what GetAll() has to
        // report: the five above, their editor entity, and nothing else.
        const auto worldSize = static_cast<int>(Pine::Entities::GetList().size());

        // OnStart is where the script walks all of these, and only a running world dispatches it.
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

        // What the script saw, reported back through its own public fields.
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

        // Entity.Children: two, in the order the engine holds them.
        check(reported("ChildCount") == 2,
            "Entity.Children reported " + std::to_string(reported("ChildCount")) + " children, expected 2");
        check(reported("ChildrenNamed") == 1, "Entity.Children handed back the wrong entities");

        // Entity.GetComponents<T>: the script adds a second Collider to itself first, so this is
        // a case where the count is neither zero nor one.
        check(reported("ColliderCount") == 2,
            "Entity.GetComponents<Collider> reported " + std::to_string(reported("ColliderCount")) + ", expected 2");
        check(reported("CollidersUsable") == 1, "Entity.GetComponents<Collider> handed back unusable components");

        // EntityList.Find(tag) and GetAll().
        check(reported("TaggedCount") == 2,
            "EntityList.Find(tag) reported " + std::to_string(reported("TaggedCount")) + " entities, expected 2");
        check(reported("TaggedNamed") == 1, "EntityList.Find(tag) handed back the wrong entities");
        check(reported("AllCount") == worldSize,
            "EntityList.GetAll() reported " + std::to_string(reported("AllCount")) +
            " entities, expected " + std::to_string(worldSize));
        check(reported("AllFoundHost") == 1, "EntityList.GetAll() did not hand back the entity running the script");

        // A game's own script class is a Script as far as the engine is concerned, which is what
        // resolving the component type through the class hierarchy buys. Before that resolution
        // moved managed-side an unrecognised class silently meant Transform.
        check(reported("FoundSelf") == 1, "GetComponent<T> did not resolve a game script class to its component");
        check(reported("FoundSelfAsScript") == 1, "GetScript<T> did not find the script instance");

        // RayCast returns an empty array here whatever the scene holds - PhysicsRayCast gives
        // PhysX no touch buffer, so nbTouches is always zero (see docs/plans/dotnet-migration.md,
        // 2.6). All this can show is that the count-then-read crossing runs and returns nothing.
        check(reported("RayCastHits") == 0, "Physics3D.RayCast returned hits this scene cannot produce");

        std::cout << "Children, GetComponents, Find(tag), GetAll and RayCast all cross without a managed array."
                  << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;

        std::_Exit(1);
    }
