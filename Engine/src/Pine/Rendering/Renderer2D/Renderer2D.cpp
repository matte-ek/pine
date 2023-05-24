#include "Renderer2D.hpp"

#include <iostream>

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Graphics/Interfaces/IGraphicsAPI.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/TextureAtlas/TextureAtlas.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <stb_truetype.h>

using namespace Pine;

namespace
{
    // Max amount of instances the 2D renderer may render before moving onto another draw call.
    constexpr int MaxInstanceCount = 4096;

    // Cached graphics API for the current context
    Graphics::IGraphicsAPI* m_GraphicsAPI = nullptr;

    // This default texture is a solid white pixel, that we treat as a "no-texture" texture.
    Graphics::ITexture* m_DefaultTexture = nullptr;

    Rendering::CoordinateSystem m_CoordinateSystem = Rendering::CoordinateSystem::Screen;

    // Matrices store globally for each PrepareFrame() call
    Matrix4f m_ProjectionMatrix;
	Matrix4f m_ViewMatrix;

    // Base properties that most rendering items require
    struct RenderItem
    {
        Vector2f m_Position;
        Vector2f m_Size;
        float m_Rotation = 0.f;
        Color m_Color;
        Rendering::CoordinateSystem m_CoordinateSystem;
    };

    struct RectangleItem : RenderItem
    {
        float m_Radius = 0.0f;
        Graphics::ITexture* m_Texture = nullptr;
        Vector2f m_UvOffset = Vector2f(0.f);
        Vector2f m_UvScale = Vector2f(1.f);
    };

    std::vector<RectangleItem> m_Rectangles;
    std::vector<RectangleItem> m_FilledRectangles;

    Vector4f ComputePositionSize(const RenderingContext* context, Vector2f position, Vector2f size, Rendering::CoordinateSystem coordinateSystem)
    {
        // Compute width and height
        const float w = size.x / context->m_Size.x;
        const float h = size.y / context->m_Size.y;

        // Compute position
        float x;
        float y;

        if (coordinateSystem == Rendering::CoordinateSystem::Screen)
        {
            // Transform the screen coordinates to normalized (0-1) coordinates
            x = (position.x / context->m_Size.x) * 2.0f;
            y = (position.y / context->m_Size.y) * 2.0f;

            // Move the origin to the top left of the screen
            x += -1.0f + w;
            y += -1.0f + h;

            // Y also needs to be inverted.
            y = -y;
        }
        else
        {
            x = position.x;
            y = position.y;
        }

        return {x, y, w, h};
    }

    // I kind of dislike how this code is structured right now
    // might work on this later as it's not a high priority right now.
    namespace RectangleRenderer
    {
        struct RectangleInstanceRenderContext
        {
            Graphics::IVertexArray* m_VertexArray = nullptr;

            Graphics::IVertexBuffer* m_PositionSizeBuffer = nullptr;
            Graphics::IVertexBuffer* m_UvRadiusBuffer = nullptr;
            Graphics::IVertexBuffer* m_ColorBuffer = nullptr;
            Graphics::IVertexBuffer* m_TextureRotationBuffer = nullptr;

            Shader* m_Shader = nullptr;

            bool m_Ready = false;

            // If we're drawing as a line loop, we'll get a non-filled rectangle
            bool m_LineLoop = false;

