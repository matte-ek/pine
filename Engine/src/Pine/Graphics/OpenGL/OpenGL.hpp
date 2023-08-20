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

        void EnableErrorLogging() override;
        void DisableErrorLogging() override;

        const char* GetName() const override;
        const char* GetVersionString() const override;
        const char* GetGraphicsAdapter() const override;

        void ClearBuffers(std::uint32_t buffers) override;
        void ClearColor(Color color) override;

        void SetViewport(Vector2i position, Vector2i size) override;

        void SetDepthTestEnabled(bool value) override;
        void SetStencilTestEnabled(bool value) override;
        void SetFaceCullingEnabled(bool value) override;

        IVertexArray* CreateVertexArray() override;
        void DestroyVertexArray(IVertexArray* array) override;

        ITexture* CreateTexture() override;
        void DestroyTexture(ITexture* texture) override;

        int GetSupportedTextureSlots() override;

        IShaderProgram* CreateShaderProgram() override;
        void DestroyShaderProgram(IShaderProgram* program) override;

        IUniformBuffer* CreateUniformBuffer() override;
        void DestroyUniformBuffer(IUniformBuffer* buffer) override;

        IFrameBuffer* CreateFrameBuffer() override;
        void DestroyFrameBuffer(IFrameBuffer* buffer) override;
        void BindFrameBuffer(IFrameBuffer* buffer) override;

        void DrawArrays(RenderMode mode, int count) override;
        void DrawElements(RenderMode mode, int count) override;

        void DrawArraysInstanced(RenderMode mode, int count, int instanceCount) override;
        void DrawElementsInstanced(RenderMode mode, int count, int instanceCount) override;
    };

}