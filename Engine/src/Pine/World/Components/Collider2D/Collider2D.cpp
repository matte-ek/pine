#include "Collider2D.hpp"

Pine::Collider2D::Collider2D()
	: IComponent(ComponentType::Collider2D)
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

void Pine::Collider2D::SetOffset(Pine::Vector2f offset)
{
	m_ColliderOffset = offset;
}

const Pine::Vector2f& Pine::Collider2D::GetOffset() const
{
	return m_ColliderOffset;
}

void Pine::Collider2D::SetSize(Pine::Vector2f size)
{
	m_ColliderSize = size;
}

const Pine::Vector2f& Pine::Collider2D::GetSize() const
{
	return m_ColliderSize;
}

void Pine::Collider2D::OnRender(float deltaTime)
{
}

void Pine::Collider2D::LoadData(const nlohmann::json& j)
{
}

void Pine::Collider2D::SaveData(nlohmann::json& j)
{
}
