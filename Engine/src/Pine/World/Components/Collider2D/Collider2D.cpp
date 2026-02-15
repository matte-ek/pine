#include "Collider2D.hpp"

#include "Pine/Physics/Physics2D/Physics2D.hpp"

#include <box2d/b2_body.h>
#include <box2d/b2_world.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_polygon_shape.h>

#include "../../../Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/World/Components/SpriteRenderer/SpriteRenderer.hpp"
#include "Pine/World/Components/RigidBody2D/RigidBody2D.hpp"
#include "Pine/World/Entity/Entity.hpp"

void Pine::Collider2D::UpdateBody()
{
	const bool hasRigidBody = GetParent()->HasComponent<RigidBody2D>();

	bool shouldDestroyBody = false;

	// If we have a rigid body, we'll let the rigid body handle the body creation.
	if (hasRigidBody)
	{
		shouldDestroyBody = true;
	}

	// If the body exists and the size has changed, we need to recreate the body.
	if (m_Body && ComputeSize() != m_BodySize)
	{
		shouldDestroyBody = true;
	}

	if (shouldDestroyBody && m_Body)
	{
		Physics2D::GetWorld()->DestroyBody(m_Body);

		m_Body = nullptr;
		m_Fixture = nullptr; // TODO: Is this getting freed properly?
	}

	if (hasRigidBody)
	{
		return;
	}

	if (!m_Body)
	{
		Pine::Vector2f size = ComputeSize();
		b2BodyDef def;
		b2PolygonShape shape;

		def.position.Set(GetParent()->GetTransform()->GetPosition().x + m_ColliderOffset.x, GetParent()->GetTransform()->GetPosition().y + m_ColliderOffset.y);

		shape.SetAsBox(size.x, size.y);

		m_BodySize = size;
		m_Body = Physics2D::GetWorld()->CreateBody(&def);
		m_Fixture = m_Body->CreateFixture(&shape, 0.f);
	}

	const auto newPosition = ComputePosition();

	m_Body->SetTransform(b2Vec2(newPosition.x, newPosition.y), ComputeRotation());
}

Pine::Vector2f Pine::Collider2D::ComputePosition() const
{
	return (Pine::Vector2f(GetParent()->GetTransform()->GetPosition()) + m_ColliderOffset);
}

Pine::Vector2f Pine::Collider2D::ComputeSize() const
{
	Pine::Vector2f ret;

	Pine::Vector2f spriteSize;

	if (const auto spriteRenderer = GetParent()->GetComponent<Pine::SpriteRenderer>())
	{
		if (const auto texture = spriteRenderer->GetTexture())
		{
			spriteSize = Pine::Vector2f(texture->GetWidth(), texture->GetHeight()) / 64.f;
		}
	}

	switch (m_ColliderType)
	{
	case Collider2DType::Sprite:
		ret = m_ColliderSize * spriteSize;
		break;
	case Collider2DType::Box:
		ret = m_ColliderSize;
		break;
	default:
		break;
	}

	ret *= Pine::Vector2f(GetParent()->GetTransform()->GetScale());

	return ret * 0.5f;
}

float Pine::Collider2D::ComputeRotation() const
{
	return GetParent()->GetTransform()->GetRotation().x + m_ColliderRotation;
}

Pine::Collider2D::Collider2D()
	: Component(ComponentType::Collider2D)
{
}

void Pine::Collider2D::SetColliderType(Collider2DType type)
{
	m_ColliderType = type;
}

Pine::Collider2DType Pine::Collider2D::GetColliderType() const
{
	return m_ColliderType;
}

void Pine::Collider2D::SetOffset(Vector2f offset)
{
	m_ColliderOffset = offset;
}

const Pine::Vector2f& Pine::Collider2D::GetOffset() const
{
	return m_ColliderOffset;
}

void Pine::Collider2D::SetSize(Vector2f size)
{
	m_ColliderSize = size;
}

const Pine::Vector2f& Pine::Collider2D::GetSize() const
{
	return m_ColliderSize;
}

void Pine::Collider2D::SetRotation(float rotation)
{
	m_ColliderRotation = rotation;
}

float Pine::Collider2D::GetRotation() const
{
	return m_ColliderRotation;
}

void Pine::Collider2D::OnPrePhysicsUpdate()
{
	if (m_Standalone)
	{
		return;
	}

	UpdateBody();
}

void Pine::Collider2D::OnDestroyed()
{
	Component::OnDestroyed();

	if (m_Body)
	{
		Physics2D::GetWorld()->DestroyBody(m_Body);

		m_Body = nullptr;
		m_Fixture = nullptr;
	}
}

void Pine::Collider2D::OnCopied()
{
	Component::OnCopied();

	m_Body = nullptr;
	m_Fixture = nullptr;
}

void Pine::Collider2D::OnRender(float deltaTime)
{
}

void Pine::Collider2D::LoadData(const ByteSpan& span)
{
    Collider2DSerializer serializer;

    serializer.Read(span);

    serializer.Type.Read(m_ColliderType);
    serializer.Offset.Read(m_ColliderOffset);
    serializer.Size.Read(m_ColliderSize);
    serializer.Rotation.Read(m_ColliderRotation);
}

Pine::ByteSpan Pine::Collider2D::SaveData()
{
    Collider2DSerializer serializer;

    serializer.Type.Write(m_ColliderType);
    serializer.Offset.Write(m_ColliderOffset);
    serializer.Size.Write(m_ColliderSize);
    serializer.Rotation.Write(m_ColliderRotation);

    return serializer.Write();
}