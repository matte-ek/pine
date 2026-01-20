#pragma once

#include "IFrameBuffer.hpp"
#include "IShaderProgram.hpp"
#include "ITexture.hpp"
#include "IVertexArray.hpp"
#include "Pine/Core/Color/Color.hpp"

namespace Pine::Graphics
{

    enum class RenderMode
    {
        Triangles,
        LineLoop
    };

    enum class TestFunction
    {
        Never,
        Always,
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
        Equal,
        NotEqual
    };

    enum class StencilOperation
    {
        Keep,
        Zero,
        Replace,
        Increment,
        IncrementWrap,
        Decrement,
        DecrementWrap,
        Invert
    };

    enum class BlendingFunction
    {
        Zero,
        One,
        SourceColor,
        OneMinusSourceColor,
        DestinationColor,
        OneMinusDestinationColor,
        SourceAlpha,
        OneMinusSourceAlpha,
        DestinationAlpha,
        OneMinusDestinationAlpha,
        ConstantColor,
        OneMinusConstantColor,
        ConstantAlpha,
        OneMinusConstantAlpha,
        SourceAlphaSaturate
    };

    enum class FaceCullMode
    {
        Back,
        Front,
        FrontAndBack
    };

    class IGraphicsAPI
    {
    public:
        IGraphicsAPI() = default;
        virtual ~IGraphicsAPI() = default;

        virtual bool Setup() = 0;
        virtual void Shutdown() = 0;

        virtual void ResetInternalChangeTracking() = 0;

        virtual void EnableErrorLogging() = 0;
        virtual void DisableErrorLogging() = 0;

        // Example: OpenGL
        virtual const char* GetName() const = 0;

        // Graphics API version only
        virtual const char* GetVersionString() const = 0;

        // The name of the GPU
        virtual const char* GetGraphicsAdapter() const = 0;

        virtual void ClearBuffers(std::uint32_t buffers) = 0;
        virtual void ClearColor(Color color) = 0;

        virtual void SetViewport(Vector2i position, Vector2i size) = 0;

        virtual void SetBlendingEnabled(bool value) = 0;
        virtual void SetDepthTestEnabled(bool value) = 0;
        virtual void SetStencilTestEnabled(bool value) = 0;
        virtual void SetFaceCullingEnabled(bool value) = 0;
        virtual void SetMultiSampleEnabled(bool value) = 0;
        virtual void SetWireframeEnabled(bool value) = 0;

        virtual void SetBlendingFunction(BlendingFunction source, BlendingFunction destination) = 0;

        virtual void SetDepthFunction(TestFunction value) = 0;

        virtual void SetFaceCullingMode(FaceCullMode mode) = 0;

        virtual void SetStencilFunction(TestFunction function, int ref, int mask) = 0;
        virtual void SetStencilOperation(StencilOperation stencilFail, StencilOperation depthFail, StencilOperation depthPass) = 0;
        virtual void SetStencilMask(int mask) = 0;

        virtual IVertexArray* CreateVertexArray() = 0;
        virtual void DestroyVertexArray(IVertexArray* array) = 0;

        virtual ITexture* CreateTexture() = 0;
        virtual void DestroyTexture(ITexture* texture) = 0;

        virtual int GetSupportedTextureSlots() = 0;

        virtual IShaderProgram* CreateShaderProgram() = 0;
        virtual void DestroyShaderProgram(IShaderProgram* program) = 0;

        virtual IUniformBuffer* CreateUniformBuffer() = 0;
        virtual void DestroyUniformBuffer(IUniformBuffer* buffer) = 0;

        virtual IFrameBuffer* CreateFrameBuffer() = 0;
        virtual void DestroyFrameBuffer(IFrameBuffer* buffer) = 0;
        virtual void BindFrameBuffer(IFrameBuffer* buffer) = 0;

        virtual void DrawArrays(RenderMode mode, int count) = 0;
        virtual void DrawElements(RenderMode mode, int count) = 0;

        virtual void DrawArraysInstanced(RenderMode mode, int count, int instanceCount) = 0;
        virtual void DrawElementsInstanced(RenderMode mode, int count, int instanceCount) = 0;
    };

}