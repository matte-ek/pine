#include <GLFW/glfw3.h>
#include "Commands.hpp"
#include "Gui/Panels/AssetBrowser/AssetBrowserPanel.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Gui/Shared/KeybindSystem/KeybindSystem.hpp"
#include "Pine/World/World.hpp"
#include "Pine/Assets/Level/Level.hpp"

namespace
{
    std::vector<Pine::Entity*> ClipboardEntities;
    std::vector<Pine::IAsset*> ClipboardAssets;

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

void Commands::Setup()
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

void Commands::Dispose()
{
}

void Commands::Copy()
{
    ClipboardEntities = Selection::GetSelectedEntities();
    ClipboardAssets = Selection::GetSelectedAssets();
}

void Commands::Paste()
{
    for (auto& entity : ClipboardEntities)
    {
        Pine::Blueprint blueprint;

        blueprint.CreateFromEntity(entity);
        blueprint.GetEntity()->SetName(blueprint.GetEntity()->GetName() + " (Copy)");
        blueprint.Spawn();
        blueprint.Dispose();
    }
}

void Commands::Duplicate()
{
    Copy();
    Paste();
}

void Commands::Delete()
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
            std::filesystem::remove(asset->GetFilePath());
        }

        Refresh(true);
    }

    Selection::Clear();
}

void Commands::Undo()
{
}

void Commands::Redo()
{
}

void Commands::Refresh(bool engineAssets)
{
    if (engineAssets)
    {
        Pine::Assets::LoadDirectory("engine/shaders", false);
        Pine::Assets::LoadDirectory("engine", false);
    }

    Panels::AssetBrowser::RebuildAssetTree();
}

void Commands::Save()
{
    if (Pine::World::GetActiveLevel())
        Pine::World::GetActiveLevel()->CreateFromWorld();

    Pine::Assets::SaveAll();
}

void Commands::Update()
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