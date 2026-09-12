#include "CharacterController.hpp"

#include <cmath>
#include <vector>

#include "Pine/World/Entity/Entity.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "../../../Core/Serialization/Json/SerializationJson.hpp"

#include "physx/PxPhysicsAPI.h"
#include "physx/characterkinematic/PxController.h"
#include "physx/characterkinematic/PxCapsuleController.h"
#include "physx/characterkinematic/PxControllerManager.h"

Pine::CharacterController::CharacterController()
        : Component(ComponentType::CharacterController)
{
}

void Pine::CharacterController::Move(const Vector3f& motion)
{
    m_PendingMovement += motion;
}

bool Pine::CharacterController::IsGrounded() const
{
    return m_Grounded;
}

void Pine::CharacterController::CreateController()
{
    const auto transform = m_Parent->GetTransform();
    const auto position = transform->GetPosition();

    physx::PxCapsuleControllerDesc desc;
    desc.radius = m_Radius;
    desc.height = m_Height;
    desc.stepOffset = m_StepOffset;
    desc.contactOffset = m_ContactOffset;
    desc.slopeLimit = std::cos(glm::radians(m_SlopeLimit));
    desc.upDirection = physx::PxVec3(0.0f, 1.0f, 0.0f);
    desc.climbingMode = physx::PxCapsuleClimbingMode::eCONSTRAINED;
    desc.material = Physics3D::GetDefaultMaterial();

    // desc.position is the capsule CENTER; offset it up so the entity origin sits at the feet.
    const float centerOffset = m_Height * 0.5f + m_Radius;
    desc.position = physx::PxExtendedVec3(position.x, position.y + centerOffset, position.z);

    m_Controller = Physics3D::GetControllerManager()->createController(desc);

    if (m_Controller == nullptr)
    {
        PError("Failed to create PhysX character controller");
        return;
    }

    m_Controller->getActor()->userData = m_Parent;

    ApplyFilterData();
}

void Pine::CharacterController::ApplyFilterData() const
{
    if (m_Controller == nullptr)
        return;

    auto* actor = m_Controller->getActor();

    if (actor == nullptr)
        return;

    // The controller's kinematic actor is created with empty filter data, which would fail
    // PineFilterShader's mask test and pass through everything - set it so it collides normally.
    physx::PxFilterData filterData;
    filterData.word0 = m_Layer;
    filterData.word1 = m_LayerMask;

    const auto shapeCount = actor->getNbShapes();

    std::vector<physx::PxShape*> shapes(shapeCount);
    actor->getShapes(shapes.data(), shapeCount);

    for (auto* shape : shapes)
    {
        shape->setSimulationFilterData(filterData);
        shape->setQueryFilterData(filterData);
    }
}

void Pine::CharacterController::Simulate(const float elapsedTime)
{
    // A static entity is one whose transform never changes, which a character controller
    // contradicts by definition - say so rather than silently doing nothing.
    if (m_Parent->GetStatic())
    {
        if (!m_StaticWarningIssued)
        {
            PWarning(fmt::format("CharacterController on entity '{}' will not simulate because the entity is marked Static.", m_Parent->GetName()));

            m_StaticWarningIssued = true;
        }

        return;
    }

    if (m_Controller == nullptr)
        CreateController();

    if (m_Controller == nullptr)
        return;

    // Accumulate gravity into a vertical velocity while airborne.
    m_VerticalVelocity += m_Gravity * elapsedTime;

    const physx::PxVec3 displacement(
        m_PendingMovement.x,
        m_PendingMovement.y + m_VerticalVelocity * elapsedTime,
        m_PendingMovement.z
    );

    physx::PxFilterData controllerFilterData;
    controllerFilterData.word0 = m_Layer;
    controllerFilterData.word1 = m_LayerMask;

    physx::PxControllerFilters filters;
    filters.mFilterData = &controllerFilterData;

    const auto collisionFlags = m_Controller->move(displacement, 0.001f, elapsedTime, filters);

    m_Grounded = collisionFlags.isSet(physx::PxControllerCollisionFlag::eCOLLISION_DOWN);

    // Stop accumulating downward speed once resting on the ground.
    if (m_Grounded && m_VerticalVelocity < 0.0f)
        m_VerticalVelocity = 0.0f;

    // The queued movement has now been consumed.
    m_PendingMovement = Vector3f(0.0f);

    WriteBackTransform();
}

void Pine::CharacterController::WriteBackTransform() const
{
    const auto foot = m_Controller->getFootPosition();

    SetTransformFromWorldPosition(Vector3f(
        static_cast<float>(foot.x),
        static_cast<float>(foot.y),
        static_cast<float>(foot.z)
    ));
}

