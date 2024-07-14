#include "Gizmo2D.hpp"

#include "imgui.h"
#include "IconsMaterialDesign.h"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Rendering/RenderHandler.hpp"
#include "Pine/Rendering/Renderer2D/Renderer2D.hpp"
#include <Pine/World/Components/SpriteRenderer/SpriteRenderer.hpp>

namespace
{

	void OnRender(Pine::RenderingContext* context, const Pine::RenderStage stage, float deltaTime)
	{
		if (context != RenderHandler::GetLevelRenderingContext())
		{
			return;
		}

		if (stage == Pine::RenderStage::PostRender2D)
		{
			Pine::Renderer2D::PrepareFrame();
			Pine::Renderer2D::SetCoordinateSystem(Pine::Rendering::CoordinateSystem::Screen);

			Pine::Renderer2D::RenderFrame(context);
		}
	}

	void RenderEntitySelectionBox(const Pine::Vector2f& viewportPositionOffset, const Pine::Vector2f& viewportSizeFactor)
	{
		auto levelRenderingContext = RenderHandler::GetLevelRenderingContext();
		const auto& selectedEntities = Selection::GetSelectedEntities();

		if (selectedEntities.empty())
			return;

		for (auto& entity : selectedEntities)
		{
			if (auto spriteRenderer = entity->GetComponent<Pine::SpriteRenderer>())
			{
				if (!spriteRenderer->GetTexture())
				{
					continue;
				}

				const auto positionWorld = entity->GetTransform()->GetPosition();

				const auto size = Pine::Vector2f(spriteRenderer->GetTexture()->GetWidth(), spriteRenderer->GetTexture()->GetHeight()) * Pine::Vector2f(entity->GetTransform()->GetScale()) / static_cast<float>(Pine::Rendering::PixelsPerUnit * 2);

				const auto upperLeftWorld = positionWorld - Pine::Vector3f(size, 0.f);
				const auto bottomRightWorld = positionWorld + Pine::Vector3f(size, 0.f);

				const auto upperLeftFrameBuffer = levelRenderingContext->SceneCamera->WorldToScreenPoint(upperLeftWorld);
				const auto bottomRightFrameBuffer = levelRenderingContext->SceneCamera->WorldToScreenPoint(bottomRightWorld);

				const auto upperLeftScreen = Pine::Vector2f(viewportPositionOffset.x + upperLeftFrameBuffer.x * viewportSizeFactor.x, viewportPositionOffset.y + upperLeftFrameBuffer.y * viewportSizeFactor.y);
				const auto bottomRightScreen = Pine::Vector2f(viewportPositionOffset.x + bottomRightFrameBuffer.x * viewportSizeFactor.x, viewportPositionOffset.y + bottomRightFrameBuffer.y * viewportSizeFactor.y);

				ImGui::GetWindowDrawList()->AddRect(ImVec2(static_cast<int>(upperLeftScreen.x), static_cast<int>(upperLeftScreen.y)), ImVec2(static_cast<int>(bottomRightScreen.x), static_cast<int>(bottomRightScreen.y)), ImColor(255, 150, 0));
				ImGui::GetWindowDrawList()->AddRect(ImVec2(static_cast<int>(upperLeftScreen.x) - 1, static_cast<int>(upperLeftScreen.y) + 1), ImVec2(static_cast<int>(bottomRightScreen.x) + 1, static_cast<int>(bottomRightScreen.y) - 1), ImColor(0, 0, 0, 100));
				ImGui::GetWindowDrawList()->AddRect(ImVec2(static_cast<int>(upperLeftScreen.x) + 1, static_cast<int>(upperLeftScreen.y) - 1), ImVec2(static_cast<int>(bottomRightScreen.x) - 1, static_cast<int>(bottomRightScreen.y) + 1), ImColor(0, 0, 0, 100));
			}
		}
	}

	void RenderTilemapHandler()
	{
	}

}

void Gizmo::Gizmo2D::Setup()
{
	Pine::RenderManager::AddRenderCallback(OnRender);
}

void Gizmo::Gizmo2D::Render(const Pine::Vector2f& position, const Pine::Vector2f& size)
{
	ImGui::GetWindowDrawList()->PushClipRect(ImVec2(position.x, position.y), ImVec2(position.x + size.x, position.y + size.y));

	const auto viewportScale = Pine::Vector2f(size) / RenderHandler::GetLevelRenderingContext()->Size;

	const auto gameRenderingContext = RenderHandler::GetGameRenderingContext();
	const auto levelRenderingContext = RenderHandler::GetLevelRenderingContext();

	if (gameRenderingContext->SceneCamera != nullptr)
	{
		const float sizeWidth = ((RenderHandler::GetGameFrameBuffer()->GetSize().x / Pine::Rendering::PixelsPerUnit) / 2.f);
		const float sizeHeight = ((RenderHandler::GetGameFrameBuffer()->GetSize().y / Pine::Rendering::PixelsPerUnit) / 2.f);

		const auto gameCameraPosition = gameRenderingContext->SceneCamera->GetParent()->GetTransform()->GetPosition();

		const auto upperLeft = levelRenderingContext->SceneCamera->WorldToScreenPoint(gameCameraPosition + (Pine::Vector3f(-sizeWidth, -sizeHeight, 0.f)));
		const auto bottomRight = levelRenderingContext->SceneCamera->WorldToScreenPoint(gameCameraPosition + (Pine::Vector3f(sizeWidth, sizeHeight, 0.f)));

		ImGui::GetWindowDrawList()->AddRect(ImVec2(static_cast<int>(position.x + (upperLeft.x * viewportScale.x)), static_cast<int>(position.y + (upperLeft.y * viewportScale.y))),
			ImVec2(static_cast<int>(position.x + (bottomRight.x * viewportScale.x)), static_cast<int>(position.y + (bottomRight.y * viewportScale.y))),
			ImColor(255, 255, 255, 255),
			0.f);


		ImGui::GetWindowDrawList()->AddRect(ImVec2(static_cast<int>(position.x + (upperLeft.x * viewportScale.x)) - 1, static_cast<int>(position.y + (upperLeft.y * viewportScale.y)) + 1),
			ImVec2(static_cast<int>(position.x + (bottomRight.x * viewportScale.x)) + 1, static_cast<int>(position.y + (bottomRight.y * viewportScale.y)) - 1),
			ImColor(0, 0, 0, 150),
			0.f);

		/*
		ImGui::GetWindowDrawList()->AddRect(ImVec2(static_cast<int>(position.x + (upperLeft.x * viewportScale.x)) + 1, static_cast<int>(position.y + (upperLeft.y * viewportScale.y)) - 1),
											ImVec2(static_cast<int>(position.x + (bottomRight.x * viewportScale.x)) - 1, static_cast<int>(position.y + (bottomRight.y * viewportScale.y)) + 1),
											ImColor(0, 0, 0, 150),
											0.f);
		*/
	}

	RenderEntitySelectionBox(position, viewportScale);

	ImGui::GetWindowDrawList()->PopClipRect();
}