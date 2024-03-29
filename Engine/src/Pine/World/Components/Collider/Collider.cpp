#include "Collider.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/World/Components/RigidBody/RigidBody.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Serialization.hpp"
#include <atomic>
#include <reactphysics3d/collision/TriangleVertexArray.h>
#include <reactphysics3d/mathematics/Vector3.h>

Pine::Collider::Collider::Collider()
        : IComponent(ComponentType::Collider)
{
}

void Pine::Collider::UpdateBody()
{
    const bool hasRigidBody = m_Parent->GetComponent<RigidBody>() != nullptr;

    if (hasRigidBody)
    {
        if (m_CollisionBody)
        {
            Pine::Physics3D::GetWorld()->destroyCollisionBody(m_CollisionBody);

            m_Collider = nullptr;
            m_CollisionBody = nullptr;
        }
    } else
    {
        m_ShapeUpdated = false;

        if (!m_CollisionBody)
        {
            m_CollisionBody = Pine::Physics3D::GetWorld()->createCollisionBody(m_CollisionBodyTransform);

            const auto transform = GetParent()->GetTransform();
            const auto position = transform->GetPosition();
            const auto rotation = transform->GetRotation();

            m_CollisionBodyTransform.setPosition(reactphysics3d::Vector3(position.x, position.y, position.z));
            m_CollisionBodyTransform.setOrientation(reactphysics3d::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w));

            m_CollisionBody->setTransform(m_CollisionBodyTransform);
        }

        if (!m_Collider && m_CollisionShape)
        {
            m_CollisionTransform.setToIdentity();

            m_Collider = m_CollisionBody->addCollider(m_CollisionShape, m_CollisionTransform);
        }
    }
}

reactphysics3d::TriangleMesh* Pine::Collider::LoadTriangleMesh()
{
    auto modelRenderer = m_Parent->GetComponent<Pine::ModelRenderer>();

    if (!modelRenderer || !modelRenderer->GetModel())
        return nullptr;

    auto model = modelRenderer->GetModel();

    // We'll currently have to reload the model again, in order to process the vertex and index data
    if (!model->LoadModel())
        return nullptr;
    if (model->m_MeshLoadData.empty())
        return nullptr;

    reactphysics3d::TriangleMesh *triangleMesh = Physics3D::GetCommon()->createTriangleMesh(); 

    for (const auto mesh : model->m_MeshLoadData)
    {        
        auto triangleArray = new reactphysics3d::TriangleVertexArray(
            mesh.VertexCount, 
            mesh.Vertices, sizeof(Vector3f), 
            mesh.Normals, sizeof(Vector3f),
            mesh.Faces,
            mesh.Indices, 3 * sizeof(std::uint32_t),
            reactphysics3d::TriangleVertexArray::VertexDataType::VERTEX_FLOAT_TYPE, 
            reactphysics3d::TriangleVertexArray::NormalDataType::NORMAL_FLOAT_TYPE,
            reactphysics3d::TriangleVertexArray::IndexDataType::INDEX_INTEGER_TYPE
        );

        triangleMesh->addSubpart(triangleArray);
    }

    // We'll have to purposely "memory-leak" a bit, since TriangleVertexArray doesn't copy any data.
    // TODO: I really *need* to get in some proper tracking for this
    for (const auto loadData : model->m_MeshLoadData)
    {
        //free(loadData.Normals);
        free(loadData.Tangents);
        free(loadData.UVs);
    }

    model->m_MeshLoadData.clear();

    return triangleMesh;
}

void Pine::Collider::CreateShape()
{
    switch (m_ColliderType)
    {
        case ColliderType::Box:
            m_CollisionShape = Pine::Physics3D::GetCommon()->createBoxShape(reactphysics3d::Vector3(m_Size.x, m_Size.y, m_Size.z));
            break;
        case ColliderType::Sphere:
            m_CollisionShape = Pine::Physics3D::GetCommon()->createSphereShape(m_Size.x);
            break;
        case ColliderType::Capsule:
            m_CollisionShape = Pine::Physics3D::GetCommon()->createCapsuleShape(m_Size.x, m_Size.y);
            break;
        default:
            break;
    }

    if (m_ColliderType == ColliderType::ConcaveMesh)
    {
        auto mesh = LoadTriangleMesh();
        
        if (mesh)
        {
            const auto& scale = m_Parent->GetTransform()->GetScale();

            m_CollisionShape = Pine::Physics3D::GetCommon()->createConcaveMeshShape(mesh, reactphysics3d::Vector3(32.f, 32.f, 32.f));
        }
    }

    m_ShapeUpdated = true;
}

void Pine::Collider::DisposeShape()
{
    if (!m_CollisionShape)
        return;

    switch (m_ColliderType)
    {
        case ColliderType::Box:
            Pine::Physics3D::GetCommon()->destroyBoxShape(dynamic_cast<reactphysics3d::BoxShape *>(m_CollisionShape));
            break;
        case ColliderType::Sphere:
            Pine::Physics3D::GetCommon()->destroySphereShape(dynamic_cast<reactphysics3d::SphereShape *>(m_CollisionShape));
            break;
        case ColliderType::Capsule:
            Pine::Physics3D::GetCommon()->destroyCapsuleShape(dynamic_cast<reactphysics3d::CapsuleShape *>(m_CollisionShape));
            break;
        case ColliderType::ConcaveMesh:
            Pine::Physics3D::GetCommon()->destroyConcaveMeshShape(dynamic_cast<reactphysics3d::ConcaveMeshShape *>(m_CollisionShape));
            break;
        case ColliderType::ConvexMesh:
            Pine::Physics3D::GetCommon()->destroyConvexMeshShape(dynamic_cast<reactphysics3d::ConvexMeshShape *>(m_CollisionShape));
            break;
        case ColliderType::HeightField:
            Pine::Physics3D::GetCommon()->destroyHeightFieldShape(dynamic_cast<reactphysics3d::HeightFieldShape *>(m_CollisionShape));
            break;
        default:
            break;
    }

    m_CollisionShape = nullptr;
    m_Collider = nullptr;
    m_ShapeUpdated = true;
}