void Pine::CharacterController::SetTransformFromWorldPosition(const Vector3f& worldPosition) const
{
    Vector3f localPosition = worldPosition;

    // Transform only exposes a local setter; convert world -> local by subtracting the parent's
    // world position (mirroring how Transform::GetPosition() sums parent positions).
    if (const auto parentEntity = m_Parent->GetParent())
    {
        localPosition = localPosition - parentEntity->GetTransform()->GetPosition();
    }

    m_Parent->GetTransform()->SetLocalPosition(localPosition);
}

void Pine::CharacterController::SetPosition(const Vector3f& position)
{
    // A teleport is not movement: drop anything queued and stop the fall, so the controller doesn't
    // arrive carrying the speed it built up somewhere else.
    m_PendingMovement = Vector3f(0.0f);
    m_VerticalVelocity = 0.0f;

    // Before the first physics tick there is no PhysX controller yet, and CreateController() reads
    // the transform - so writing the transform is enough, the controller starts in the right place.
    if (m_Controller == nullptr)
    {
        SetTransformFromWorldPosition(position);
        return;
    }

    m_Controller->setFootPosition(physx::PxExtendedVec3(position.x, position.y, position.z));

    WriteBackTransform();
}

void Pine::CharacterController::SetRadius(const float radius)
{
    m_Radius = radius;

    if (m_Controller)
        static_cast<physx::PxCapsuleController*>(m_Controller)->setRadius(radius);
}

float Pine::CharacterController::GetRadius() const
{
    return m_Radius;
}

void Pine::CharacterController::SetHeight(const float height)
{
    m_Height = height;

    if (m_Controller)
        static_cast<physx::PxCapsuleController*>(m_Controller)->setHeight(height);
}

float Pine::CharacterController::GetHeight() const
{
    return m_Height;
}

void Pine::CharacterController::SetSlopeLimit(const float degrees)
{
    m_SlopeLimit = degrees;

    // PhysX stores the slope limit as a cosine, same conversion as CreateController().
    if (m_Controller)
        m_Controller->setSlopeLimit(std::cos(glm::radians(degrees)));
}

float Pine::CharacterController::GetSlopeLimit() const
{
    return m_SlopeLimit;
}

void Pine::CharacterController::SetStepOffset(const float stepOffset)
{
    m_StepOffset = stepOffset;

    if (m_Controller)
        m_Controller->setStepOffset(stepOffset);
}

float Pine::CharacterController::GetStepOffset() const
{
    return m_StepOffset;
}

void Pine::CharacterController::SetContactOffset(const float contactOffset)
{
    m_ContactOffset = contactOffset;

    if (m_Controller)
        m_Controller->setContactOffset(contactOffset);
}

float Pine::CharacterController::GetContactOffset() const
{
    return m_ContactOffset;
}

void Pine::CharacterController::SetGravity(const float gravity)
{
    m_Gravity = gravity;
}

float Pine::CharacterController::GetGravity() const
{
    return m_Gravity;
}

void Pine::CharacterController::SetLayer(const std::uint32_t layer)
{
    m_Layer = layer;

    ApplyFilterData();
}

std::uint32_t Pine::CharacterController::GetLayer() const
{
    return m_Layer;
}

void Pine::CharacterController::SetLayerMask(const std::uint32_t layerMask)
{
    m_LayerMask = layerMask;

    ApplyFilterData();
}

std::uint32_t Pine::CharacterController::GetLayerMask() const
{
    return m_LayerMask;
}

void Pine::CharacterController::OnCopied()
{
    Component::OnCopied();

    // The copy must create and own its own PhysX controller.
    m_Controller = nullptr;
    m_VerticalVelocity = 0.0f;
    m_Grounded = false;
    m_PendingMovement = Vector3f(0.0f);
    m_StaticWarningIssued = false;
}

void Pine::CharacterController::OnDestroyed()
{
    Component::OnDestroyed();

    if (m_Controller)
    {
        m_Controller->release();
        m_Controller = nullptr;
    }
}

void Pine::CharacterController::LoadData(const ByteSpan& span)
{
    CharacterControllerSerializer serializer;

    serializer.Read(span);

    serializer.Radius.Read(m_Radius);
    serializer.Height.Read(m_Height);
    serializer.SlopeLimit.Read(m_SlopeLimit);
    serializer.StepOffset.Read(m_StepOffset);
    serializer.ContactOffset.Read(m_ContactOffset);
    serializer.Gravity.Read(m_Gravity);
    serializer.Layer.Read(m_Layer);
    serializer.LayerMask.Read(m_LayerMask);
}

Pine::ByteSpan Pine::CharacterController::SaveData()
{
    CharacterControllerSerializer serializer;

    serializer.Radius.Write(m_Radius);
    serializer.Height.Write(m_Height);
    serializer.SlopeLimit.Write(m_SlopeLimit);
    serializer.StepOffset.Write(m_StepOffset);
    serializer.ContactOffset.Write(m_ContactOffset);
    serializer.Gravity.Write(m_Gravity);
    serializer.Layer.Write(m_Layer);
    serializer.LayerMask.Write(m_LayerMask);

    return serializer.Write();
}