            void Create(bool lineLoop = false)
            {
                m_VertexArray = m_GraphicsAPI->CreateVertexArray();
                m_VertexArray->Bind();

                m_LineLoop = lineLoop;

                if (lineLoop)
                {
                    const std::vector<float> vertices =
                    {
                        -1.f, 1.f,
                        1.f, 1.f,
                        1.f, -1.f,
                        -1.f, -1.f,
                    };

                    m_VertexArray->Bind();
                    m_VertexArray->StoreFloatArrayBuffer(vertices, 0, 2, Graphics::BufferUsageHint::StaticDraw);
                }
                else
                {
                    const std::vector<float> vertices =
                    {
                        -1.f, 1.f, 0.f,
                        -1.f, -1.f, 0.f,
                        1.f, -1.f, 0.f,
                        1.f, 1.f, 0.f,
                    };

                    const std::vector<int> indices =
                    {
                        0,1,3,
                        3,1,2
                    };

                    const std::vector<float> uvs = { 0, 0, 0, 1, 1, 1, 1, 0 };

                    m_VertexArray->StoreFloatArrayBuffer(vertices, 0, 3, Graphics::BufferUsageHint::StaticDraw);
                    m_VertexArray->StoreFloatArrayBuffer(uvs, 1, 2, Graphics::BufferUsageHint::StaticDraw);
                    m_VertexArray->StoreElementArrayBuffer(indices);
                }

                m_PositionSizeBuffer = m_VertexArray->CreateFloatArrayBuffer(sizeof(Vector4f) * MaxInstanceCount, 2, 4, Graphics::BufferUsageHint::DynamicDraw);
                m_PositionSizeBuffer->SetDivisor(Graphics::VertexBufferDivisor::PerInstance, 1);

                m_UvRadiusBuffer = m_VertexArray->CreateFloatArrayBuffer(sizeof(Vector4f) * MaxInstanceCount, 3, 4, Graphics::BufferUsageHint::DynamicDraw);
                m_UvRadiusBuffer->SetDivisor(Graphics::VertexBufferDivisor::PerInstance, 1);

                m_ColorBuffer = m_VertexArray->CreateFloatArrayBuffer(sizeof(Vector4f) * MaxInstanceCount, 4, 4, Graphics::BufferUsageHint::DynamicDraw);
                m_ColorBuffer->SetDivisor(Graphics::VertexBufferDivisor::PerInstance, 1);

                m_TextureRotationBuffer = m_VertexArray->CreateFloatArrayBuffer(sizeof(Vector3f) * MaxInstanceCount, 5, 3, Graphics::BufferUsageHint::DynamicDraw);
                m_TextureRotationBuffer->SetDivisor(Graphics::VertexBufferDivisor::PerInstance, 1);

                m_Ready = true;
            }

            void Render(RenderingContext* context, const std::vector<RectangleItem>& rects) const
            {
                m_Shader->GetProgram()->Use();
                m_VertexArray->Bind();

                m_GraphicsAPI->SetActiveTexture(0);
                m_DefaultTexture->Bind();

                m_Shader->GetProgram()->GetUniformVariable("m_ViewMatrix")->LoadMatrix4(m_ViewMatrix);
                m_Shader->GetProgram()->GetUniformVariable("m_ProjectionMatrix")->LoadMatrix4(m_ProjectionMatrix);

                // Prepare the new instance data for the next batch of rectangles
                std::vector<Vector4f> rectPositionSizeData;
                std::vector<Vector4f> rectUvTransformData;
                std::vector<Vector4f> rectColorData;
                std::vector<Vector3f> rectTextureIndexRadiusData;

                int startIndex = 0;
                while (startIndex < static_cast<int>(rects.size()))
                {
                	int minSize = std::min(static_cast<int>(rects.size()) - startIndex, MaxInstanceCount);

                    // To avoid re-allocations, we can fill out the vector directly
                    rectPositionSizeData.resize(minSize);
                    rectUvTransformData.resize(minSize);
                    rectColorData.resize(minSize);
                    rectTextureIndexRadiusData.resize(minSize);

                    int vertexBufferIndex = 0;
                    int currentTextureSlot = 1;

                    // To keep track of which textures we currently have in this instance batch,
                    // and their respective indices.
                    std::unordered_map<Graphics::ITexture*, int> textures;

                    // Fill up the vertex buffer instance data
                    for (int i = startIndex; i < startIndex + minSize;i++)
                    {
                        const auto& rect = rects[i];

                        // If we've reached the instance count limit, hop out of the loop
                        if (vertexBufferIndex >= MaxInstanceCount)
                        {
                            minSize = i - startIndex;
                            break;
                        }

                        // Or if we've reached the maximum amount of textures
                        if (currentTextureSlot >= m_GraphicsAPI->GetSupportedTextureSlots())
                        {
                            minSize = i - startIndex;
                            break;
                        }

                        rectPositionSizeData[vertexBufferIndex] = ComputePositionSize(context, rect.m_Position, rect.m_Size, rect.m_CoordinateSystem);

                        rectUvTransformData[vertexBufferIndex].x = rect.m_UvOffset.x;
                        rectUvTransformData[vertexBufferIndex].y = rect.m_UvOffset.y;
                        rectUvTransformData[vertexBufferIndex].z = rect.m_UvScale.x;
                        rectUvTransformData[vertexBufferIndex].w = rect.m_UvScale.y;

                        rectColorData[vertexBufferIndex] = Vector4f(static_cast<float>(rect.m_Color.r) / 255.f,
                                                                    static_cast<float>(rect.m_Color.g) / 255.f,
                                                                    static_cast<float>(rect.m_Color.b) / 255.f,
                                                                    static_cast<float>(rect.m_Color.a) / 255.f);

                        rectTextureIndexRadiusData[vertexBufferIndex].y = rect.m_Radius;
                        rectTextureIndexRadiusData[vertexBufferIndex].z = -rect.m_Rotation;

                        if (rect.m_Texture != nullptr)
                        {
                            if (textures.count(rect.m_Texture) == 0)
                            {
                                m_GraphicsAPI->SetActiveTexture(currentTextureSlot);
                                rect.m_Texture->Bind();

                                textures[rect.m_Texture] = currentTextureSlot;

                                currentTextureSlot++;
                            }

                            rectTextureIndexRadiusData[vertexBufferIndex].x = static_cast<float>(textures[rect.m_Texture]);
                        }
                        else
                        {
                            rectTextureIndexRadiusData[vertexBufferIndex].x = 0.f;
                        }

                        vertexBufferIndex++;
                    }

                    // Upload the data to the vertex buffers
                    m_PositionSizeBuffer->Bind();
                    m_PositionSizeBuffer->UploadData(rectPositionSizeData.data(), sizeof(Vector4f) * minSize, 0);

                    m_UvRadiusBuffer->Bind();
                    m_UvRadiusBuffer->UploadData(rectUvTransformData.data(), sizeof(Vector4f) * minSize, 0);

                    m_ColorBuffer->Bind();
                    m_ColorBuffer->UploadData(rectColorData.data(), sizeof(Vector4f) * minSize, 0);

                    m_TextureRotationBuffer->Bind();
                    m_TextureRotationBuffer->UploadData(rectTextureIndexRadiusData.data(), sizeof(Vector3f) * minSize, 0);

                    // Render the quads
                    if (m_LineLoop)
                        m_GraphicsAPI->DrawArraysInstanced(Graphics::RenderMode::LineLoop, 4, minSize);
                    else
                        m_GraphicsAPI->DrawElementsInstanced(Graphics::RenderMode::Triangles, 12, minSize);

                    context->m_DrawCalls++;

                    startIndex += minSize;
                }
            }
        };

