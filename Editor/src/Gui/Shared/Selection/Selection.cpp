#include "Selection.hpp"

namespace
{

    Pine::Entity* m_SelectedEntity = nullptr;
    Pine::IAsset* m_SelectedAsset = nullptr;

}

void Selection::SelectEntity(Pine::Entity* entity)
{
    m_SelectedEntity = entity;
    m_SelectedAsset = nullptr;
}

void Selection::SelectAsset(Pine::IAsset* asset)
{
    m_SelectedEntity = nullptr;
    m_SelectedAsset = asset;
}

void Selection::Clear()
{
    m_SelectedEntity = nullptr;
    m_SelectedAsset = nullptr;
}

Pine::Entity* Selection::GetSelectedEntity()
{
    return m_SelectedEntity;
}

Pine::IAsset* Selection::GetSelectedAsset()
{
    return m_SelectedAsset;
}
