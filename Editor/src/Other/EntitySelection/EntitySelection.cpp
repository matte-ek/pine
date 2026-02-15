#include "EntitySelection.hpp"
#include "Pine/Rendering/RenderingContext.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Other/EditorEntity/EditorEntity.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/SpriteRenderer/SpriteRenderer.hpp"
#include "Pine/Rendering/Renderer3D/Renderer3D.hpp"
#include "Pine/Rendering/Renderer2D/Renderer2D.hpp"
#include "Pine/Rendering/Pipeline/Pipeline2D/Pipeline2D.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Rendering/RenderHandler.hpp"

namespace
{
    Pine::RenderingContext m_EntitySelectionRenderingContext;
    Pine::Graphics::IFrameBuffer *m_FrameBuffer = nullptr;

    Pine::Shader *m_ObjectSolidShader3D = nullptr;
    Pine::Shader* m_ObjectSolidShader2D = nullptr;

    bool m_PickEntity = false;
    bool m_PickMultiple = false;
    Pine::Vector2i m_CursorPosition;

    Pine::Vector3f ComputeColorIndex(int id)
    {
        // Compute the "color" we'll render with, i.e., entity index to base 256.
        float colorValue[3] = {0.f};
        int value = id;
        int pass = 0;

        while (value >= 0)
        {
            auto [quot, rem] = std::div(value, 256);

            colorValue[pass] = static_cast<float>(rem) / 256.f;

            if (quot == 0)
                break;

            value -= quot * 255;
            pass++;

            if (pass >= 2)
            {
                // We cannot encode this properly since the entity index is too high, should hopefully never
                // happen.
                Pine::Log::Warning(fmt::format("Failed to encode entity id {}, value is too high.", id));
                break;
            }
        }

        return { colorValue[0], colorValue[1], colorValue[2] };
    }

    Pine::Vector3f ComputeColorIndexNew(std::uint32_t id)
    {
        std::uint8_t byte0 = id & 0xFF;
        std::uint8_t byte1 = ((id >> 8) & 0xFF);
        std::uint8_t byte2 = ((id >> 16) & 0xFF);

        return { static_cast<float>(byte0) / 255.f, static_cast<float>(byte1) / 255.f, static_cast<float>(byte2) / 255.f };
    }

    void HandleRendering3D(const Pine::RenderingContext *context)
    {
        Pine::Renderer3D::FrameReset();
        Pine::Renderer3D::SetCamera(context->SceneCamera);
        Pine::Renderer3D::GetRenderConfiguration().OverrideShader = m_ObjectSolidShader3D;
        Pine::Renderer3D::GetRenderConfiguration().IgnoreShaderVersions = true;

        Pine::Graphics::GetGraphicsAPI()->SetDepthTestEnabled(true);

        for (const auto &modelRenderer: Pine::Components::Get<Pine::ModelRenderer>())
        {
            if (!modelRenderer.GetModel())
            {
                continue;
            }

            auto entity = modelRenderer.GetParent();

            int meshIndex = -1;
            for (const auto &mesh : modelRenderer.GetModel()->GetMeshes())
            {
                meshIndex++;

                if (modelRenderer.GetModelMeshIndex() != -1 && modelRenderer.GetModelMeshIndex() != meshIndex)
                {
                    continue;
                }

                Pine::Renderer3D::PrepareMesh(mesh);

                m_ObjectSolidShader3D->GetProgram()->GetUniformVariable("m_Color")->LoadVector3(ComputeColorIndexNew(entity->GetInternalId()));

                Pine::Renderer3D::RenderMesh(entity->GetTransform()->GetTransformationMatrix());
            }
        }

        Pine::Renderer3D::GetRenderConfiguration().OverrideShader = nullptr;
        Pine::Renderer3D::GetRenderConfiguration().IgnoreShaderVersions = false;
    }