        RectangleInstanceRenderContext m_RectangleRender;
        RectangleInstanceRenderContext m_FilledRectangleRender;

        void PrepareFrame()
        {
            // Make sure to set shaders if we haven't already
            if (m_RectangleRender.m_Shader == nullptr)
                m_RectangleRender.m_Shader = Assets::Get<Shader>("engine/shaders/2d/rect.shader");
            if (m_FilledRectangleRender.m_Shader == nullptr)
                m_FilledRectangleRender.m_Shader = Assets::Get<Shader>("engine/shaders/2d/rect-filled.shader");

            if (!m_RectangleRender.m_Ready)
                m_RectangleRender.Create();
            if (!m_FilledRectangleRender.m_Ready)
                m_FilledRectangleRender.Create();
        }

        void RenderFrame(RenderingContext* context)
        {
            // Make sure we have all the required shaders, should be set in PrepareFrame()
            if (m_RectangleRender.m_Shader == nullptr || m_FilledRectangleRender.m_Shader == nullptr)
            {
                throw std::runtime_error("Renderer2D::RenderFrame(): Missing essential shaders.");
            }

            // If we don't have any rectangles to process we can just exit
            if (m_Rectangles.empty() && m_FilledRectangles.empty())
            {
                return;
            }

            m_FilledRectangles.reserve(context->m_PreAllocItems);

            if (!m_FilledRectangles.empty())
                m_FilledRectangleRender.Render(context, m_FilledRectangles);
            if (!m_Rectangles.empty())
                m_RectangleRender.Render(context, m_Rectangles);
        }
    }
}

