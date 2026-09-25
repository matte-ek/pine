    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };

        const auto maxEntityCount = Pine::Engine::GetEngineConfiguration().m_MaxObjectCount;

        // A block keeps its first free slot, its occupied count and the end of iteration up to date
        // as slots change. Each of them has to agree with a scan of the whole block.
        const auto checkBlock = [&](Pine::ComponentType type)
        {
            const auto& block = Pine::Components::GetData(type);
            const auto capacity = block.m_ComponentArrayAllocatedCount;
            const std::string name = Pine::ComponentTypeToString(type);

            int highestIndex = -1;
            std::uint32_t firstFreeIndex = capacity;
            std::uint32_t occupiedCount = 0;

            for (std::uint32_t i = 0; i < capacity; i++)
            {
                if (block.m_ComponentOccupationArray[i])
                {
                    occupiedCount++;
                    highestIndex = static_cast<int>(i) + 1;
                }
                else if (firstFreeIndex == capacity)
                {
                    firstFreeIndex = i;
                }
            }

            check(block.m_HighestComponentIndex == highestIndex,
                fmt::format("{} block ends iteration at {}, but its highest slot says {}",
                    name, block.m_HighestComponentIndex, highestIndex));
            check(block.GetAvailableIndex() == firstFreeIndex,
                fmt::format("{} block hands out slot {}, but the first free slot is {}",
                    name, block.GetAvailableIndex(), firstFreeIndex));
            check(block.m_OccupiedCount == occupiedCount,
                fmt::format("{} block counts {} occupied slots, but {} are", name, block.m_OccupiedCount, occupiedCount));
            check(Pine::Components::GetFreeSlotCount(type) == capacity - occupiedCount,
                fmt::format("{} block reports the wrong number of free slots", name));
        };

        const auto countIterated = [](auto& block)
        {
            std::uint32_t count = 0;

            for ([[maybe_unused]] auto& component : block)
            {
                count++;
            }

            return count;
        };

        // Returns the first free entity slot, which is where the next entity has to land.
        const auto checkEntities = [&]()
        {
            std::uint32_t firstFreeSlot = maxEntityCount;
            std::uint32_t occupiedCount = 0;

            for (std::uint32_t i = 0; i < maxEntityCount; i++)
            {
                if (Pine::Entities::GetByInternalId(i) != nullptr)
                {
                    occupiedCount++;
                }
                else if (firstFreeSlot == maxEntityCount)
                {
                    firstFreeSlot = i;
                }
            }

            check(occupiedCount == Pine::Entities::GetList().size(), "The entity list and the occupied entity slots disagree");
            check(Pine::Entities::GetFreeSlotCount() == maxEntityCount - occupiedCount,
                "Entities::GetFreeSlotCount does not match the occupied entity slots");

            return firstFreeSlot;
        };

        const auto checkAll = [&]()
        {
            checkBlock(Pine::ComponentType::Transform);
            checkBlock(Pine::ComponentType::ModelRenderer);
            checkBlock(Pine::ComponentType::Camera);

            return checkEntities();
        };

        const auto createEntity = [&](const char* name)
        {
            const auto expectedSlot = checkEntities();
            const auto entity = Pine::Entities::Create(name);

            check(entity->GetInternalId() == expectedSlot,
                fmt::format("A new entity landed in slot {}, not in the first free slot {}", entity->GetInternalId(), expectedSlot));

            return entity;
        };

        checkAll();

        // Random creates and deletes, so slots are freed below, at and above the top, and reused.
        std::mt19937 random(1234);
        std::vector<Pine::Entity*> probeEntities;

        for (int step = 0; step < 1500; step++)
        {
            const bool create = probeEntities.empty() || random() % 5 < 3;

            if (create)
            {
                const auto entity = createEntity("Stress");

                if (random() % 2 == 0)
                {
                    entity->AddComponent<Pine::ModelRenderer>();
                }

                probeEntities.push_back(entity);
            }
            else
            {
                // Half the time the newest entity, which is usually the one in the top slot.
                const auto index = random() % 2 == 0 ? probeEntities.size() - 1 : random() % probeEntities.size();

                check(Pine::Entities::Delete(probeEntities[index]), "Failed to delete a probe entity");

                probeEntities.erase(probeEntities.begin() + static_cast<std::ptrdiff_t>(index));
            }

            checkAll();
        }

        check(countIterated(Pine::Components::Get<Pine::Transform>(true)) == Pine::Components::GetData(Pine::ComponentType::Transform).m_OccupiedCount,
            "Iterating the Transform block does not visit every occupied slot");

        // DeleteAll only walks entity slots up to the highest occupied one, so one that was left too
        // low would leave entities behind.
        Pine::Entities::DeleteAll();
        probeEntities.clear();

        checkAll();

        for (const auto entity : Pine::Entities::GetList())
        {
            check(entity->GetTemporary(), "DeleteAll left a scene entity behind");
        }

        // The editor-tool plan: a scatter parents its output under one entity, removed in one go.
        const auto trees = createEntity("Trees");

        for (int i = 0; i < 3000; i++)
        {
            const auto tree = Pine::Entities::Create("Tree");

            tree->AddComponent<Pine::ModelRenderer>();
            trees->AddChild(tree);
        }

        checkAll();

        check(Pine::Entities::Delete(trees), "Failed to delete the Trees parent");

        checkAll();

        for (const auto entity : Pine::Entities::GetList())
        {
            check(entity->GetName() != "Tree", "Deleting the Trees parent left a child behind");
        }

        std::cout << "Pool bookkeeping matched a full scan after every create and delete." << std::endl;

        // C# asking for more than fits has to get null back, not an exception unwinding into it.
        auto scriptAsset = dynamic_cast<Pine::CSharpScript*>(
            Pine::Assets::CreateAsset(Pine::AssetType::CSharpScript, "projects/pools/assets/CapacityTest"));

        check(scriptAsset != nullptr, "Failed to create the script asset");
        check(Pine::Script::Manager::HasGameAssembly(), "The probe's game assembly did not load");

        scriptAsset->SetTypeName("Game.CapacityTest");

        Pine::Assets::Internal::RegisterAsset(scriptAsset);
        Pine::Script::Manager::ReloadScripts();

        check(scriptAsset->GetScriptData() != nullptr && scriptAsset->GetScriptData()->IsReady,
            "Game.CapacityTest did not resolve against the game assembly");

        const auto host = Pine::Entities::Create("Host");
        const auto script = host->AddComponent<Pine::ScriptComponent>();

        script->SetScript(scriptAsset);

        // Only a running world dispatches scripts. Started before the pools fill, so the level
        // snapshot it takes stays small.
        PlayHandler::Play();

        const auto reported = [&](const char* name)
        {
            for (const auto field : scriptAsset->GetScriptData()->Fields)
            {
                if (field->GetName() != name)
                {
                    continue;
                }

                Pine::ScriptFieldValue value;

                check(field->ReadValue(*script->GetScriptObjectHandle(), value) && value.Data.size() == sizeof(int),
                    std::string("Could not read the ") + name + " field back");

                int result = 0;

                std::memcpy(&result, value.Data.data(), sizeof(result));

                return result;
            }

            throw std::runtime_error(std::string("The script has no ") + name + " field");
        };

        std::vector<Pine::Entity*> fillers;

        while (Pine::Entities::GetFreeSlotCount() > 0)
        {
            fillers.push_back(Pine::Entities::Create("Filler"));
        }

        // Camera has a fixed block of its own, so it fills long before the entity cap.
        for (const auto filler : fillers)
        {
            if (Pine::Components::GetFreeSlotCount(Pine::ComponentType::Camera) == 0)
            {
                break;
            }

            filler->AddComponent<Pine::Camera>();
        }

        checkAll();

        check(Pine::Components::GetFreeSlotCount(Pine::ComponentType::Transform) == 0, "The Transform block did not fill with the entities");

        Pine::Script::Manager::OnUpdate(0.f);

        check(reported("Attempts") == 1, "The script did not run");
        check(reported("EntityCreated") == 0, "Entity.Create returned an entity with every entity slot taken");
        check(reported("CameraAdded") == 0, "AddComponent<Camera>() returned a component with every Camera slot taken");
        check(Pine::Entities::GetFreeSlotCount() == 0 && Pine::Components::GetFreeSlotCount(Pine::ComponentType::Camera) == 0,
            "A refused create changed the pools anyway");

        std::cout << "C# got null back from creating past the cap, and the editor kept running." << std::endl;

        for (const auto filler : fillers)
        {
            check(Pine::Entities::Delete(filler), "Failed to delete a filler entity");
        }

        checkAll();

        // The same calls succeed again once there is room.
        Pine::Script::Manager::OnUpdate(0.f);

        check(reported("Attempts") == 2, "The script did not run a second time");
        check(reported("EntityCreated") == 1, "Entity.Create still failed after slots were freed");
        check(reported("CameraAdded") == 1, "AddComponent<Camera>() still failed after slots were freed");

        checkAll();

        std::cout << "Creating from C# worked again once slots were free." << std::endl;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;

        std::_Exit(1);
    }
