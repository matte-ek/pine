#pragma once

#include <mono/metadata/object-forward.h>
#include <mono/metadata/class.h>
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"

namespace Pine
{

    class ScriptField;

    struct ScriptData
    {
        CSharpScript* Asset = nullptr;
        MonoClass* Class = nullptr;

        // The available methods that the user may "override"
        MonoMethod* MethodOnStart = nullptr;
        MonoMethod* MethodOnDestroy = nullptr;
        MonoMethod* MethodOnUpdate = nullptr;
        MonoMethod* MethodOnRender = nullptr;

        // These are from the Component parent, but needs to be set
        // for the script as well.
        MonoClassField* ComponentParentField = nullptr;
        MonoClassField* ComponentInternalIdField = nullptr;
        MonoClassField* ComponentTypeField = nullptr;

        // All user made fields that are set to public
        std::vector<ScriptField*> Fields;

        bool IsReady = false;
    };

}