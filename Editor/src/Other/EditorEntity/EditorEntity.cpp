#include "EditorEntity.hpp"

#include "Pine/Input/Input.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/NativeScript/NativeScript.hpp"
#include <GLFW/glfw3.h>

namespace
{

	Pine::InputContext* m_EditorInputContext = nullptr;

	Pine::InputBind* m_Forward = nullptr;
	Pine::InputBind* m_Sideways = nullptr;
	Pine::InputBind* m_Pitch = nullptr;
	Pine::InputBind* m_Yaw = nullptr;
	Pine::InputBind* m_Boost = nullptr;

	Pine::Entity* m_Entity = nullptr;

    bool m_CaptureMouse = false;
    Pine::Vector2f m_ViewAngles = Pine::Vector2f(0.f);

    bool m_Perspective2D = false;

	float m_SpeedMultiplier = 1.f;

	Pine::Vector3f m_Velocity = Pine::Vector3f(0.f);

	class EditorComponent final : public Pine::NativeScript
	{
		Pine::Camera* m_Camera = nullptr;

        void HandleMovement3D(float deltaTime) const
        {
            if (m_Perspective2D)
                return;

            const auto transform = m_Parent->GetTransform();

        	if (m_CaptureMouse)
			{
				constexpr float sensitivity = 20.0f;
        		const float speedMultiplier = m_Boost->GetAxisValue() + m_SpeedMultiplier;

				m_Velocity += transform->GetForward() * m_Forward->GetAxisValue() * deltaTime * 0.275f * speedMultiplier;
        		m_Velocity += transform->GetRight() * m_Sideways->GetAxisValue() * deltaTime * 0.275f * speedMultiplier;

        		if (fabsf(m_Forward->GetAxisValue()) + fabsf(m_Sideways->GetAxisValue()) < 0.1f)
        		{
        			m_Velocity *= 0.96f;
        		}
        		else
        		{
        			if (glm::length(m_Velocity) > .05f * speedMultiplier)
        			{
        				m_Velocity = glm::normalize(m_Velocity) * .025f * speedMultiplier;
        			}
        		}

        		m_ViewAngles.x += m_Pitch->GetAxisValue() * sensitivity * deltaTime;
        		m_ViewAngles.y += m_Yaw->GetAxisValue() * sensitivity * deltaTime;

        		transform->SetEulerAngles(Pine::Vector3f(m_ViewAngles, 0.f));
			}
        	else
        	{
        		m_Velocity *= 0.96f;
        	}

        	transform->SetLocalPosition(transform->GetLocalPosition() + m_Velocity);
        }

        void HandleMovement2D(float deltaTime) const
        {
            const auto transform = m_Parent->GetTransform();

            if (!m_Perspective2D)
                return;

            if (m_CaptureMouse)
            {
	            constexpr float sensitivity = 0.025f;

                const float zoomFactor = m_Camera->GetOrthographicSize() * 0.5f + 0.5f;

                // Allow moving with the mouse
            	transform->SetLocalPosition(transform->GetLocalPosition() + Pine::Vector3f(0.f, -1.f, 0.f) * m_Pitch->GetAxisValue() * sensitivity * zoomFactor);
                transform->SetLocalPosition(transform->GetLocalPosition() + Pine::Vector3f(1.f, 0.f, 0.f) * m_Yaw->GetAxisValue() * sensitivity * zoomFactor);

                // Allow moving with the keyboard (put this outside m_CaptureMouse?)
                transform->SetLocalPosition(transform->GetLocalPosition() + Pine::Vector3f(0.f, 1.f, 0.f) * m_Forward->GetAxisValue() * deltaTime * m_SpeedMultiplier);
                transform->SetLocalPosition(transform->GetLocalPosition() + Pine::Vector3f(1.f, 0.f, 0.f) * m_Sideways->GetAxisValue() * deltaTime * m_SpeedMultiplier);
            }

            transform->SetLocalPosition({transform->GetLocalPosition().x, transform->GetLocalPosition().y, 0.1f});
            transform->SetEulerAngles(Pine::Vector3f(0.f, 0.f, 0.f));
        }

	public:
        void OnSetup() override
        {
            m_Camera = m_Parent->GetComponent<Pine::Camera>();
        }

		void OnRender(float deltaTime) override
        {
            HandleMovement2D(deltaTime);
            HandleMovement3D(deltaTime);
		}
	};

}

void EditorEntity::Setup()
{
	/* Setup Inputs */
	m_EditorInputContext = Pine::Input::CreateContext("Editor");

	m_Forward = m_EditorInputContext->CreateInputBinding("Forward");
	m_Sideways = m_EditorInputContext->CreateInputBinding("Sideways");
	m_Pitch = m_EditorInputContext->CreateInputBinding("Pitch");
	m_Yaw = m_EditorInputContext->CreateInputBinding("Yaw");
	m_Boost = m_EditorInputContext->CreateInputBinding("Boost Speed");

	m_Forward->AddKeyboardBinding(GLFW_KEY_W, 1.f);
	m_Forward->AddKeyboardBinding(GLFW_KEY_S, -1.f);

	m_Sideways->AddKeyboardBinding(GLFW_KEY_D, 1.f);
	m_Sideways->AddKeyboardBinding(GLFW_KEY_A, -1.f);

	m_Boost->AddKeyboardBinding(GLFW_KEY_LEFT_SHIFT, 1.f);

	m_Pitch->AddAxisBinding(Pine::Axis::MouseY);
	m_Yaw->AddAxisBinding(Pine::Axis::MouseX);

	/* Setup entity */
	m_Entity = Pine::Entity::Create("EditorEntity");

    m_Entity->SetTemporary(true);
	m_Entity->AddComponent(new EditorComponent());
	m_Entity->AddComponent<Pine::Camera>();

    m_Entity->GetComponent<EditorComponent>()->OnSetup();
}

void EditorEntity::Dispose()
{
}

Pine::Entity* EditorEntity::Get()
{
	return m_Entity;
}

void EditorEntity::SetCaptureMouse(bool value)
{
    m_CaptureMouse = value;
}

bool EditorEntity::GetCaptureMouse()
{
    return m_CaptureMouse;
}

bool EditorEntity::GetPerspective2D()
{
    return m_Perspective2D;
}

void EditorEntity::SetPerspective2D(bool value)
{
    m_Perspective2D = value;

    if (m_Entity != nullptr)
    {
        m_Entity->GetComponent<Pine::Camera>()->SetCameraType(value ? Pine::CameraType::Orthographic : Pine::CameraType::Perspective);
    }
}

float EditorEntity::GetSpeedMultiplier()
{
	return m_SpeedMultiplier;
}

void EditorEntity::SetSpeedMultiplier(float value)
{
	m_SpeedMultiplier = value;
}
