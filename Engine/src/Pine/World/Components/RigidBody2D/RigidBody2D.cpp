#include "RigidBody2D.hpp"

#include <box2d/b2_body.h>
#include <box2d/b2_polygon_shape.h>
#include <box2d/b2_world.h>

#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/Physics/Physics2D/Physics2D.hpp"
#include "Pine/World/Components/Collider2D/Collider2D.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
	b2BodyType GetBodyType(Pine::RigidBody2DType type)
	{
		switch (type)
		{
		case Pine::RigidBody2DType::Static:
			return b2_staticBody;
		case Pine::RigidBody2DType::Dynamic:
			return b2_dynamicBody;
		case Pine::RigidBody2DType::Kinematic:
			return b2_kinematicBody;
		}

		return b2_staticBody;
	}
}

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
			Physics2D::GetWorld()->DestroyBody(m_Body);

			m_Body = nullptr;
			m_Fixture = nullptr;
		}
	}

	if (!m_Body)
	{
		const auto position = collider->ComputePosition();
		const auto size = collider->ComputeSize();

		b2BodyDef def;

		def.type = GetBodyType(m_RigidBodyType);
		def.position.Set(position.x, position.y);
		def.angle = collider->ComputeRotation();
		def.fixedRotation = true;

		b2PolygonShape shape;

		shape.SetAsBox(size.x, size.y);

		m_BodyType = m_RigidBodyType;
	    m_BodySize = size;

		m_Body = Physics2D::GetWorld()->CreateBody(&def);
		m_Fixture = m_Body->CreateFixture(&shape, 1.f);
	}

	const auto newPosition = collider->ComputePosition();

	m_Body->SetTransform(b2Vec2(newPosition.x, newPosition.y), collider->ComputeRotation());
}

Pine::RigidBody2D::RigidBody2D()
	: IComponent(ComponentType::RigidBody2D)
{
}

void Pine::RigidBody2D::SetRigidBodyType(RigidBody2DType type)
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

	const auto position = m_Body->GetPosition();
	const auto rotation = m_Body->GetAngle();

	auto transform = GetParent()->GetTransform();

	// this will fucking explode for objects with parents
	transform->LocalPosition = { position.x, position.y, 0.f };
}

void Pine::RigidBody2D::OnDestroyed()
{
	IComponent::OnDestroyed();

	if (m_Body)
	{
		Physics2D::GetWorld()->DestroyBody(m_Body);

		m_Body = nullptr;
		m_Fixture = nullptr;
	}
}

void Pine::RigidBody2D::OnCopied()
{
	IComponent::OnCopied();

	m_Body = nullptr;
	m_Fixture = nullptr;
}

void Pine::RigidBody2D::OnRender(float deltaTime)
{
	IComponent::OnRender(deltaTime);
}

void Pine::RigidBody2D::LoadData(const nlohmann::json& j)
{
	IComponent::LoadData(j);

	Serialization::LoadValue(j, "rtype", m_RigidBodyType);
}

void Pine::RigidBody2D::SaveData(nlohmann::json& j)
{
	IComponent::SaveData(j);

	j["rtype"] = m_RigidBodyType;
}
