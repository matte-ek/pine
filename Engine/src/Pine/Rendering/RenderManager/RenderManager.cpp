#include "RenderManager.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/Rendering/Pipeline/Pipeline2D/Pipeline2D.hpp"
#include <vector>

namespace
{

    Pine::RenderingContext m_DefaultRenderingContext;
    Pine::RenderingContext* m_CurrentRenderingContext = nullptr;

    std::vector<std::function<void(Pine::RenderStage)>> m_RenderCallbackFunctions;

    void CallRenderCallback(Pine::RenderStage stage)
    {
        for (const auto& func : m_RenderCallbackFunctions)
        {
            func(stage);
        }
    }

}

void Pine::RenderManager::Setup()
{
    m_DefaultRenderingContext.m_Size = Vector2f(Engine::GetEngineConfiguration().m_WindowSize);

    SetRenderingContext(&m_DefaultRenderingContext);

    Pipeline2D::Setup();
}

void Pine::RenderManager::Shutdown()
{
    Pipeline2D::Shutdown();
}

void Pine::RenderManager::Run()
{
    if (m_CurrentRenderingContext == nullptr)
    {
        return;
    }

    m_CurrentRenderingContext->m_DrawCalls = 0;

    if (m_CurrentRenderingContext->m_Camera)
        m_CurrentRenderingContext->m_Camera->OnRender(0.f);

    CallRenderCallback(RenderStage::PreRender);

    Pipeline2D::Run(*m_CurrentRenderingContext);

    CallRenderCallback(RenderStage::PostRender);
}

void Pine::RenderManager::AddRenderCallback(const std::function<void(RenderStage)>& func)
{
    m_RenderCallbackFunctions.push_back(func);
}

void Pine::RenderManager::SetRenderingContext(Pine::RenderingContext* context)
{
    m_CurrentRenderingContext = context;
}

Pine::RenderingContext* Pine::RenderManager::GetRenderingContext()
{
    return m_CurrentRenderingContext;
}

Pine::RenderingContext* Pine::RenderManager::GetDefaultRenderingContext()
{
    return &m_DefaultRenderingContext;
}