    void HandleRendering2D(Pine::RenderingContext *context)
    {
        Pine::Renderer2D::SetOverrideShader(m_ObjectSolidShader2D);

        std::unordered_map<std::uint32_t, Pine::Vector4f> backupColors;
        
        for (auto& spriteRenderer : Pine::Components::Get<Pine::SpriteRenderer>())
        {
            backupColors[spriteRenderer.GetParent()->GetId()] = spriteRenderer.GetColor();

            spriteRenderer.SetColor(Pine::Vector4f(ComputeColorIndex(static_cast<int>(spriteRenderer.GetParent()->GetId())), 1.f));
        }

        Pine::Pipeline2D::Run(*context);

        for (auto& spriteRenderer : Pine::Components::Get<Pine::SpriteRenderer>())
        {
            spriteRenderer.SetColor(backupColors[spriteRenderer.GetParent()->GetId()]);
        }

        Pine::Renderer2D::SetOverrideShader(nullptr);
    }

    void OnRender(Pine::RenderingContext *context, Pine::RenderStage stage, float deltaTime)
    {
        if (!m_PickEntity)
        {
            return;
        }

        if (stage == Pine::RenderStage::PreRender)
        {
            m_EntitySelectionRenderingContext.Active = true;
            m_EntitySelectionRenderingContext.Size = RenderHandler::GetLevelRenderingContext()->Size;

            return;
        }

        if (context != &m_EntitySelectionRenderingContext ||
            stage != Pine::RenderStage::RenderContext)
        {
            return;
        }

        if (context->SceneCamera == nullptr)
        {
            return;
        }

        if (EditorEntity::GetPerspective2D())
        {
            HandleRendering2D(context);
        }
        else
        {
            HandleRendering3D(context);
        }

        std::uint8_t mouseColorData[3];

        m_FrameBuffer->ReadPixels(m_CursorPosition,
                                  Pine::Vector2i(1),
                                  Pine::Graphics::ReadFormat::RGB,
                                  Pine::Graphics::TextureDataFormat::UnsignedByte,
                                  sizeof(mouseColorData),
                                  mouseColorData);

        std::uint32_t entityId = *reinterpret_cast<std::uint32_t*>(mouseColorData);

        entityId &= 0x00FFFFFF;

        if (entityId != 0)
        {
            if (const auto pickedEntity = Pine::Entities::GetByInternalId(entityId))
            {
                Selection::Add(pickedEntity, !m_PickMultiple);
            }
            else
            {
                Pine::Log::Error(fmt::format("Picked entity with ID {} does not exist.", entityId));
            }
        }
        else
        {
            Selection::Clear();
        }

        m_EntitySelectionRenderingContext.Active = false;
        m_PickEntity = false;
    }
}

void EntitySelection::Setup()
{
    m_ObjectSolidShader3D = Pine::Assets::Get<Pine::Shader>("editor/shaders/generic-solid.shader");
    m_ObjectSolidShader2D = Pine::Assets::Get<Pine::Shader>("editor/shaders/generic-solid-2d.shader");

    m_FrameBuffer = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();
    m_FrameBuffer->Prepare();
    m_FrameBuffer->AttachTextures(1920, 1080, Pine::Graphics::Buffers::ColorBuffer | Pine::Graphics::Buffers::DepthBuffer);
    m_FrameBuffer->Finish();

    m_EntitySelectionRenderingContext.Active = false;
    m_EntitySelectionRenderingContext.Size = Pine::Vector2f(1920, 1080);
    m_EntitySelectionRenderingContext.UseRenderPipeline = false;
    m_EntitySelectionRenderingContext.EnableStencilBuffer = false;
    m_EntitySelectionRenderingContext.FrameBuffer = m_FrameBuffer;
    m_EntitySelectionRenderingContext.SceneCamera = EditorEntity::Get()->GetComponent<Pine::Camera>();

    Pine::RenderManager::AddRenderingContextPass(&m_EntitySelectionRenderingContext);
    Pine::RenderManager::AddRenderCallback(OnRender);
}

void EntitySelection::Dispose()
{
    Pine::Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_FrameBuffer);

    m_FrameBuffer = nullptr;
}

void EntitySelection::Pick(Pine::Vector2i cursorPosition, bool pickMultiple)
{
    m_PickEntity = true;
    m_CursorPosition = cursorPosition;
    m_PickMultiple = pickMultiple;
}
