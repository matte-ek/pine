    try
    {
        using nlohmann::json;
        using namespace physx;
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };
        const auto edit = [&](const json& operations, int status = 200)
        {
            Editor::DebugServer::Request request;
            request.Body = json{{"version", 1}, {"operations", operations}}.dump();
            const auto response = Editor::DebugServer::Editing::Edit(request);
            check(response.StatusCode == status, response.Body.dump());
            return response.Body;
        };
        const auto vector = [](float x, float y, float z)
        {
            return json{{"x", x}, {"y", y}, {"z", z}};
        };
        const auto locks = [](bool x, bool y, bool z)
        {
            return json{{"x", x}, {"y", y}, {"z", z}};
        };
        const auto create = [&](const char* name, const json& position, const json& collider, const json& body)
        {
            json components = {
                {{"type", "Transform"}, {"properties", {{"LocalPosition", position}}}},
                {{"type", "Collider"}, {"properties", collider}}
            };
            if (!body.is_null()) components.push_back({{"type", "RigidBody"}, {"properties", body}});
            edit({{{"op", "entity.create"}, {"name", name}, {"components", components}}});
        };
        const auto find = [&](const char* name)
        {
            for (auto entity : Pine::Entities::GetList())
            {
                if (entity->GetName() == name) return entity;
            }
            throw std::runtime_error(std::string("Missing entity ") + name);
        };
        const auto patch = [&](Pine::Component* component, const json& properties)
        {
            return json{{"op", "component.update"}, {"target", {{"id", component->GetId().ToString()}}},
                {"properties", properties}};
        };
        auto scene = Pine::Physics3D::GetScene();
        const auto actor = [&](const char* name) -> PxRigidActor*
        {
            const auto flags = PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC;
            std::vector<PxActor*> actors(scene->getNbActors(flags));
            scene->getActors(flags, actors.data(), actors.size());
            for (auto candidate : actors)
            {
                if (candidate->userData == find(name)) return candidate->is<PxRigidActor>();
            }
            return nullptr;
        };
        const auto shape = [&](const char* name)
        {
            auto owner = actor(name);
            check(owner != nullptr, std::string("Missing actor ") + name);
            PxShape* result = nullptr;
            check(owner->getShapes(&result, 1) == 1, "Missing shape");
            return result;
        };
        const auto noActors = [&]()
        {
            check(scene->getNbActors(PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC) == 0,
                "Stopped scene retained physics actors");
        };
        const auto step = [&](int count)
        {
            for (int i = 0; i < count; ++i) Pine::Physics3D::Update(1.0 / 60.0);
        };

        create("Floor", vector(0, -0.5f, 0), {{"Size", vector(20, 0.5f, 20)}}, nullptr);
        create("Falling", vector(0, 4, 0), {{"Size", vector(0.5f, 0.5f, 0.5f)}},
            {{"Mass", 7}, {"MaxLinearVelocity", 12}, {"MaxAngularVelocity", 3},
             {"RotationLock", locks(true, false, true)}});
        create("Frozen", vector(4, 4, 0), {{"Type", "Sphere"}, {"Size", vector(0.75f, 1, 1)}},
            {{"GravityEnabled", false}, {"PositionLock", locks(true, false, false)},
             {"RotationLock", locks(false, true, false)}});
        create("Kinematic", vector(8, 4, 0), {{"Type", "Capsule"}, {"Size", vector(0.5f, 1.5f, 1)}},
            {{"Type", "Kinematic"}, {"Mass", 5}});
        create("StaticBody", vector(12, 4, 0), json::object(), {{"Type", "Static"}});
        create("StaticEntity", vector(16, 4, 0), json::object(), {{"Type", "Dynamic"}});
        edit({{{"op", "entity.update"}, {"target", {{"id", find("StaticEntity")->GetId().ToString()}}},
            {"properties", {{"static", true}}}}});
        create("Trigger", vector(-4, 4, 0), {{"IsTrigger", true}, {"Layer", 2147483648ULL}}, nullptr);
        create("Visitor", vector(-4, 4, 0), {{"Type", "Sphere"}, {"Layer", 2}}, {{"GravityEnabled", false}});
        create("MutedTrigger", vector(-8, 4, 0), {{"IsTrigger", true}, {"TriggerMask", 0}}, nullptr);
        create("MutedVisitor", vector(-8, 4, 0), {{"Type", "Sphere"}}, {{"GravityEnabled", false}});

        struct TriggerEvents final : PxSimulationEventCallback
        {
            int Entered = 0;
            void onConstraintBreak(PxConstraintInfo*, PxU32) override {}
            void onWake(PxActor**, PxU32) override {}
            void onSleep(PxActor**, PxU32) override {}
            void onContact(const PxContactPairHeader&, const PxContactPair*, PxU32) override {}
            void onAdvance(const PxRigidBody* const*, const PxTransform*, const PxU32) override {}
            void onTrigger(PxTriggerPair* pairs, PxU32 count) override
            {
                for (PxU32 i = 0; i < count; ++i)
                {
                    if (pairs[i].status == PxPairFlag::eNOTIFY_TOUCH_FOUND) ++Entered;
                }
            }
        } events;
        auto previousCallback = scene->getSimulationEventCallback();
        scene->setSimulationEventCallback(&events);
        noActors();
        step(1);
        noActors();
        PlayHandler::Play();
        edit({{{"op", "entity.create"}}}, 409);
        step(1);
        auto falling = find("Falling")->GetComponent<Pine::RigidBody>()->GetRigidBody();
        check(falling != nullptr && std::abs(falling->getMass() - 7.f) < 0.0001f, "Dynamic mass was not applied on creation: " + std::to_string(falling ? falling->getMass() : -1.f));
        check(falling->getMaxLinearVelocity() == 12.f && falling->getMaxAngularVelocity() == 3.f,
            "Velocity limits were not applied");
        check(falling->getRigidDynamicLockFlags() == (PxRigidDynamicLockFlag::eLOCK_ANGULAR_X | PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z),
            "Rotation locks were not applied");
        auto frozen = find("Frozen")->GetComponent<Pine::RigidBody>()->GetRigidBody();
        check(frozen->getActorFlags().isSet(PxActorFlag::eDISABLE_GRAVITY), "Gravity flag was not applied");
        check(frozen->getRigidDynamicLockFlags() == (PxRigidDynamicLockFlag::eLOCK_LINEAR_X | PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y),
            "Position/rotation locks were not applied");
        auto kinematic = find("Kinematic")->GetComponent<Pine::RigidBody>()->GetRigidBody();
        check(kinematic->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC) && std::abs(kinematic->getMass() - 5.f) < 0.0001f,
            "Kinematic configuration was not applied");
        check(actor("Floor")->is<PxRigidStatic>() && actor("StaticBody")->is<PxRigidStatic>()
            && actor("StaticEntity")->is<PxRigidStatic>(), "Static actor policy was not respected");
        const auto box = PxGeometryHolder(shape("Floor")->getGeometry()).box();
        check(box.halfExtents == PxVec3(20, 0.5f, 20), "Box dimensions wrong");
        const auto sphere = PxGeometryHolder(shape("Frozen")->getGeometry()).sphere();
        check(sphere.radius == 0.75f, "Sphere radius wrong");
        const auto capsule = PxGeometryHolder(shape("Kinematic")->getGeometry()).capsule();
        check(capsule.radius == 0.5f && capsule.halfHeight == 1.5f,
            "Capsule dimensions wrong");
        const auto capsuleAxis = shape("Kinematic")->getLocalPose().q.rotate(PxVec3(1, 0, 0));
        check((capsuleAxis - PxVec3(0, 1, 0)).magnitude() < 0.001f, "Capsule is not aligned to Y");
        check(shape("Trigger")->getFlags().isSet(PxShapeFlag::eTRIGGER_SHAPE)
            && !shape("Trigger")->getFlags().isSet(PxShapeFlag::eSIMULATION_SHAPE), "Trigger flags wrong");
        const auto filter = shape("Trigger")->getSimulationFilterData();
        check(filter.word0 == 2147483648U && filter.word1 == 0xffffffffU && filter.word2 == 0xffffffffU,
            "Unsigned filter bits were lost");
        check(events.Entered == 1, "Trigger mask did not select exactly one overlap");
        step(180);
        check(std::abs(find("Falling")->GetTransform()->GetPosition().y - 0.5f) < 0.1f,
            "Falling box did not settle on the floor");
        check(find("Frozen")->GetTransform()->GetPosition().y == 4.f, "Gravity-disabled sphere moved");
        PlayHandler::Pause();
        edit({{{"op", "entity.create"}}}, 409);
        PlayHandler::Stop();
        scene->setSimulationEventCallback(previousCallback);
        noActors();
        check(find("Falling")->GetTransform()->GetPosition().y == 4.f, "Stop did not restore authored position");

        // Stop destroys actors; editing, undoing and redoing must affect their next creation.
        edit({patch(find("Falling")->GetComponent<Pine::Collider>(),
                {{"Type", "Sphere"}, {"Size", vector(1, 1, 1)}}),
            patch(find("Falling")->GetComponent<Pine::RigidBody>(), {{"Mass", 9}})});
        check(Editor::Actions::ExecuteUndo().Applied, "Physics patch undo failed");
        check(find("Falling")->GetComponent<Pine::RigidBody>()->GetMass() == 7.f, "Undo lost mass");
        check(Editor::Actions::ExecuteRedo().Applied, "Physics patch redo failed");
        noActors();
        PlayHandler::Play();
        step(1);
        falling = find("Falling")->GetComponent<Pine::RigidBody>()->GetRigidBody();
        check(std::abs(falling->getMass() - 9.f) < 0.0001f && PxGeometryHolder(shape("Falling")->getGeometry()).sphere().radius == 1.f,
            "Second play reused stale body or shape properties");
        step(180);
        check(std::abs(find("Falling")->GetTransform()->GetPosition().y - 1.f) < 0.1f,
            "Edited sphere did not settle at its new radius");
        PlayHandler::Stop();
        noActors();

        auto fallingEntity = find("Falling");
        edit({{{"op", "component.remove"}, {"target", {{"id", fallingEntity->GetComponent<Pine::RigidBody>()->GetId().ToString()}}}}});
        PlayHandler::Play();
        step(1);
        check(actor("Falling")->is<PxRigidStatic>() != nullptr, "Collider did not become static without a body");
        PlayHandler::Stop();
        fallingEntity = find("Falling");
        edit({{{"op", "component.remove"}, {"target", {{"id", fallingEntity->GetComponent<Pine::Collider>()->GetId().ToString()}}}},
              {{"op", "component.add"}, {"target", {{"id", fallingEntity->GetId().ToString()}}}, {"type", "RigidBody"}}});
        PlayHandler::Play();
        step(1);
        check(actor("Falling") == nullptr, "Body without collider should remain dormant");
        PlayHandler::Stop();
        noActors();
        std::cout << "PASS: actual actors/shapes, primitive dimensions, triggers/masks, mass/gravity/locks/limits,\n"
                     "      falling contacts, play guards, stop/edit/undo/redo/play recreation and component removal.\n";
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        Pine::Physics3D::GetScene()->setSimulationEventCallback(nullptr);
        std::_Exit(1);
    }
