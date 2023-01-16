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

        int m_SupportedTextureSlots = 0;
    public:
        bool Setup() override;
        void Shutdown() override;

        const char* GetName() const override;
        const char* GetVersionString() const override;
        const char* GetGraphicsAdapter() const override;

        void ClearBuffers(Buffers buffers) override;
        void ClearColor(Color color) override;

        void SetViewport(Vector2i position, Vector2i size) override;

        IVertexArray* CreateVertexArray() override;
        void DestroyVertexArray(IVertexArray* array) override;

        ITexture* CreateTexture() override;
        void DestroyTexture(ITexture* texture) override;

        int GetSupportedTextureSlots() override;

        void SetActiveTexture(int binding) override;

        IShaderProgram* CreateShaderProgram() override;
        void DestroyShaderProgram(IShaderProgram* program) override;

        IFrameBuffer* CreateFrameBuffer() override;
        void DestroyFrameBuffer(IFrameBuffer* buffer) override;
        void BindFrameBuffer(IFrameBuffer* buffer) override;

        void DrawArrays(RenderMode mode, int count) override;
        void DrawElements(RenderMode mode, int count) override;

        void DrawArraysInstanced(RenderMode mode, int count, int instanceCount) override;
        void DrawElementsInstanced(RenderMode mode, int count, int instanceCount) override;
    };

}