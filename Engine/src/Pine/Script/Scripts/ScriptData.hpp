#pragma once

#include <mono/metadata/object-forward.h>
#include <mono/metadata/class.h>
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"

namespace Pine
{

    struct ScriptData
    {
        CSharpScript* Asset = nullptr;
        MonoClass* Class = nullptr;

        MonoMethod* MethodOnStart = nullptr;
        MonoMethod* MethodOnDestroy = nullptr;
        MonoMethod* MethodOnUpdate = nullptr;
        MonoMethod* MethodOnRender = nullptr;

        MonoClassField* FieldEntity = nullptr;

        bool IsReady = false;
    };

}