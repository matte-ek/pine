#pragma once

#include "Pine/Assets/CSharpScript/CSharpScript.hpp"

#include <vector>

namespace Pine
{
    class ScriptField;

    // Everything the engine knows about one C# script class, rebuilt from scratch on every
    // game-assembly load. Owns its fields.
    struct ScriptData
    {
        CSharpScript* Asset = nullptr;

        // Where the game assembly's class registry keeps the managed class, or -1 when it could
        // not be resolved. The engine holds nothing of the class itself, so that a reload can
        // unload the assembly it lives in - see Script/GameAssembly/GameAssembly.hpp.
        //
        // This is not the id a ScriptField holds; that one belongs to the field registry.
        int ClassId = -1;

        // Which lifecycle methods the class has. All of them are optional, so a component whose
        // class does not have the one about to be dispatched is skipped rather than called.
        bool HasOnStart = false;
        bool HasOnUpdate = false;

        // The fields of this script the editor shows and the engine stores
        std::vector<ScriptField*> Fields;

        bool IsReady = false;

        ~ScriptData();
    };
}