void Pine::Renderer2D::PrepareFrame()
{
    // Cache Graphics API
    m_GraphicsAPI = Graphics::GetGraphicsAPI();

    // Create the default texture if required
    if (m_DefaultTexture == nullptr)
    {
        m_DefaultTexture = m_GraphicsAPI->CreateTexture();

        // Create a 1x1 solid white pixel texture
        auto* textureData = static_cast<std::uint8_t*>(malloc(sizeof(std::uint8_t) * 4));
        
        for (size_t i = 0; i < sizeof(std::uint8_t) * 4;i++)
            textureData[i] = 255;

        m_DefaultTexture->Bind();
        m_DefaultTexture->UploadTextureData(1, 1, Graphics::TextureFormat::RGBA, Graphics::TextureDataFormat::UnsignedByte, textureData);

        free(textureData);
    }

    RectangleRenderer::PrepareFrame();

    // Clear up stuff from the last frame
    m_FilledRectangles.clear();
    m_Rectangles.clear();
}

void Renderer2D::RenderFrame(RenderingContext* context)
{
    if (!context)
    {
        throw std::runtime_error("Renderer2D::RenderFrame(): No rendering context provided");
    }

    if (context->m_Camera)
    {
        m_ProjectionMatrix = context->m_Camera->GetProjectionMatrix();
        m_ViewMatrix = context->m_Camera->GetViewMatrix();
    }
    else
    {
        m_ProjectionMatrix = Matrix4f(1.f);
    	m_ViewMatrix = Matrix4f(1.f);
    }

    Graphics::GetGraphicsAPI()->SetViewport(Vector2i(0), context->m_Size);

    RectangleRenderer::RenderFrame(context);
}

void Pine::Renderer2D::AddRectangle(Pine::Vector2f position, Pine::Vector2f size, Pine::Color color)
{
    RectangleItem rectangleItem =
    {
        position,
        size,
        0.f,
        color,
        m_CoordinateSystem
    };

    m_Rectangles.push_back(rectangleItem);
}

void Pine::Renderer2D::AddFilledRectangle(Pine::Vector2f position, Pine::Vector2f size, float rotation, Pine::Color color)
{
    RectangleItem rectangleItem =
    {
        position,
        size,
        rotation,
        color,
        m_CoordinateSystem
    };

    m_FilledRectangles.push_back(rectangleItem);
}

void Renderer2D::AddFilledTexturedRectangle(Vector2f position, Vector2f size, float rotation, Color color, const Texture2D* texture, Vector2f uvOffset, Vector2f uvScale)
{
    RectangleItem rectangleItem =
    {
        position,
        size,
        rotation,
        color,
        m_CoordinateSystem,
        0.f,
        texture->GetGraphicsTexture(),
        uvOffset,
        uvScale
    };

    m_FilledRectangles.push_back(rectangleItem);
}

void Renderer2D::AddFilledTexturedRectangle(Vector2f position, Vector2f size, float rotation, Color color, Graphics::ITexture* texture,
                                            Vector2f uvOffset, Vector2f uvScale)
{
    RectangleItem rectangleItem =
    {
        position,
        size,
        rotation,
        color,
    	m_CoordinateSystem,
        0.f,
        texture,
        uvOffset,
        uvScale
    };

    m_FilledRectangles.push_back(rectangleItem);
}

void Pine::Renderer2D::AddFilledRoundedRectangle(Pine::Vector2f position, Pine::Vector2f size, float radius, Pine::Color color)
{
    RectangleItem rectangleItem =
    {
        position,
        size,
        0.f,
        color,
        m_CoordinateSystem,
        radius
    };

    m_FilledRectangles.push_back(rectangleItem);
}

void Pine::Renderer2D::AddText(Pine::Vector2f position, Pine::Color color, const std::string& str)
{

}

void Renderer2D::AddTextureAtlasItem(Vector2f position, float size, const Graphics::TextureAtlas* atlas, std::uint32_t itemId,
                                     Color color)
{
    const auto uvScale = atlas->GetTextureUvScale();

    RectangleItem rectangleItem =
    {
        position,
        Vector2f(size),
        0.f,
        color,
        m_CoordinateSystem,
        0.f,
        atlas->GetColorBuffer(),
        atlas->GetTextureUvOffset(itemId),
        Vector2f(uvScale, -uvScale)
    };

    m_FilledRectangles.push_back(rectangleItem);
}

void Renderer2D::SetCoordinateSystem(Rendering::CoordinateSystem coordinateSystem)
{
    m_CoordinateSystem = coordinateSystem;
}

Rendering::CoordinateSystem Renderer2D::GetCoordinateSystem()
{
    return m_CoordinateSystem;
}
