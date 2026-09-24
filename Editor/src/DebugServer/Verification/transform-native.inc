    try
    {
        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };
        const auto describe = [](const Pine::Vector3f& vector)
        {
            return std::to_string(vector.x) + ", " + std::to_string(vector.y) + ", " + std::to_string(vector.z);
        };
        const auto near = [](const Pine::Vector3f& a, const Pine::Vector3f& b)
        {
            return glm::length(a - b) < 1e-4f;
        };
        const auto sameRotation = [](const Pine::Quaternion& a, const Pine::Quaternion& b)
        {
            return std::abs(std::abs(glm::dot(a, b)) - 1.f) < 1e-5f;
        };
        const auto quarterTurn = [](const Pine::Vector3f& axis)
        {
            return glm::angleAxis(glm::radians(90.f), axis);
        };
        const auto position = [](const Pine::Entity* entity)
        {
            return entity->GetTransform()->GetPosition();
        };
        const auto matrixPosition = [](const Pine::Entity* entity)
        {
            return Pine::Vector3f(entity->GetTransform()->GetTransformationMatrix()[3]);
        };
        const auto version = [](const Pine::Entity* entity)
        {
            return entity->GetTransform()->GetWorldVersion();
        };

        // Under a uniformly scaled parent, the composition has to agree with multiplying the
        // parent's matrix by the child's local one, as a conventional scene graph does.
        const auto composesLikeMatrices = [](const Pine::Entity* child)
        {
            const auto transform = child->GetTransform();

            auto local = glm::translate(Pine::Matrix4f(1.f), transform->GetLocalPosition());
            local *= glm::toMat4(transform->GetLocalRotation());
            local = glm::scale(local, transform->GetLocalScale());

            const auto expected = child->GetParent()->GetTransform()->GetTransformationMatrix() * local;
            const auto& actual = transform->GetTransformationMatrix();

            for (int column = 0; column < 4; column++)
            {
                if (glm::length(expected[column] - actual[column]) > 1e-4f) return false;
            }

            return true;
        };

        // A group turned a quarter about +Y (+X becomes -Z, +Z becomes +X) and doubled in size.
        auto group = Pine::Entities::Create("Transform probe group");
        auto child = group->CreateChild();
        auto grandchild = child->CreateChild();

        group->GetTransform()->SetLocalPosition({10, 0, 0});
        group->GetTransform()->SetLocalRotation(quarterTurn({0, 1, 0}));
        group->GetTransform()->SetLocalScale({2, 2, 2});
        child->GetTransform()->SetLocalPosition({1, 0, 0});
        grandchild->GetTransform()->SetLocalPosition({0, 0, 3});
        grandchild->GetTransform()->SetLocalScale({1, 3, 1});

        check(near(position(child), {10, 0, -2}), "Child ignores its parent's rotation or scale: " + describe(position(child)));
        check(near(position(grandchild), {16, 0, -2}), "Grandchild is misplaced: " + describe(position(grandchild)));
        check(sameRotation(grandchild->GetTransform()->GetRotation(), quarterTurn({0, 1, 0})), "Rotations did not compose");
        check(near(grandchild->GetTransform()->GetScale(), {2, 6, 2}), "Scales did not multiply");
        check(near(matrixPosition(grandchild), position(grandchild)), "Matrix disagrees with the world position");
        check(composesLikeMatrices(child) && composesLikeMatrices(grandchild), "Composition differs from matrix parenting");

        // Moving only the group has to reach the descendants' cached matrices, and advance the
        // world versions the renderer's caches compare against.
        const auto childVersion = version(child);
        const auto grandchildVersion = version(grandchild);
        group->GetTransform()->SetLocalPosition({0, 5, 0});
        check(near(matrixPosition(grandchild), {6, 5, -2}),
            "Grandchild's matrix is stale after its group moved: " + describe(matrixPosition(grandchild)));
        check(version(child) > childVersion && version(grandchild) > grandchildVersion,
            "Moving a parent did not advance its descendants' world versions");
        check(version(group) != version(child) && version(child) != version(grandchild) && version(group) != version(grandchild),
            "Two transforms share a world version");

        const auto settledVersion = version(grandchild);
        position(grandchild);
        check(version(grandchild) == settledVersion, "Reading a transform advanced its world version");

        // World setters under a rotated, non-uniformly scaled parent store the local value that
        // reads back as the requested world value, and leave the parent alone.
        group->GetTransform()->SetLocalScale({2, 3, 0.5f});
        const auto childBefore = position(child);
        const Pine::Vector3f targetPosition(-4, 7, 1);
        const auto targetRotation = glm::angleAxis(0.7f, glm::normalize(Pine::Vector3f(1, 2, 3)));
        grandchild->GetTransform()->SetPosition(targetPosition);
        grandchild->GetTransform()->SetRotation(targetRotation);
        grandchild->GetTransform()->SetScale({1, 1, 1});
        check(near(position(grandchild), targetPosition), "SetPosition did not reach its world target: " + describe(position(grandchild)));
        check(sameRotation(grandchild->GetTransform()->GetRotation(), targetRotation), "SetRotation did not reach its world target");
        check(near(grandchild->GetTransform()->GetScale(), {1, 1, 1}), "SetScale did not reach its world target");
        check(near(position(child), childBefore), "Setting a child's world transform moved its parent");

        // Reparenting keeps the local values, so the child is now placed by the new parent.
        auto carrier = Pine::Entities::Create("Transform probe carrier");
        carrier->GetTransform()->SetLocalPosition({0, 0, 20});
        carrier->GetTransform()->SetLocalRotation(quarterTurn({0, 0, 1}));
        const auto versionBeforeReparent = version(grandchild);
        group->RemoveChild(child);
        carrier->AddChild(child);
        check(version(grandchild) > versionBeforeReparent, "Reparenting did not advance a descendant's world version");
        check(child->GetTransform()->GetLocalPosition() == Pine::Vector3f(1, 0, 0), "Reparenting changed the local position");
        check(near(position(child), {0, 1, 20}), "Reparented child is not placed by its new parent: " + describe(position(child)));
        check(composesLikeMatrices(grandchild), "Grandchild did not follow its reparented parent");

        // A spawned copy is built detached and attached piece by piece; it has to come out where
        // the original is.
        Pine::Blueprint blueprint;
        blueprint.CreateFromEntity(carrier);
        auto spawned = blueprint.Spawn();
        blueprint.Dispose();
        const auto spawnedGrandchild = spawned->GetChildren().at(0)->GetChildren().at(0);
        check(near(position(spawnedGrandchild), position(grandchild)),
            "Spawned grandchild is misplaced: " + describe(position(spawnedGrandchild)) + " vs " + describe(position(grandchild)));

        const auto countBefore = Pine::Entities::GetList().size();
        Pine::Entities::Delete(spawned);
        Pine::Entities::Delete(carrier);
        Pine::Entities::Delete(group);
        check(Pine::Entities::GetList().size() == countBefore - 7, "Deleting the hierarchies left entities behind");

        std::cout << "PASS: rotation and scale carried to children, matrix-parenting agreement, cached matrices\n"
                     "      and world versions following a moved parent, world setters, reparenting and spawning.\n";
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }
