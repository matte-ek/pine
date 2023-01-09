#include "RenderManager.hpp"
#include <vector>

namespace
{

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
}

void Pine::RenderManager::Shutdown()
{
}

void Pine::RenderManager::Run()
{
    CallRenderCallback(RenderStage::PreRender);

    CallRenderCallback(RenderStage::PostRender);
}

void Pine::RenderManager::AddRenderCallback(const std::function<void(RenderStage)>& func)
{
    m_RenderCallbackFunctions.push_back(func);
}