void Pine::Collider::UpdateShape()
{
    auto size = m_Size;

    size *= m_Parent->GetTransform()->GetScale();

    if (!m_CollisionShape)
        CreateShape();

    switch (m_ColliderType)
    {
        case ColliderType::Box:
            dynamic_cast< reactphysics3d::BoxShape * >(m_CollisionShape)->setHalfExtents(reactphysics3d::Vector3(size.x, size.y, size.z));
            break;
        case ColliderType::Sphere:
            dynamic_cast< reactphysics3d::SphereShape * >(m_CollisionShape)->setRadius(size.x);
            break;
        case ColliderType::Capsule:
            dynamic_cast< reactphysics3d::CapsuleShape * >(m_CollisionShape)->setRadius(size.x);
            dynamic_cast< reactphysics3d::CapsuleShape * >(m_CollisionShape)->setHeight(size.y);
            break;
        case ColliderType::ConcaveMesh:
            //dynamic_cast< reactphysics3d::ConcaveMeshShape * >(m_CollisionShape)->setScale(reactphysics3d::Vector3(size.x, size.y, size.z));
            break;
        case ColliderType::ConvexMesh:
        case ColliderType::HeightField:
            break;
        default:
            break;
    }
}

void Pine::Collider::SetColliderType(Pine::ColliderType type)
{
    if (m_CollisionShape)
    {
        DisposeShape();
    }

    m_ColliderType = type;

    if (!m_Standalone)
    {
        CreateShape();
    }
}

Pine::ColliderType Pine::Collider::GetColliderType() const
{
    return m_ColliderType;
}

void Pine::Collider::SetPosition(Pine::Vector3f position)
{
    m_Position = position;
}

const Pine::Vector3f &Pine::Collider::GetPosition() const
{
    return m_Position;
}

void Pine::Collider::SetSize(Pine::Vector3f size)
{
    m_Size = size;
}

const Pine::Vector3f &Pine::Collider::GetSize() const
{
    return m_Size;
}

void Pine::Collider::SetRadius(float radius)
{
    m_Size.x = radius;
}

float Pine::Collider::GetRadius() const
{
    return m_Size.x;
}

void Pine::Collider::SetHeight(float height)
{
    m_Size.y = height;
}

float Pine::Collider::GetHeight() const
{
    return m_Size.y;
}

bool Pine::Collider::PollShapeUpdated()
{
    const bool ret = m_ShapeUpdated;

    m_ShapeUpdated = false;

    return ret;
}

reactphysics3d::CollisionShape *Pine::Collider::GetCollisionShape() const
{
    return m_CollisionShape;
}

reactphysics3d::Collider *Pine::Collider::GetCollider() const
{
    return m_Collider;
}

void Pine::Collider::OnPrePhysicsUpdate()
{
    if (m_Standalone) return;

    UpdateShape();
    UpdateBody();

    if (!m_CollisionShape)
    {
        Log::Warning("Pine::Collider::OnPrePhysicsUpdate(): No collision shape?");
        return;
    }

    if (m_CollisionBody && !m_Parent->GetStatic())
    {
        const auto transform = GetParent()->GetTransform();
        const auto position = transform->GetPosition();
        const auto rotation = transform->GetRotation();

        m_CollisionBodyTransform.setPosition(reactphysics3d::Vector3(position.x, position.y, position.z));
        m_CollisionBodyTransform.setOrientation(reactphysics3d::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w));

        m_CollisionBody->setTransform(m_CollisionBodyTransform);
    }
}

void Pine::Collider::OnDestroyed()
{
    const auto rigidBody = m_Parent->GetComponent<Pine::RigidBody>();

    if (rigidBody)
    {
        if (rigidBody->IsColliderAttached(this))
        {
            rigidBody->DetachCollider();
        }
    }

    if (m_CollisionBody)
    {
        Physics3D::GetWorld()->destroyCollisionBody(m_CollisionBody);

        m_CollisionBody = nullptr;
    }

    DisposeShape();
}

void Pine::Collider::OnCopied()
{
    m_CollisionBody = nullptr;
    m_CollisionShape = nullptr;
}

void Pine::Collider::LoadData(const nlohmann::json &j)
{
    Pine::Serialization::LoadVector3(j, "pos", m_Position);
    Pine::Serialization::LoadVector3(j, "size", m_Size);
    Pine::Serialization::LoadValue(j, "ctype", m_ColliderType);
}

void Pine::Collider::SaveData(nlohmann::json &j)
{
    j["pos"] = Pine::Serialization::StoreVector3(m_Position);
    j["size"] = Pine::Serialization::StoreVector3(m_Size);
    j["ctype"] = m_ColliderType;
}