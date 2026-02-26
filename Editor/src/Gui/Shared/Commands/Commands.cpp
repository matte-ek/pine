#include "Commands.hpp"
#include <GLFW/glfw3.h>
#include "Gui/Panels/AssetBrowser/AssetBrowserPanel.hpp"
#include "Gui/Shared/KeybindSystem/KeybindSystem.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Other/Actions/Actions.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/World/World.hpp"
#include "Utilities/Assets/AssetUtilities.hpp"
#include "Utilities/Scripts/ScriptUtilities.hpp"

namespace
{
    std::vector<Pine::Entity*> ClipboardEntities;
    std::vector<Pine::Asset*> ClipboardAssets;

    namespace Keybinds
    {
        std::uint32_t Copy;
        std::uint32_t Paste;
        std::uint32_t Duplicate;
        std::uint32_t Delete;
        std::uint32_t Undo;
        std::uint32_t Redo;
        std::uint32_t Refresh;
        std::uint32_t Save;
    }
}

void Editor::Commands::Setup()
{
    Keybinds::Copy = KeybindSystem::RegisterKeybind("Copy", GLFW_KEY_C, true);
    Keybinds::Paste = KeybindSystem::RegisterKeybind("Paste", GLFW_KEY_V, true);
    Keybinds::Duplicate = KeybindSystem::RegisterKeybind("Duplicate", GLFW_KEY_D, true);
    Keybinds::Delete = KeybindSystem::RegisterKeybind("Delete", GLFW_KEY_DELETE);
    Keybinds::Undo = KeybindSystem::RegisterKeybind("Undo", GLFW_KEY_Z, true);
    Keybinds::Redo = KeybindSystem::RegisterKeybind("Redo", GLFW_KEY_Y, true);
    Keybinds::Refresh = KeybindSystem::RegisterKeybind("Refresh", GLFW_KEY_F5);
    Keybinds::Save = KeybindSystem::RegisterKeybind("Save", GLFW_KEY_S, true);
}

void Editor::Commands::Dispose()
{
}

void Editor::Commands::Copy()
{
    ClipboardEntities = Selection::GetSelectedEntities();
    ClipboardAssets = Selection::GetSelectedAssets();
}

void Editor::Commands::Paste()
{
    Selection::Clear();

    for (auto& entity : ClipboardEntities)
    {
        Pine::Blueprint blueprint;

        blueprint.CreateFromEntity(entity);
        blueprint.GetEntity()->SetName(blueprint.GetEntity()->GetName());

        auto spawnedEntity = blueprint.Spawn();

        if (entity->GetParent() != nullptr)
        {
            entity->GetParent()->AddChild(spawnedEntity);
        }

        blueprint.Dispose();

        Selection::Add(spawnedEntity);
    }
}

void Editor::Commands::Duplicate()
{
    Copy();
    Paste();
}

void Editor::Commands::Delete()
{
    if (!Selection::GetSelectedEntities().empty())
    {
        for (auto entity : Selection::GetSelectedEntities())
        {
            entity->Delete();
        }
    }
    else if (!Selection::GetSelectedAssets().empty())
    {
        for (auto asset : Selection::GetSelectedAssets())
        {
            if (asset->GetType() == Pine::AssetType::CSharpScript)
            {
                Utilities::Script::DeleteScript(asset->GetFilePath().string());
            }

            std::filesystem::remove(asset->GetFilePath());
        }

        Refresh(true);
    }

    Selection::Clear();
}

void Editor::Commands::Undo()
{
    Actions::Undo();
}

void Editor::Commands::Redo()
{
}

void Editor::Commands::Refresh(bool engineAssets)
{
    Utilities::Asset::RefreshAll();
    Panels::AssetBrowser::BuildAssetHierarchy();
}

void Editor::Commands::Save()
{
    if (Pine::World::GetActiveLevel())
    {
        Pine::World::GetActiveLevel()->CreateFromWorld();
        Pine::World::GetActiveLevel()->MarkAsModified();
    }

    Utilities::Asset::SaveAll();
}

void Editor::Commands::Update()
{
    if (KeybindSystem::IsKeybindPressed(Keybinds::Copy))
        Copy();
    if (KeybindSystem::IsKeybindPressed(Keybinds::Paste))
        Paste();
    if (KeybindSystem::IsKeybindPressed(Keybinds::Duplicate))
        Duplicate();
    if (KeybindSystem::IsKeybindPressed(Keybinds::Delete))
        Delete();
    if (KeybindSystem::IsKeybindPressed(Keybinds::Undo))
        Undo();
    if (KeybindSystem::IsKeybindPressed(Keybinds::Redo))
        Redo();
    if (KeybindSystem::IsKeybindPressed(Keybinds::Refresh))
        Refresh();
    if (KeybindSystem::IsKeybindPressed(Keybinds::Save))
        Save();
}