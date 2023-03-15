#include "EditorEntity.hpp"

#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/NativeScript/NativeScript.hpp"

namespace
{

	Pine::Entity* m_Entity = nullptr;

	class EditorComponent : public Pine::NativeScript
	{
	private:
	public:
		void OnRender(float deltaTime) override
		{
			
		}

		void LoadData(const nlohmann::json& j) override {}
		void SaveData(nlohmann::json& j) override {}
	};

	
}

void EditorEntity::Setup()
{
	m_Entity = Pine::Entity::Create("dffdsfsfed");
//	m_Entity->SetTemporary(true);

//	m_Entity->AddComponent(new EditorComponent());
	m_Entity->AddComponent<Pine::Camera>();
}

void EditorEntity::Dispose()
{
}

Pine::Entity* EditorEntity::Get()
{
	return m_Entity;
}
