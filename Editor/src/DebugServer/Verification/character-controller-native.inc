    try
    {
        using nlohmann::json;
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

        // A static box of collision, which is all the world a walker needs to be tested against.
        const auto block = [&](const char* name, const json& position, const json& size)
        {
            json components = {
                {{"type", "Transform"}, {"properties", {{"LocalPosition", position}}}},
                {{"type", "Collider"}, {"properties", {{"Size", size}}}}
            };
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
        const auto step = [&](int count)
        {
            for (int i = 0; i < count; ++i) Pine::Physics3D::Update(1.0 / 60.0);
        };

        block("Floor", vector(0, -0.5f, 0), vector(20, 0.5f, 20));
        block("Wall", vector(2, 1, 0), vector(0.5f, 1, 5));
        block("Ceiling", vector(6, 2.7f, 0), vector(1, 0.5f, 1));

        // /edit cannot author a CharacterController, so the walker is built directly.
        auto player = Pine::Entities::Create("Player");
        player->GetTransform()->SetLocalPosition(Pine::Vector3f(0.f, 1.f, 0.f));
        player->AddComponent<Pine::CharacterController>();

        PlayHandler::Play();

        auto controller = find("Player")->GetComponent<Pine::CharacterController>();
        check(controller != nullptr, "Play lost the CharacterController");

        const auto height = [&]() { return find("Player")->GetTransform()->GetPosition().y; };

        // Drive the controller the way a script does: queue one tick's worth of displacement,
        // then let the tick consume it.
        const auto walk = [&](const Pine::Vector3f& velocity, int ticks)
        {
            for (int i = 0; i < ticks; ++i)
            {
                controller->Move(velocity * (1.f / 60.f));
                step(1);
            }
        };

        // Jump from a standstill, reporting how high it got and how long it stayed up. Both are
        // what a ceiling has to cut short.
        const auto jump = [&](float speed, float& apex, int& airborneTicks)
        {
            const auto start = height();

            controller->SetVerticalVelocity(speed);
            check(controller->GetVerticalVelocity() == speed, "Vertical velocity did not read back");

            apex = 0.f;
            airborneTicks = 0;

            bool hasLeftGround = false;

            for (int tick = 0; tick < 600; ++tick)
            {
                step(1);
                ++airborneTicks;

                apex = std::max(apex, height() - start);

                if (!controller->IsGrounded())
                {
                    hasLeftGround = true;
                }
                else if (hasLeftGround)
                {
                    return true;
                }
            }

            return false;
        };

        // Falling onto the floor, which every later check starts from.
        step(60);
        check(controller->IsGrounded(), "Controller did not land on the floor");
        check(std::abs(height()) < 0.2f, "Controller did not come to rest at the floor surface: " + std::to_string(height()));
        check(std::abs(controller->GetVelocity().y) < 0.5f, "Resting controller reported a falling velocity");

        // Open ground: what was asked for is what was travelled, so the reported velocity matches.
        walk(Pine::Vector3f(-3.f, 0.f, 0.f), 30);
        check(std::abs(controller->GetVelocity().x + 3.f) < 0.2f,
            "Unobstructed walk did not report its own speed: " + std::to_string(controller->GetVelocity().x));
        check(!controller->IsTouchingSides(), "Open ground reported a side contact");

        // A teleport is not travel, so it must not leave a velocity behind to be read back.
        controller->SetPosition(Pine::Vector3f(0.f, 0.5f, 0.f));
        check(controller->GetVelocity() == Pine::Vector3f(0.f), "Teleport kept the previous velocity");
        check(!controller->IsTouchingSides(), "Teleport kept the previous side contact");
        step(30);

        // Into the wall: still asking for 3 units/second, travelling none of it. This is the pair
        // of readings a script integrating its own velocity depends on.
        walk(Pine::Vector3f(3.f, 0.f, 0.f), 60);
        check(controller->IsTouchingSides(), "Walking into a wall did not report a side contact");
        check(std::abs(controller->GetVelocity().x) < 0.3f,
            "Blocked walk reported the speed it was asked for rather than the one it travelled: "
                + std::to_string(controller->GetVelocity().x));

        // A jump in the open: high enough to clear the ceiling's underside, used as the baseline.
        controller->SetPosition(Pine::Vector3f(0.f, 0.2f, 0.f));
        step(30);
        check(controller->IsGrounded(), "Controller did not settle before the open jump");

        float openApex = 0.f;
        int openTicks = 0;
        check(jump(5.f, openApex, openTicks), "Open jump never came back down");
        check(openApex > 1.1f && openApex < 1.5f, "Open jump apex was not the one gravity implies: " + std::to_string(openApex));

        // The same jump under a ceiling: the head contact has to end the climb rather than leave
        // the controller hanging there, spending upward speed it can no longer use.
        controller->SetPosition(Pine::Vector3f(6.f, 0.2f, 0.f));
        step(30);
        check(controller->IsGrounded(), "Controller did not settle under the ceiling");

        float ceilingApex = 0.f;
        int ceilingTicks = 0;
        check(jump(5.f, ceilingApex, ceilingTicks), "Jump under a ceiling never came back down");
        check(ceilingApex < openApex - 0.2f, "Ceiling did not stop the climb: " + std::to_string(ceilingApex));
        check(ceilingTicks < openTicks, "Controller hung under the ceiling for as long as an open jump lasted: "
            + std::to_string(ceilingTicks) + " vs " + std::to_string(openTicks));

        // Under a rotated, non-uniformly scaled parent the write-back has to undo all of the
        // parent's transform, or the rider's world position would disagree with the controller's.
        auto rig = Pine::Entities::Create("Rig");
        rig->GetTransform()->SetLocalPosition(Pine::Vector3f(3.f, 0.f, -4.f));
        rig->GetTransform()->SetLocalRotation(glm::angleAxis(glm::radians(90.f), Pine::Vector3f(0.f, 1.f, 0.f)));
        rig->GetTransform()->SetLocalScale(Pine::Vector3f(2.f, 1.f, 0.5f));

        auto rider = rig->CreateChild();
        rider->GetTransform()->SetPosition(Pine::Vector3f(-4.f, 1.f, 3.f));
        auto riderController = rider->AddComponent<Pine::CharacterController>();

        const auto riderAt = [&](float x, float z, float tolerance)
        {
            const auto position = rider->GetTransform()->GetPosition();
            return std::abs(position.x - x) < tolerance && std::abs(position.z - z) < tolerance;
        };
        const auto describeRider = [&]()
        {
            const auto position = rider->GetTransform()->GetPosition();
            return std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z);
        };

        step(60);
        check(riderController->IsGrounded(), "Parented controller did not land");
        check(riderAt(-4.f, 3.f, 0.05f) && std::abs(rider->GetTransform()->GetPosition().y) < 0.2f,
            "Parented controller did not land below where it was placed: " + describeRider());

        for (int i = 0; i < 30; ++i)
        {
            riderController->Move(Pine::Vector3f(0.f, 0.f, 3.f) * (1.f / 60.f));
            step(1);
        }
        check(riderAt(-4.f, 4.5f, 0.2f), "Parented controller did not walk along world +Z: " + describeRider());

        riderController->SetPosition(Pine::Vector3f(-6.f, 0.2f, 0.f));
        step(1);
        check(riderAt(-6.f, 0.f, 0.05f), "Parented controller did not teleport to a world position: " + describeRider());

        PlayHandler::Stop();

        std::cout << "PASS: grounding, reported velocity on open ground and against a wall, side contacts,\n"
                     "      velocity cleared by teleport, jump apex from an assigned vertical velocity,\n"
                     "      the ceiling ending the climb, and a controller under a rotated, scaled parent.\n";
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }
