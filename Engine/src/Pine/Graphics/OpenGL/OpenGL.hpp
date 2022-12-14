#pragma once
#include "Pine/Graphics/Interfaces/IGraphicsAPI.hpp"
#include <string>

namespace Pine::Graphics
{

    class OpenGL : public IGraphicsAPI
    {
    private:
        // We cache these in Setup()
        std::string m_VersionString;
        std::string m_GraphicsAdapter;
    public:
        bool Setup() override;
        void Shutdown() override;

        const char* GetName() override;
        const char* GetVersionString() override;
        const char* GetGraphicsAdapter() override;

        void ClearBuffers(Buffers buffers) override;
        void ClearColor(Color color) override;
    };

}