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

            // Stored before models had LOD settings, so this is also the old-file path.
            check(model->GetLodLevels().empty() && model->GetLodCullDistance() == 0.f,
                "A model stored without LOD settings loaded with some");

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

        // Part 4 - what a model saves from memory next to the geometry it recovers: its LOD
        // settings and its embedded materials. Recovering the geometry reads the whole stored
        // payload, and reading an array appends to it, so either one could be written twice.
        {
            {
                std::ofstream obj("projects/resave/content/probe.obj");

                obj << "mtllib probe.mtl\n"
                       "v 0 0 0\n"
                       "v 1 0 0\n"
                       "v 0 1 0\n"
                       "usemtl Red\n"
                       "f 1 2 3\n";

                std::ofstream mtl("projects/resave/content/probe.mtl");

                mtl << "newmtl Red\n"
                       "Kd 1 0 0\n"
                       "map_Kd probe.png\n";
            }

            auto context = Pine::Importer::CreateContext();

            Pine::Importer::AddFile(context, "projects/resave/content/probe.obj", "projects/resave/assets/probe");
            Pine::Importer::Run(context);

            const auto imported = context->Imports.front()->ImportStatus == Pine::AssetImportStatus::Imported
                ? context->Imports.front()->AssetPtr : nullptr;

            Pine::Importer::DeleteContext(context);

            const auto model = dynamic_cast<Pine::Model*>(imported);

            check(model != nullptr, "The model this probe re-saves did not import");

            // Counted in the '.passet' itself, which is what the next run loads. Memory would not
            // show a duplicate until then.
            const auto storedEmbeddedMaterialCount = [&]()
            {
                struct EnvelopeSerializer : Pine::Serialization::Serializer
                {
                    PINE_SERIALIZE_DATA(Data);
                };

                struct PayloadSerializer : Pine::Serialization::Serializer
                {
                    PINE_SERIALIZE_ARRAY(EmbeddedMaterials);
                };

                EnvelopeSerializer envelope;
                PayloadSerializer payload;

                check(envelope.Read(Pine::File::ReadCompressed(model->GetFilePath())),
                    "Could not read the re-saved model's '.passet'");
                check(payload.Read(envelope.Data.Read()),
                    "Could not read the re-saved model's payload");

                return payload.EmbeddedMaterials.GetDataCount();
            };

            check(storedEmbeddedMaterialCount() == 1, "The imported model should embed exactly one material");

            // The settings of the embedded material, read from the model's '.passet' too.
            const auto storedEmbeddedMaterial = [&]()
            {
                struct EnvelopeSerializer : Pine::Serialization::Serializer
                {
                    PINE_SERIALIZE_DATA(Data);
                };

                struct PayloadSerializer : Pine::Serialization::Serializer
                {
                    PINE_SERIALIZE_ARRAY(EmbeddedMaterials);
                };

                struct MaterialSettingsSerializer : Pine::Serialization::Serializer
                {
                    PINE_SERIALIZE_PRIMITIVE(RenderingMode, Pine::Serialization::DataType::Int32);
                    PINE_SERIALIZE_PRIMITIVE(RenderFace, Pine::Serialization::DataType::Int32);
                };

                EnvelopeSerializer modelEnvelope;
                PayloadSerializer payload;
                EnvelopeSerializer materialEnvelope;
                MaterialSettingsSerializer settings;

                check(modelEnvelope.Read(Pine::File::ReadCompressed(model->GetFilePath())) &&
                      payload.Read(modelEnvelope.Data.Read()) &&
                      payload.EmbeddedMaterials.GetDataCount() == 1 &&
                      materialEnvelope.Read(payload.EmbeddedMaterials.GetData(0)) &&
                      settings.Read(materialEnvelope.Data.Read()),
                    "Could not read the embedded material out of the model's '.passet'");

                auto renderingMode = Pine::MaterialRenderingMode::Opaque;
                auto renderFace = Pine::MaterialRenderFace::Default;

                settings.RenderingMode.Read(renderingMode);
                settings.RenderFace.Read(renderFace);

                return std::make_pair(renderingMode, renderFace);
            };

            // An embedded material edited in the editor, and saved the way the editor saves it:
            // the material has no file of its own, so the edit marks its model, and saving all
            // modified assets writes the model.
            {
                const auto material = model->GetMeshes().front()->GetMaterial();

                check(material != nullptr && material->IsEmbedded() && material->GetEmbeddingModel() == model,
                    "The imported model's material does not know the model it is embedded in");
                check(material->GetFilePath().empty(),
                    "An embedded material has a file of its own, which saving all assets would write");
                check(material->GetDiffuse() != nullptr &&
                      material->GetDiffuse()->GetAlphaMode() == Pine::TextureAlphaMode::Cutout,
                    "The model's diffuse map did not import as a cutout");
                check(material->GetRenderingMode() == Pine::MaterialRenderingMode::Discard,
                    "A new embedded material did not take its rendering mode from its diffuse map");

                material->SetRenderingMode(Pine::MaterialRenderingMode::Transparent);
                material->SetRenderFace(Pine::MaterialRenderFace::Both);

                material->GetEmbeddingModel()->SaveToFile();

                check(storedEmbeddedMaterial() == std::make_pair(Pine::MaterialRenderingMode::Transparent,
                                                                 Pine::MaterialRenderFace::Both),
                    "Saving the model did not store the edit to its embedded material");

                model->ReLoad();

                const auto reloaded = model->GetMeshes().front()->GetMaterial();

                check(reloaded == material && reloaded->IsEmbedded(),
                    "Re-saving the model replaced its embedded material");
                check(reloaded->GetRenderingMode() == Pine::MaterialRenderingMode::Transparent &&
                      reloaded->GetRenderFace() == Pine::MaterialRenderFace::Both,
                    "Re-saving the model lost the edit to its embedded material");
            }

            const auto cube = Pine::Assets::Get<Pine::Model>("engine/primitive/cube");
            const auto sphere = Pine::Assets::Get<Pine::Model>("engine/primitive/sphere");

            check(cube != nullptr && sphere != nullptr, "Missing the primitives used as LOD levels");

            std::vector<Pine::ModelLodLevel> levels(2);

            levels[0].LodModel = cube;
            levels[0].Distance = 12.5f;
            levels[1].LodModel = sphere;
            levels[1].Distance = 40.f;

            model->SetLodLevels(levels);
            model->SetLodCullDistance(75.f);

            const auto checkLodSettings = [&](const std::string& when)
            {
                const auto& stored = model->GetLodLevels();

                check(stored.size() == 2, "The LOD levels did not survive " + when);
                check(stored[0].LodModel.Get() == cube && stored[0].Distance == 12.5f &&
                      stored[1].LodModel.Get() == sphere && stored[1].Distance == 40.f,
                    "The LOD levels changed across " + when);
                check(model->GetLodCullDistance() == 75.f, "The LOD cull distance did not survive " + when);
            };

            model->ReLoad();
            checkLodSettings("a re-save");

            model->ReLoad();
            checkLodSettings("a second re-save");

            check(storedEmbeddedMaterialCount() == 1, "Re-saving a model wrote its embedded materials again");

            // A mesh pointed at another material in the editor, then saved. Done before the
            // re-import below, which rightly gives each mesh the source file's material back.
            {
                auto* replacement = dynamic_cast<Pine::Material*>(
                    Pine::Assets::CreateAsset(Pine::AssetType::Material, "projects/resave/assets/replacement"));

                check(replacement != nullptr, "Could not create the replacement material");

                // As every material the editor's picker offers is.
                Pine::Assets::Internal::RegisterAsset(replacement);

                model->GetMeshes().front()->SetMaterial(replacement);

                model->ReLoad();

                check(model->GetMeshes().front()->GetMaterial() == replacement,
                    "Re-saving a model reverted a mesh to the material it was loaded with");

                model->ReLoad();

                check(model->GetMeshes().front()->GetMaterial() == replacement,
                    "A mesh's material reverted on the second re-save");
            }

            // The importer rebuilds the geometry and materials from the source file, and must leave
            // the settings made in the editor alone. Through the importer's Update action, as the
            // editor re-imports - Asset::ReImport hands a Model no import context, which its
            // importer does not handle.
            {
                auto reimportContext = Pine::Importer::CreateContext();

                Pine::Importer::AddFile(reimportContext, "projects/resave/content/probe.obj", "projects/resave/assets/probe");
                Pine::Importer::Resolve(reimportContext);

                const auto& reimport = *reimportContext->Imports.front();

                check(reimport.Action == Pine::AssetImportAction::Update && reimport.AssetPtr == model,
                    "Importing the model's source again did not target the existing model");

                Pine::Importer::Run(reimportContext);

                check(reimport.ImportStatus == Pine::AssetImportStatus::Imported, "Re-importing the model failed");

                Pine::Importer::DeleteContext(reimportContext);
            }

            checkLodSettings("a re-import");

            check(storedEmbeddedMaterialCount() == 1, "Re-importing a model changed how many materials it embeds");

            // The file names the same diffuse map as before, so the mode set in the editor stands,
            // and the file does not ask for both faces, which is no reason to stop drawing them.
            {
                const auto material = model->GetMeshes().front()->GetMaterial();

                check(material != nullptr && material->IsEmbedded() && material->GetEmbeddingModel() == model,
                    "The re-imported model's material does not know the model it is embedded in");
                check(material->GetRenderingMode() == Pine::MaterialRenderingMode::Transparent &&
                      material->GetRenderFace() == Pine::MaterialRenderFace::Both,
                    "Re-importing a model reset the settings made on its embedded material");
                check(storedEmbeddedMaterial() == std::make_pair(Pine::MaterialRenderingMode::Transparent,
                                                                 Pine::MaterialRenderFace::Both),
                    "Re-importing a model stored its embedded material without the settings made on it");
            }
        }

        std::cout << "PASS(native): a texture's mips, a model's meshes and bounds, and an audio "
                     "clip's samples all\n              surviving repeated saves of an asset that "
                     "was not just imported - the path that\n              has to recover them from "
                     "the '.passet' it is about to overwrite - and a model's LOD settings,\n"
                     "              mesh materials and embedded materials saved alongside, once "
                     "each, through re-saves and a re-import,\n              with an edit to an "
                     "embedded material stored by saving its model and kept by a re-import." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }

    std::_Exit(0);
