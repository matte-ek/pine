#include <mono/jit/jit.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/assembly.h>
#include <cassert>
#include "ScriptObjectFactory.hpp"
#include "Pine/Script/Runtime/ScriptingRuntime.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    MonoDomain *m_Domain = nullptr;
    MonoAssembly *m_PineAssembly = nullptr;
    MonoImage *m_PineImage = nullptr;

    MonoClass *m_EntityClass = nullptr;
    MonoClassField *m_EntityInternalIdField = nullptr;
    MonoClassField *m_EntityIdProperty = nullptr;

    MonoClass *m_ComponentClass = nullptr;
    MonoClassField *m_ComponentInternalIdField = nullptr;
    MonoClassField *m_ComponentParentField = nullptr;
    MonoClassField *m_ComponentTypeField = nullptr;
}

void Pine::Script::ObjectFactory::Setup()
{
    m_Domain = Pine::Script::Runtime::GetDomain();
    m_PineAssembly = Pine::Script::Runtime::GetPineAssembly();
    m_PineImage = Pine::Script::Runtime::GetPineImage();

    m_EntityClass = mono_class_from_name(m_PineImage, "Pine.World", "Entity");
    m_EntityInternalIdField = mono_class_get_field_from_name(m_EntityClass, "_internalId");
    m_EntityIdProperty = mono_class_get_field_from_name(m_EntityClass, "Id");

    m_ComponentClass = mono_class_from_name(m_PineImage, "Pine.World", "Component");
    m_ComponentInternalIdField = mono_class_get_field_from_name(m_ComponentClass, "_internalId");
    m_ComponentParentField = mono_class_get_field_from_name(m_ComponentClass, "Parent");
    m_ComponentTypeField = mono_class_get_field_from_name(m_ComponentClass, "ComponentType");

    assert(m_EntityClass);
    assert(m_EntityInternalIdField);
    assert(m_EntityIdProperty);

    assert(m_ComponentClass);
    assert(m_ComponentInternalIdField);
    assert(m_ComponentParentField);
    assert(m_ComponentTypeField);
}

Pine::Script::ObjectHandle Pine::Script::ObjectFactory::CreateEntity(std::uint32_t entityId, std::uint32_t internalId)
{
    auto entity = mono_object_new(m_Domain, m_EntityClass);

    mono_runtime_object_init(entity);

    mono_field_set_value(entity, m_EntityInternalIdField, &internalId);
    mono_field_set_value(entity, m_EntityIdProperty, &entityId);

    auto handle = mono_gchandle_new(entity, true);

    return {entity, handle};
}

void Pine::Script::ObjectFactory::DestroyEntity(ObjectHandle *entity)
{
    entity->Object = nullptr;

    mono_gchandle_free(entity->Handle);
}

Pine::Script::ObjectHandle Pine::Script::ObjectFactory::CreateComponent(Pine::IComponent *engineComponent)
{
    auto component = mono_object_new(m_Domain, m_ComponentClass);

    mono_runtime_object_init(component);

    auto internalId = engineComponent->GetInternalId();
    auto type = static_cast<int>(engineComponent->GetType());

    mono_field_set_value(component, m_ComponentInternalIdField, &internalId);
    mono_field_set_value(component, m_ComponentTypeField, &type);
    mono_field_set_value(component, m_ComponentParentField, (void *) mono_gchandle_get_target(engineComponent->GetParent()->GetScriptHandle()->Handle));

    auto handle = mono_gchandle_new(component, true);

    return {component, handle};
}

void Pine::Script::ObjectFactory::DestroyComponent(Pine::Script::ObjectHandle *component)
{
    component->Object = nullptr;

    mono_gchandle_free(component->Handle);
}

Pine::Script::ObjectHandle Pine::Script::ObjectFactory::CreateScriptObject(Pine::CSharpScript *script, Pine::IComponent *component)
{
    auto data = script->GetScriptData();
    if (!data || !data->IsReady)
    {
        Log::Warning("Failed to create script object, script data is not ready.");
        return {};
    }

    auto object = mono_object_new(m_Domain, data->Class);

    auto internalId = component->GetInternalId();
    auto type = static_cast<int>(component->GetType());

    mono_runtime_object_init(object);
    mono_field_set_value(object, data->ComponentParentField, (void *) mono_gchandle_get_target(component->GetParent()->GetScriptHandle()->Handle));
    mono_field_set_value(object, data->ComponentInternalIdField, &internalId);
    mono_field_set_value(object, data->ComponentTypeField, &type);

    auto handle = mono_gchandle_new(object, true);

    return {object, handle};
}

void Pine::Script::ObjectFactory::DestroyScriptObject(ObjectHandle *component)
{
    component->Object = nullptr;

    mono_gchandle_free(component->Handle);
}
