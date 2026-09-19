    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        check(Pine::Script::Manager::HasGameAssembly(), "The probe's game assembly did not load");

        auto scriptAsset = dynamic_cast<Pine::CSharpScript*>(
            Pine::Assets::CreateAsset(Pine::AssetType::CSharpScript, "projects/scriptcomponents/assets/ComponentTest"));

        check(scriptAsset != nullptr, "Failed to create the script asset");

        scriptAsset->SetTypeName("Game.ComponentTest");

        Pine::Assets::Internal::RegisterAsset(scriptAsset);

        Pine::Script::Manager::ReloadScripts();

        check(scriptAsset->GetScriptData() != nullptr && scriptAsset->GetScriptData()->IsReady,
            "Game.ComponentTest did not resolve against the game assembly");

        // The clip the script looks up and assigns. Imported rather than created empty, so its
        // duration is a real number the managed side can be checked against.
        auto clipContext = Pine::Importer::CreateContext();

        Pine::Importer::AddFile(clipContext, "projects/scriptcomponents/content/tone.wav",
            "projects/scriptcomponents/assets/tone");
        Pine::Importer::Run(clipContext);

        check(!clipContext->Imports.empty() &&
              clipContext->Imports.front()->ImportStatus == Pine::AssetImportStatus::Imported,
            "The probe's clip did not import");

        auto clip = dynamic_cast<Pine::AudioFile*>(clipContext->Imports.front()->AssetPtr);

        Pine::Importer::DeleteContext(clipContext);

        check(clip != nullptr, "The probe's clip did not import as an AudioFile");

        auto entity = Pine::Entities::Create("Bound");

        auto light = entity->AddComponent<Pine::Light>();
        auto camera = entity->AddComponent<Pine::Camera>();
        auto collider = entity->AddComponent<Pine::Collider>();
        auto audioSource = entity->AddComponent<Pine::AudioSource>();
        auto audioListener = entity->AddComponent<Pine::AudioListener>();

        // What the script has to read back. Chosen to be different from every default, so a
        // binding that silently returns a fresh component's value cannot pass.
        light->SetLightType(Pine::LightType::SpotLight);
        light->SetLightIntensity(2.5f);
        light->SetRange(12.f);
        light->SetCastShadows(false);

        camera->SetFieldOfView(80.f);
        camera->SetFarPlane(500.f);

        collider->SetColliderType(Pine::ColliderType::Sphere);
        collider->SetIsTrigger(true);
        collider->SetLayer(4);

        // Left without a clip on purpose: the script assigns one, which is the only way to check
        // that the asset half of the binding works in both directions.
        audioSource->SetLoop(true);
        audioSource->SetSpatial(false);
        audioSource->SetPitch(1.5f);

        audioListener->SetVolume(0.25f);

        entity->AddComponent<Pine::ScriptComponent>()->SetScript(scriptAsset);

        // OnStart is where the script does its reading and writing, and only a running world
        // dispatches it.
        PlayHandler::Play();

        const auto playing = [&](const char* name)
        {
            for (const auto candidate : Pine::Entities::GetList())
            {
                if (candidate->GetName() == name) return candidate;
            }

            throw std::runtime_error(std::string("The ") + name + " entity is gone");
        };

        auto liveEntity = playing("Bound");
        auto liveScript = liveEntity->GetComponent<Pine::ScriptComponent>();

        check(liveScript != nullptr, "Play lost the script component");

        // The script reports what it saw through a public field, which is also a second pair of
        // eyes on field reflection working.
        const auto readBack = [&]()
        {
            for (const auto field : scriptAsset->GetScriptData()->Fields)
            {
                if (field->GetName() == "ReadBackOk")
                {
                    return field->Get<bool>(mono_gchandle_get_target(liveScript->GetScriptObjectHandle()->Handle));
                }
            }

            throw std::runtime_error("The script has no ReadBackOk field");
        };

        check(readBack(), "C# read back the wrong values from Light, Camera, Collider, AudioSource or AudioListener");

        // And what the script wrote has to have reached the engine components themselves.
        auto liveLight = liveEntity->GetComponent<Pine::Light>();
        auto liveCamera = liveEntity->GetComponent<Pine::Camera>();
        auto liveCollider = liveEntity->GetComponent<Pine::Collider>();
        auto liveSource = liveEntity->GetComponent<Pine::AudioSource>();
        auto liveListener = liveEntity->GetComponent<Pine::AudioListener>();

        check(liveLight->GetLightColor() == Pine::Vector3f(0.25f, 0.5f, 0.75f), "A light colour written from C# did not land");
        check(liveLight->GetLightIntensity() == 4.f, "A light intensity written from C# did not land");
        check(liveLight->GetSpotlightOuterAngle() == 60.f, "A spotlight angle written from C# did not land");

        check(liveCamera->GetFieldOfView() == 55.f, "A field of view written from C# did not land");
        check(liveCamera->GetCameraType() == Pine::CameraType::Orthographic, "A camera type written from C# did not land");

        check(liveCollider->GetSize() == Pine::Vector3f(2.f, 3.f, 4.f), "A collider size written from C# did not land");
        check(liveCollider->GetLayerMask() == 12u, "A collider layer mask written from C# did not land");

        check(liveSource->GetAudioFile() == clip, "A clip assigned from C# did not land");
        check(liveSource->GetVolume() == 0.75f, "A source volume written from C# did not land");
        check(liveSource->GetMaxDistance() == 80.f, "A source max distance written from C# did not land");
        check(liveSource->GetPlaybackPosition() == 0.2f, "A playback position written from C# did not land");

        // Play() has to reach the component itself, not just set something on the managed mirror.
        // Nothing has run Audio::Update() since, so this is still what the script asked for.
        check(liveSource->IsPlaying(), "Play() called from C# did not reach the component");

        check(liveListener->GetVolume() == 0.5f, "A listener volume written from C# did not land");

        PlayHandler::Stop();

        std::cout << "Light, Camera, Collider, AudioSource and AudioListener are readable and writable from C#." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;

        std::_Exit(1);
    }
