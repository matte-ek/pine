#include "RigidBody2D.hpp"

#include "../../../Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Physics/Physics2D/Physics2D.hpp"
#include "Pine/World/Components/Collider2D/Collider2D.hpp"
#include "Pine/World/Entity/Entity.hpp"

void Pine::RigidBody2D::UpdateBody()
{
	if (!GetParent()->HasComponent<Collider2D>())
	{
		return;
	}

	auto collider = GetParent()->GetComponent<Collider2D>();

	if (m_Body)
	{
	    if (m_BodySize != collider->ComputeSize() || 
			m_BodyType != m_RigidBodyType)
	    {
			m_Body = nullptr;
			m_Fixture = nullptr;
		}
	}

	if (!m_Body)
	{
		const auto position = collider->ComputePosition();
		const auto size = collider->ComputeSize();

	}

	const auto newPosition = collider->ComputePosition();
}

Pine::RigidBody2D::RigidBody2D()
	: Component(ComponentType::RigidBody2D)
{
}

void Pine::RigidBody2D::SetRigidBodyType(const RigidBody2DType type)
{
	m_RigidBodyType = type;
}

Pine::RigidBody2DType Pine::RigidBody2D::GetRigidBodyType() const
{
	return m_RigidBodyType;
}

void Pine::RigidBody2D::OnPrePhysicsUpdate()
{
	if (m_Standalone)
	{
		return;
	}

	UpdateBody();
}

void Pine::RigidBody2D::OnPostPhysicsUpdate()
{
	if (!m_Body)
	{
		return;
	}

}

void Pine::RigidBody2D::OnDestroyed()
{
	Component::OnDestroyed();

	if (m_Body)
	{
		m_Body = nullptr;
		m_Fixture = nullptr;
	}
}

void Pine::RigidBody2D::OnCopied()
{
	Component::OnCopied();

	m_Body = nullptr;
	m_Fixture = nullptr;
}

void Pine::RigidBody2D::OnRender(const float deltaTime)
{
	Component::OnRender(deltaTime);
}

void Pine::RigidBody2D::LoadData(const ByteSpan& span)
{
    RigidBody2DSerializer serializer;

    serializer.Read(span);

    serializer.Type.Read(m_RigidBodyType);
    serializer.Size.Read(m_BodySize);
    serializer.BodyType.Read(m_BodyType);
}

Pine::ByteSpan Pine::RigidBody2D::SaveData()
{
    RigidBody2DSerializer serializer;

    serializer.Type.Write(m_RigidBodyType);
    serializer.Size.Write(m_BodySize);
    serializer.BodyType.Write(m_BodyType);

    return serializer.Write();
}