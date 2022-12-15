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

        const char* GetName() const override;
        const char* GetVersionString() const override;
        const char* GetGraphicsAdapter() const override;

        void ClearBuffers(Buffers buffers) override;
        void ClearColor(Color color) override;

        IVertexArray* CreateVertexArray() override;
        void DestroyVertexArray(IVertexArray* array) override;
    };

}