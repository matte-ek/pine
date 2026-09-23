    try
    {
        using namespace physx;

        const auto check = [](bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        };
        const auto close = [](float value, float expected, float tolerance)
        {
            return std::abs(value - expected) <= tolerance;
        };

        auto scene = Pine::Physics3D::GetScene();

        const auto step = [&](int count)
        {
            for (int i = 0; i < count; ++i) Pine::Physics3D::Update(1.0 / 60.0);
        };

        // Every part below builds its terrain at its own position, and a leftover terrain from an
        // earlier part would sit under the next one's raycasts and answer them first. Run between
        // parts, never during play: PlayHandler::Stop reloads the level from its snapshot, so every
        // entity pointer taken before Play is stale by the time this is called.
        const auto resetScene = [&]()
        {
            Pine::Entities::DeleteAll();

            step(1);

            const auto flags = PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC;

            check(scene->getNbActors(flags) == 0,
                "A cleared scene still holds " + std::to_string(scene->getNbActors(flags)) + " physics actors");
        };

        // The terrain's collision is a static actor, so the only way to ask "where is the ground
        // here" is to cast at it. Straight down from above the terrain's ceiling, which is where a
        // falling body comes from anyway.
        const auto groundUnder = [&](float x, float z, float from) -> std::optional<float>
        {
            PxRaycastBuffer hit;

            if (!scene->raycast(PxVec3(x, from, z), PxVec3(0.f, -1.f, 0.f), from + 10000.f, hit))
            {
                return std::nullopt;
            }

            return hit.block.position.y;
        };

        // The scene is built here rather than over /edit, which has no TerrainRenderer operation.
        const auto buildTerrain = [&](const char* name, Pine::Vector3f entityPosition)
        {
            auto terrain = dynamic_cast<Pine::Terrain*>(
                Pine::Assets::CreateAsset(Pine::AssetType::Terrain,
                    std::string("projects/terrain/assets/") + name));

            Pine::Assets::Internal::RegisterAsset(terrain);

            auto entity = Pine::Entities::Create(name);

            entity->GetTransform()->SetLocalPosition(entityPosition);
            entity->AddComponent<Pine::TerrainRendererComponent>()->SetTerrain(terrain);
            entity->AddComponent<Pine::Collider>()->SetColliderType(Pine::ColliderType::HeightField);

            return std::make_pair(terrain, entity);
        };

        // Part 1 - a terrain with real relief at the world origin. Every later part changes exactly
        // one thing about this setup, so a failure there points at that one thing.
        auto [ground, groundEntity] = buildTerrain("ground", Pine::Vector3f(0.f));

        ground->SetChunkQuads(32);
        ground->SetChunkSize(32.f);
        ground->Resize({ 0, 0 }, { 3, 3 });
        ground->GenerateFromNoise();

        PlayHandler::Play();
        step(2);

        // One shape for the whole terrain, not one per chunk. That is what keeps Collider and
        // RigidBody from ever having to know about multi-shape actors.
        PxRigidActor* groundActor = nullptr;
        {
            const auto flags = PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC;
            std::vector<PxActor*> actors(scene->getNbActors(flags));

            scene->getActors(flags, actors.data(), actors.size());

            for (auto candidate : actors)
            {
                if (candidate->userData == groundEntity) groundActor = candidate->is<PxRigidActor>();
            }
        }

        check(groundActor != nullptr, "The terrain collider produced no actor");
        check(groundActor->getNbShapes() == 1,
            "A terrain should attach as one shape, got " + std::to_string(groundActor->getNbShapes()));

        {
            PxShape* shape = nullptr;

            groundActor->getShapes(&shape, 1);

            check(shape->getGeometry().getType() == PxGeometryType::eHEIGHTFIELD,
                "The terrain collider's shape is not a height field");
        }

        // The assertion the whole unit exists for: what PhysX thinks the ground is, everywhere,
        // equals what the asset says it is. Anything wrong with the row or column scale, the height
        // scale, the local pose or the sample transposition moves these apart - and a scale error
        // that happens to vanish at the origin does not survive a sweep across the whole terrain.
        const float span = ground->GetChunkSize() * ground->GetChunkCount().x;
        const float ceiling = ground->GetHeightMax() + 10.f;

        const auto sweep = [&](const char* what, Pine::Terrain* terrain, Pine::Vector3f origin, int steps)
        {
            const float terrainSpan = terrain->GetChunkSize() * terrain->GetChunkCount().x;
            const float terrainCeiling = origin.y + terrain->GetHeightMax() + 10.f;
            const auto sampleMin = terrain->GetSampleMin();

            const float localBase = static_cast<float>(sampleMin.x) * terrain->GetSampleSpacing();
            const float localBaseZ = static_cast<float>(sampleMin.y) * terrain->GetSampleSpacing();

            for (int i = 1; i < steps; i++)
            {
                for (int j = 1; j < steps; j++)
                {
                    const float localX = localBase + terrainSpan * i / steps;
                    const float localZ = localBaseZ + terrainSpan * j / steps;

                    const auto expected = terrain->GetHeightAt(localX, localZ);

                    check(expected.has_value(), std::string(what) + ": sampled a point off the terrain");

                    const auto actual = groundUnder(origin.x + localX, origin.z + localZ, terrainCeiling);

                    check(actual.has_value(), std::string(what) + ": nothing to stand on at "
                        + std::to_string(localX) + ", " + std::to_string(localZ));

                    // Generous next to the terrain's own ~0.002 sample resolution, tight next to
                    // the metres a transposed axis or a doubled scale would be out by.
                    check(close(*actual, origin.y + *expected, 0.05f),
                        std::string(what) + ": collision sits at " + std::to_string(*actual)
                            + " where the terrain says " + std::to_string(origin.y + *expected)
                            + " (at " + std::to_string(localX) + ", " + std::to_string(localZ) + ")");
                }
            }
        };

        sweep("relief at the origin", ground, Pine::Vector3f(0.f), 17);

        // A body dropped from above lands on that surface and stays there.
        {
            auto falling = Pine::Entities::Create("Falling");

            falling->GetTransform()->SetLocalPosition(Pine::Vector3f(span * 0.5f, ceiling, span * 0.5f));
            falling->AddComponent<Pine::Collider>()->SetSize(Pine::Vector3f(0.5f));
            falling->AddComponent<Pine::RigidBody>();

            step(600);

            const auto resting = falling->GetTransform()->GetPosition();
            const auto body = falling->GetComponent<Pine::RigidBody>()->GetRigidBody();

            check(resting.y < ceiling - 1.f, "A dropped body never fell");

            check(body->getLinearVelocity().magnitude() < 0.1f,
                "A dropped body was still moving after ten seconds at "
                    + std::to_string(body->getLinearVelocity().magnitude()) + " units/s");

            // Compared against the ground where it *ended up*, not where it was released: a body
            // dropped onto a slope slides, and measuring against the release point would only be
            // asserting that this particular spot happens to be flat.
            const auto surface = ground->GetHeightAt(resting.x, resting.z);

            check(surface.has_value(), "A dropped body came to rest off the terrain");

            // A half-unit box resting on ground cannot have its centre below that ground, and
            // cannot be further above it than its own half-diagonal - which is the most a box
            // balanced on one corner of a slope can manage.
            check(resting.y > *surface,
                "A dropped body sank into the terrain: centre at " + std::to_string(resting.y)
                    + ", ground at " + std::to_string(*surface));

            check(resting.y - *surface < 0.87f,
                "A dropped body came to rest " + std::to_string(resting.y - *surface)
                    + " above the ground, which is too high to be touching it");
        }

        PlayHandler::Stop();
        resetScene();

        // Part 2 - the same thing, moved. The entity's position and a negative chunk origin are the
        // two offsets the shape's local pose has to carry, and each of them is invisible in part 1
        // because both are zero there.
        {
            auto [offset, offsetEntity] = buildTerrain("offset", Pine::Vector3f(120.f, 25.f, -60.f));

            offset->SetChunkQuads(16);
            offset->SetChunkSize(32.f);
            offset->Resize({ -2, -2 }, { 3, 3 });
            offset->GenerateFromNoise();

            PlayHandler::Play();
            step(2);

            sweep("moved terrain with a negative chunk origin", offset, Pine::Vector3f(120.f, 25.f, -60.f), 13);

            // Terrain is drawn by position alone - a height field cannot represent a rotated
            // surface - so collision has to ignore rotation too, or the ground you walk on stops
            // being the ground you see.
            offsetEntity->GetTransform()->SetEulerAngles(Pine::Vector3f(0.f, 40.f, 0.f));
            offsetEntity->GetComponent<Pine::Collider>()->Reset();

            step(2);

            sweep("rotated terrain", offset, Pine::Vector3f(120.f, 25.f, -60.f), 13);

            PlayHandler::Stop();
            resetScene();
        }

        // Part 3 - the quad diagonal. A quad with three corners at zero and one raised is flat
        // across one of its triangles and sloped across the other, so casting into each of them
        // separates the two possible diagonals by the whole raised height. Nothing about a smooth
        // noise terrain can tell them apart, which is exactly how the old implementation shipped
        // three expressions of this rule that disagreed.
        {
            auto [wedge, wedgeEntity] = buildTerrain("wedge", Pine::Vector3f(0.f));

            wedge->SetChunkQuads(8);
            wedge->SetChunkSize(8.f);
            wedge->Resize({ 0, 0 }, { 1, 1 });

            wedge->SetSampleHeight({ 4, 4 }, 0.f);
            wedge->SetSampleHeight({ 5, 4 }, 0.f);
            wedge->SetSampleHeight({ 4, 5 }, 0.f);
            wedge->SetSampleHeight({ 5, 5 }, 40.f);

            PlayHandler::Play();
            step(2);

            const float spacing = wedge->GetSampleSpacing();

            // The quad with its low corner at sample (4, 4) is split from (4, 5) to (5, 4). The
            // triangle on the (4, 4) side of that diagonal is flat at zero; the other rises to the
            // far corner.
            const float flatX = (4.f + 0.25f) * spacing;
            const float flatZ = (4.f + 0.25f) * spacing;
            const float slopedX = (4.f + 0.75f) * spacing;
            const float slopedZ = (4.f + 0.75f) * spacing;

            const auto flat = groundUnder(flatX, flatZ, 80.f);
            const auto sloped = groundUnder(slopedX, slopedZ, 80.f);

            check(flat.has_value() && sloped.has_value(), "The wedge quad has no collision over it");

            check(close(*flat, 0.f, 0.05f),
                "The flat triangle of the quad collides at " + std::to_string(*flat)
                    + ", not 0 - PhysX split the quad along the other diagonal");

            check(close(*sloped, 20.f, 0.5f),
                "The sloped triangle of the quad collides at " + std::to_string(*sloped) + ", not 20");

            // And both agree with the query the renderer and the sculpting brush use.
            check(close(*flat, *wedge->GetHeightAt(flatX, flatZ), 0.05f) &&
                  close(*sloped, *wedge->GetHeightAt(slopedX, slopedZ), 0.05f),
                "Collision and GetHeightAt disagree about which triangle a point falls in");

            PlayHandler::Stop();
            resetScene();
        }

        // Part 4 - a height field collider with no terrain to take geometry from produces no shape
        // and no actor, rather than an actor with nothing attached to it.
        {
            auto orphan = Pine::Entities::Create("Orphan");

            orphan->AddComponent<Pine::Collider>()->SetColliderType(Pine::ColliderType::HeightField);

            PlayHandler::Play();
            step(2);

            const auto flags = PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC;
            std::vector<PxActor*> actors(scene->getNbActors(flags));

            scene->getActors(flags, actors.data(), actors.size());

            for (auto candidate : actors)
            {
                check(candidate->userData != orphan, "A height field collider with no terrain made an actor");
            }

            PlayHandler::Stop();
            resetScene();
        }

        // Part 5 - edits reach the collision. One terrain under two entities: a bare collider, whose
        // shape sits on its own actor, and one with a static RigidBody, whose shape sits on the
        // body's actor instead - the case Collider::Reset has to hand over to RigidBody::Reset.
        {
            auto [edited, editedEntity] = buildTerrain("edited", Pine::Vector3f(0.f));

            edited->SetChunkQuads(8);
            edited->SetChunkSize(8.f);
            edited->Resize({ 0, 0 }, { 2, 2 });

            const Pine::Vector3f bodyOrigin(100.f, 0.f, 0.f);

            auto bodyEntity = Pine::Entities::Create("edited-body");

            bodyEntity->GetTransform()->SetLocalPosition(bodyOrigin);
            bodyEntity->AddComponent<Pine::TerrainRendererComponent>()->SetTerrain(edited);
            bodyEntity->AddComponent<Pine::Collider>()->SetColliderType(Pine::ColliderType::HeightField);
            bodyEntity->AddComponent<Pine::RigidBody>()->SetRigidBodyType(Pine::RigidBodyType::Static);

            PlayHandler::Play();
            step(2);

            // Both copies of the ground, checked in one call so neither can be forgotten.
            const auto sweepBoth = [&](const std::string& what)
            {
                sweep((what + ", bare collider").c_str(), edited, Pine::Vector3f(0.f), 9);
                sweep((what + ", with a RigidBody").c_str(), edited, bodyOrigin, 9);
            };

            sweepBoth("flat terrain before any edit");

            const float centre = 8.f;
            const float ceilingY = edited->GetHeightMax() + 10.f;
            const auto flatHeight = *edited->GetHeightAt(centre, centre);

            for (int z = 6; z <= 10; z++)
            {
                for (int x = 6; x <= 10; x++)
                {
                    check(edited->SetSampleHeight({ x, z }, flatHeight + 10.f), "The raised block fell outside the field");
                }
            }

            // Cooked once, not per frame: until something asks for a rebuild, the collision keeps
            // the ground it was cooked from. This is also what shows the checks below can fail.
            step(2);

            const auto stale = groundUnder(centre, centre, ceilingY);

            check(stale.has_value() && close(*stale, flatHeight, 0.05f),
                "The collision followed a height edit nobody asked it to rebuild for");

            Pine::Physics3D::TerrainCollision::RebuildColliders(*edited);
            step(2);

            sweepBoth("after RebuildColliders");

            {
                auto body = bodyEntity->GetComponent<Pine::RigidBody>();

                check(body->GetRigidBody() == nullptr, "The rebuilt static body came back as a dynamic actor");
            }

            // A sculpt stroke rebuilds once, when it ends. The editor only sculpts while stopped,
            // when no actors exist to go stale, so this drives TerrainSculpting directly in play
            // mode to hold the brush to the same rule as every other height edit.
            Editor::TerrainSculpting::Brush brush;

            brush.Mode = Editor::TerrainSculpting::BrushMode::Raise;
            brush.Radius = 3.f;
            brush.Strength = 20.f;

            const Pine::Vector2f strokePoint(4.f, 4.f);
            const auto beforeStroke = *edited->GetHeightAt(strokePoint.x, strokePoint.y);

            check(Editor::TerrainSculpting::Apply(edited, brush, strokePoint, 0.5f), "The sculpt stroke changed nothing");
            step(2);

            const auto midStroke = groundUnder(strokePoint.x, strokePoint.y, ceilingY);

            check(midStroke.has_value() && close(*midStroke, beforeStroke, 0.05f),
                "The collision was rebuilt in the middle of a stroke rather than at its end");

            Editor::TerrainSculpting::EndStroke();
            step(2);

            check(*edited->GetHeightAt(strokePoint.x, strokePoint.y) > beforeStroke + 1.f, "The stroke did not raise the ground");

            sweepBoth("after a sculpt stroke");

            PlayHandler::Stop();
        }

        std::cout << "PASS: one height field shape per terrain, collision matching GetHeightAt across a\n"
                     "      terrain with relief, under an entity offset, a negative chunk origin and a\n"
                     "      rotated entity, the quad diagonal, a settling dropped body, a collider\n"
                     "      with no terrain, and collision rebuilt after RebuildColliders and at the\n"
                     "      end of a sculpt stroke, with and without a RigidBody.\n";
    }
    catch (const std::exception& exception)
    {
        std::cerr << "FAIL: " << exception.what() << std::endl;
        std::_Exit(1);
    }
