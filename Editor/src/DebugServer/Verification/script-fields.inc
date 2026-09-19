    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        check(Pine::Script::Manager::HasGameAssembly(), "The probe's game assembly did not load");

        // The script asset the component points at. Created here rather than shipped as a .passet,
        // so the probe's data directory stays readable.
        auto scriptAsset = dynamic_cast<Pine::CSharpScript*>(
            Pine::Assets::CreateAsset(Pine::AssetType::CSharpScript, "projects/scriptfields/assets/FieldTest"));

        check(scriptAsset != nullptr, "Failed to create the script asset");

        scriptAsset->SetTypeName("Game.FieldTest");

        Pine::Assets::Internal::RegisterAsset(scriptAsset);

        // Resolve it against the assembly the editor loaded at boot, which happened before this
        // asset existed.
        Pine::Script::Manager::ReloadScripts();

        check(scriptAsset->GetScriptData() != nullptr && scriptAsset->GetScriptData()->IsReady,
            "Game.FieldTest did not resolve against the game assembly");

        const auto fields = [&]() -> const std::vector<Pine::ScriptField*>&
        {
            return scriptAsset->GetScriptData()->Fields;
        };

        // Looked up by name every time rather than held, because a hot reload replaces every
        // ScriptField in the list with a new one.
        const auto fieldOf = [&](const char* name)
        {
            for (const auto field : fields())
            {
                if (field->GetName() == name)
                {
                    return field;
                }
            }

            throw std::runtime_error(std::string("The script has no reflected field named ") + name);
        };

        // A double is not a type the engine can store, so it has to be left out of the reflection
        // entirely rather than reflected as something nothing downstream knows what to do with.
        for (const auto field : fields())
        {
            check(field->GetType() != Pine::ScriptFieldType::Invalid,
                "An unsupported field type was reflected: " + field->GetName());
        }

        check(fields().size() == 9,
            "Expected nine reflected fields, got " + std::to_string(fields().size()));

        // A concrete asset type has to classify through the Asset base, since matching on the type
        // name would only ever recognise a field declared as Asset itself.
        check(fieldOf("Prop")->GetType() == Pine::ScriptFieldType::Asset,
            "A Model field was not recognised as an asset reference");
        check(fieldOf("Prop")->GetAssetType() == Pine::AssetType::Model,
            "A Model field did not report the asset type it is declared as");
        check(fieldOf("Target")->GetType() == Pine::ScriptFieldType::Entity,
            "An Entity field was not recognised");

        auto model = Pine::Assets::GetAssetByPath("engine/primitive/cube");

        check(model != nullptr, "The probe needs an engine model to reference");

        const auto assetValue = [](const char* name, const Pine::Asset* asset)
        {
            Pine::ScriptFieldValue value;

            value.Name = name;
            value.Type = Pine::ScriptFieldType::Asset;

            const auto& id = asset->GetUId();
            const auto bytes = reinterpret_cast<const std::byte*>(&id);

            value.Data.assign(bytes, bytes + sizeof(Pine::UId));

            return value;
        };

        const std::string label = "eight pages";

        const auto author = [&](Pine::ScriptComponent* component)
        {
            const auto object = mono_gchandle_get_target(component->GetScriptObjectHandle()->Handle);

            check(object != nullptr, "The script component has no managed object to author");

            Pine::ScriptFieldValue text;

            text.Name = "Label";
            text.Type = Pine::ScriptFieldType::String;
            text.Data.assign(reinterpret_cast<const std::byte*>(label.data()),
                             reinterpret_cast<const std::byte*>(label.data()) + label.size());

            fieldOf("Flag")->Set(object, true);
            fieldOf("Count")->Set(object, 8);
            fieldOf("Speed")->Set(object, 3.5f);
            fieldOf("Offset")->Set(object, Pine::Vector2f(1.f, 2.f));
            fieldOf("Origin")->Set(object, Pine::Vector3f(3.f, 4.f, 5.f));
            fieldOf("Tint")->Set(object, Pine::Vector4f(6.f, 7.f, 8.f, 9.f));
            fieldOf("Label")->WriteValue(object, text);
            fieldOf("Prop")->WriteValue(object, assetValue("Prop", model));
        };

        const auto expect = [&](Pine::ScriptComponent* component, const std::string& stage)
        {
            const auto failure = [&](const std::string& what)
            {
                return stage + " did not preserve " + what;
            };

            const auto object = mono_gchandle_get_target(component->GetScriptObjectHandle()->Handle);

            check(object != nullptr, stage + " left the component without a managed object");

            check(fieldOf("Flag")->Get<bool>(object), failure("a bool"));
            check(fieldOf("Count")->Get<int>(object) == 8, failure("an int"));
            check(fieldOf("Speed")->Get<float>(object) == 3.5f, failure("a float"));
            check(fieldOf("Offset")->Get<Pine::Vector2f>(object) == Pine::Vector2f(1.f, 2.f), failure("a Vector2"));
            check(fieldOf("Origin")->Get<Pine::Vector3f>(object) == Pine::Vector3f(3.f, 4.f, 5.f), failure("a Vector3"));
            check(fieldOf("Tint")->Get<Pine::Vector4f>(object) == Pine::Vector4f(6.f, 7.f, 8.f, 9.f), failure("a Vector4"));

            Pine::ScriptFieldValue value;

            check(fieldOf("Label")->ReadValue(object, value), failure("a string"));
            check(std::string(reinterpret_cast<const char*>(value.Data.data()), value.Data.size()) == label,
                failure("a string"));

            check(fieldOf("Prop")->ReadValue(object, value), failure("an asset reference"));
            check(value.Data == assetValue("Prop", model).Data, failure("an asset reference"));
        };

        const auto component = [&]()
        {
            for (const auto entity : Pine::Entities::GetList())
            {
                if (entity->GetName() != "FieldTest")
                {
                    continue;
                }

                if (const auto found = entity->GetComponent<Pine::ScriptComponent>())
                {
                    return found;
                }
            }

            throw std::runtime_error("The FieldTest entity is gone");
        };

        auto entity = Pine::Entities::Create("FieldTest");

        entity->AddComponent<Pine::ScriptComponent>()->SetScript(scriptAsset);

        author(component());
        expect(component(), "Authoring");

        // 1. A hot reload, which resets the whole Mono domain. Nothing but the component itself
        //    holds the values across it.
        Pine::Script::Manager::ReloadGameAssembly();
        expect(component(), "A hot reload");

        // 2. Play then stop, which the editor implements as a level snapshot taken and restored
        //    through the same SaveData/LoadData the file format goes through.
        PlayHandler::Play();
        PlayHandler::Stop();
        expect(component(), "Play and stop");

        // 3. A level written to disk and read back, which is the path an author actually cares
        //    about, and the only one that exercises the serializer's own encoding.
        auto level = dynamic_cast<Pine::Level*>(
            Pine::Assets::CreateAsset(Pine::AssetType::Level, "projects/scriptfields/assets/fieldtest"));

        check(level != nullptr, "Failed to create the level asset");

        Pine::Assets::Internal::RegisterAsset(level);

        level->CreateFromWorld();
        level->SaveToFile();

        Pine::Entities::DeleteAll();

        auto reloaded = dynamic_cast<Pine::Level*>(Pine::Asset::LoadFromFile(level->GetFilePath()));

        check(reloaded != nullptr, "The saved level did not load back");

        reloaded->Load();

        expect(component(), "A level save and load");

        std::cout << "Script field persistence verified across authoring, hot reload, "
                     "play/stop and a level round trip." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;

        std::_Exit(1);
    }
