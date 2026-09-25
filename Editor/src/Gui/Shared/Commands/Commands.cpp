#include "Commands.hpp"

#include "imgui.h"
#include "Gui/Panels/AssetBrowser/AssetBrowserPanel.hpp"
#include "Gui/Shared/KeybindSystem/KeybindSystem.hpp"
#include "Gui/Shared/Selection/Selection.hpp"
#include "Other/Actions/Actions.hpp"
#include "Other/Clipboard/AssetClipboard/AssetClipboard.hpp"
#include "Other/Clipboard/EntityClipboard/EntityClipboard.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/World/World.hpp"
#include "Utilities/Assets/AssetUtilities.hpp"
#include "Utilities/Entity/EntityUtilities.hpp"
#include "Utilities/Scripts/ScriptUtilities.hpp"

namespace
{
    // Selects what a paste or duplicate made, leaving the selection alone when that was nothing.
    void SelectEntities(const std::vector<Pine::Entity*>& entities)
    {
        if (entities.empty())
        {
            return;
        }

        Selection::Clear();

        for (const auto entity : entities)
        {
            Selection::Add(entity);
        }
    }

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
    Keybinds::Copy = KeybindSystem::RegisterKeybind("Copy", ImGuiKey_C, true);
    Keybinds::Paste = KeybindSystem::RegisterKeybind("Paste", ImGuiKey_V, true);
    Keybinds::Duplicate = KeybindSystem::RegisterKeybind("Duplicate", ImGuiKey_D, true);
    Keybinds::Delete = KeybindSystem::RegisterKeybind("Delete", ImGuiKey_Delete);
    Keybinds::Undo = KeybindSystem::RegisterKeybind("Undo", ImGuiKey_Z, true);
    Keybinds::Redo = KeybindSystem::RegisterKeybind("Redo", ImGuiKey_Y, true);
    Keybinds::Refresh = KeybindSystem::RegisterKeybind("Refresh", ImGuiKey_F5);
    Keybinds::Save = KeybindSystem::RegisterKeybind("Save", ImGuiKey_S, true);
}

void Editor::Commands::Dispose()
{
}

void Editor::Commands::Copy()
{
    Clipboard::Entity::Copy(Selection::GetSelectedEntities());
    Clipboard::Asset::Copy(Selection::GetSelectedAssets());
}

void Editor::Commands::Paste()
{
    SelectEntities(Clipboard::Entity::Paste());
}

void Editor::Commands::Duplicate()
{
    SelectEntities(Clipboard::Entity::Duplicate(Selection::GetSelectedEntities()));
}

void Editor::Commands::Delete()
{
    if (!Selection::GetSelectedEntities().empty())
    {
        // A copy of the selection, which DeleteHierarchy edits as it goes.
        const auto entities = Utilities::Entity::GetTopmost(Selection::GetSelectedEntities());

        // Captured before deleting, so undo can bring the entities back.
        auto command = std::make_unique<Actions::CreateDeleteEntityCommand>(entities, Actions::CommandType::Delete);

        for (const auto entity : entities)
        {
            Utilities::Entity::DeleteHierarchy(entity);
        }

        Actions::RegisterCommand(std::move(command));
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
    Actions::ExecuteUndo();
}

void Editor::Commands::Redo()
{
    Actions::ExecuteRedo();
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