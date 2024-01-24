#include "Selection.hpp"

namespace
{

    std::vector<Pine::Entity*> m_SelectedEntities;
    std::vector<Pine::IAsset*> m_SelectedAssets;

}

void Selection::AddEntity(Pine::Entity* entity)
{
    if (entity == nullptr)
    {
        throw std::runtime_error("Entity is nullptr.");
    }

    for (int i = 0; i < m_SelectedEntities.size();i++)
    {
        if (m_SelectedEntities[i] == entity)
        {
            m_SelectedEntities.erase(m_SelectedEntities.begin() + i);

            // Yes, return is intentional.
            return;
        }
    }

    m_SelectedAssets.clear();
    m_SelectedEntities.push_back(entity);
}

void Selection::AddAsset(Pine::IAsset* asset)
{
    if (asset == nullptr)
    {
        throw std::runtime_error("Asset is nullptr.");
    }

    for (int i = 0; i < m_SelectedAssets.size();i++)
    {
        if (m_SelectedAssets[i] == asset)
        {
            m_SelectedAssets.erase(m_SelectedAssets.begin() + i);

            // Yes, return is intentional.
            return;
        }
    }

    m_SelectedEntities.clear();
    m_SelectedAssets.push_back(asset);
}

void Selection::Clear()
{
    m_SelectedEntities.clear();
    m_SelectedAssets.clear();
}

const std::vector<Pine::Entity*>& Selection::GetSelectedEntities()
{
    return m_SelectedEntities;
}

const std::vector<Pine::IAsset*>& Selection::GetSelectedAssets()
{
    return m_SelectedAssets;
}
