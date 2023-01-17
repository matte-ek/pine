#include "Blueprint.hpp"

Pine::Blueprint::Blueprint()
{
    m_Type = AssetType::Blueprint;
    m_LoadMode = AssetLoadMode::MultiThread;
}

void Pine::Blueprint::CopyEntity(Pine::Entity* dst, const Pine::Entity* src, bool createInstance) const
{
}

void Pine::Blueprint::Dispose()
{
    delete m_Entity;
}