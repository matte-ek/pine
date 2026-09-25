#include "Actions.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "imgui.h"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Utilities/Entity/EntityUtilities.hpp"

#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    using namespace Editor::Actions;

    constexpr std::size_t MaxHistorySize = 128;

    bool m_ItemUpdated = false;

    bool m_IsSavingHeldState = false;
    std::unique_ptr<EditorCommand> m_HeldStateCommand;
    std::uint64_t m_HistoryGeneration = 0;

    // Standard undo stack: index 0 is the oldest command, the back is the newest. m_CommandIndex is the number
    // of commands currently "applied" - the next undo targets m_CommandHistory[m_CommandIndex - 1], the next
    // redo targets m_CommandHistory[m_CommandIndex]. Ownership lives in the unique_ptrs, so trimming the history
    // frees the commands automatically.
    std::size_t m_CommandIndex = 0;
    std::vector<std::unique_ptr<EditorCommand>> m_CommandHistory;

    void SynchronizeScene()
    {
        if (m_HistoryGeneration != Pine::Entities::GetSceneGeneration())
        {
            ClearHistory();
        }
    }

    void AppendCommand(std::unique_ptr<EditorCommand> editorCommand)
    {
        // Anything that was undone can no longer be redone once new history is written, so discard it.
        if (m_CommandIndex < m_CommandHistory.size())
        {
            PVerbose(fmt::format("Re-writing history, discarding {} redoable command(s)", m_CommandHistory.size() - m_CommandIndex));

            m_CommandHistory.erase(m_CommandHistory.begin() + m_CommandIndex, m_CommandHistory.end());
        }

        m_CommandHistory.push_back(std::move(editorCommand));
        m_CommandIndex = m_CommandHistory.size();

        if (m_CommandHistory.size() > MaxHistorySize)
        {
            m_CommandHistory.erase(m_CommandHistory.begin());
            m_CommandIndex--;
        }

        PVerbose(fmt::format("Writing history, index: {}, size: {}", m_CommandIndex, m_CommandHistory.size()));
    }
}

void EditorCommand::SaveState(CommandState commandState)
{
}

UpdateComponentCommand::UpdateComponentCommand(const Pine::Component* component, const bool savePreState)
{
    m_ComponentId = component->GetId();
    m_ComponentType = component->GetType();

    if (savePreState)
    {
        UpdateComponentCommand::SaveState(CommandState::PreCommand);
    }
}

void UpdateComponentCommand::SaveState(const CommandState commandState)
{
    const auto component = Pine::Components::FindById(m_ComponentType, m_ComponentId);

    if (component == nullptr)
    {
        PWarning(fmt::format("UpdateComponentCommand::SaveState: component {} no longer exists, skipping.", m_ComponentId.ToString()));
        return;
    }

    if (commandState == CommandState::PreCommand)
    {
        m_PreCommand = component->SaveData();
        m_PreActive = component->GetActive();
    }
    else
    {
        m_PostCommand = component->SaveData();
        m_PostActive = component->GetActive();
    }
}

void UpdateComponentCommand::Apply(const CommandState commandState)
{
    const auto component = Pine::Components::FindById(m_ComponentType, m_ComponentId);

    if (component == nullptr)
    {
        PWarning(fmt::format("UpdateComponentCommand::Apply: component {} no longer exists, skipping.", m_ComponentId.ToString()));
        return;
    }

    const bool isPreCommand = commandState == CommandState::PreCommand;

    component->LoadData(isPreCommand ? m_PreCommand : m_PostCommand);
    component->SetActive(isPreCommand ? m_PreActive : m_PostActive);
}

CreateDeleteComponentCommand::CreateDeleteComponentCommand(Pine::Component* component, const CommandType type)
    : m_CommandType(type)
{
    m_ComponentId = component->GetId();
    m_ParentId = component->GetParent()->GetId();

    m_ComponentType = component->GetType();
    m_ComponentData = component->SaveData();
    m_ComponentActive = component->GetActive();
}

void CreateDeleteComponentCommand::Apply(const CommandState commandState)
{
    if ((commandState == CommandState::PreCommand && m_CommandType == CommandType::Delete) ||
        (commandState == CommandState::PostCommand && m_CommandType == CommandType::Create))
    {
        const auto entity = Pine::Entities::Find(m_ParentId);

        if (entity == nullptr)
        {
            PWarning(fmt::format("CreateDeleteComponentCommand::Apply: parent entity {} no longer exists, skipping.", m_ParentId.ToString()));
            return;
        }

        const auto component = entity->AddComponent(m_ComponentType);

        component->SetId(m_ComponentId);
        component->LoadData(m_ComponentData);
        component->SetActive(m_ComponentActive);
    }
    else if ((commandState == CommandState::PostCommand && m_CommandType == CommandType::Delete) ||
             (commandState == CommandState::PreCommand && m_CommandType == CommandType::Create))
    {
        const auto component = Pine::Components::FindById(m_ComponentType, m_ComponentId);

        if (component == nullptr)
        {
            PWarning(fmt::format("CreateDeleteComponentCommand::Apply: component {} no longer exists, skipping.", m_ComponentId.ToString()));
            return;
        }

        component->GetParent()->RemoveComponent(component);
    }
}

namespace
{
    std::size_t IndexInEntityList(const Pine::Entity* entity)
    {
        const auto& list = Pine::Entities::GetList();

        return std::find(list.begin(), list.end(), entity) - list.begin();
    }

    std::size_t IndexAmongSiblings(const Pine::Entity* entity)
    {
        if (entity->GetParent() == nullptr)
        {
            return 0;
        }

        const auto& siblings = entity->GetParent()->GetChildren();

        return std::find(siblings.begin(), siblings.end(), entity) - siblings.begin();
    }

    EntitySnapshot CaptureEntity(const Pine::Entity* entity)
    {
        EntitySnapshot snapshot;

        snapshot.Id = entity->GetId();
        snapshot.Name = entity->GetName();
        snapshot.Active = entity->GetActive();
        snapshot.Static = entity->GetStatic();
        snapshot.Tags = entity->GetTags();

        for (const auto component : entity->GetComponents())
        {
            EntitySnapshot::ComponentState state;

            state.Type = component->GetType();
            state.Id = component->GetId();
            state.Data = component->SaveData();
            state.Active = component->GetActive();

            snapshot.Components.push_back(std::move(state));
        }

        for (const auto child : entity->GetChildren())
        {
            snapshot.Children.push_back(CaptureEntity(child));
        }

        if (const auto parent = entity->GetParent())
        {
            snapshot.ParentId = parent->GetId();
        }

        snapshot.SiblingIndex = IndexAmongSiblings(entity);
        snapshot.ListIndex = IndexInEntityList(entity);

        return snapshot;
    }

    // Throws if restoring the snapshots could stop partway, because an id is taken or a pool is out
    // of room, so that nothing is created unless all of it can be.
    void CheckCanRestore(const std::vector<EntitySnapshot>& snapshots)
    {
        std::uint32_t entityCount = 0;
        std::map<Pine::ComponentType, std::uint32_t> componentCounts;

        std::vector<const EntitySnapshot*> pending;

        for (const auto& snapshot : snapshots)
        {
            pending.push_back(&snapshot);
        }

        while (!pending.empty())
        {
            const auto snapshot = pending.back();
            pending.pop_back();

            if (Pine::Entities::Find(snapshot->Id) != nullptr)
            {
                throw std::runtime_error(fmt::format("Cannot restore entity '{}', its id is already in use.", snapshot->Name));
            }

            entityCount++;

            for (const auto& component : snapshot->Components)
            {
                componentCounts[component.Type]++;
            }

            for (const auto& child : snapshot->Children)
            {
                pending.push_back(&child);
            }
        }

        if (entityCount > Pine::Entities::GetFreeSlotCount())
        {
            throw std::runtime_error("Cannot restore entities, the entity pool is full.");
        }

        for (const auto& [type, count] : componentCounts)
        {
            if (count > Pine::Components::GetFreeSlotCount(type))
            {
                throw std::runtime_error(fmt::format("Cannot restore entities, the {} pool is full.", Pine::ComponentTypeToString(type)));
            }
        }
    }

    // Records every entity it creates with its saved list position, for RestoreEntities to apply once
    // all of them exist.
    Pine::Entity* RestoreEntity(const EntitySnapshot& snapshot, std::vector<std::pair<std::size_t, Pine::Entity*>>& listPlacements)
    {
        const auto entity = Pine::Entities::CreateWithId(snapshot.Id);

        entity->SetName(snapshot.Name);
        entity->SetActive(snapshot.Active);
        entity->SetStatic(snapshot.Static);
        entity->SetTags(snapshot.Tags);

        for (const auto& saved : snapshot.Components)
        {
            // The new entity already has a Transform, which takes over the saved one's id and data.
            Pine::Component* component = nullptr;

            if (saved.Type == Pine::ComponentType::Transform)
            {
                component = entity->GetTransform();
            }
            else
            {
                component = entity->AddComponent(saved.Type);
            }

            component->SetId(saved.Id);
            component->LoadData(saved.Data);
            component->SetActive(saved.Active);
        }

        listPlacements.emplace_back(snapshot.ListIndex, entity);

        for (const auto& child : snapshot.Children)
        {
            entity->AddChild(RestoreEntity(child, listPlacements));
        }

        return entity;
    }

    void InsertChild(Pine::Entity* parent, Pine::Entity* child, const std::size_t index)
    {
        parent->AddChild(child);

        // AddChild appends, so move the siblings that belong after the child back behind it.
        const auto children = parent->GetChildren();

        for (std::size_t i = index; i + 1 < children.size(); i++)
        {
            parent->RemoveChild(children[i]);
            parent->AddChild(children[i]);
        }
    }

    void RestoreEntities(const std::vector<EntitySnapshot>& snapshots)
    {
        CheckCanRestore(snapshots);

        // In ascending sibling order, so that inserting one never shifts another already in place.
        std::vector<const EntitySnapshot*> ordered;

        for (const auto& snapshot : snapshots)
        {
            ordered.push_back(&snapshot);
        }

        std::stable_sort(ordered.begin(), ordered.end(), [](const EntitySnapshot* a, const EntitySnapshot* b)
        {
            return a->SiblingIndex < b->SiblingIndex;
        });

        std::vector<std::pair<std::size_t, Pine::Entity*>> listPlacements;

        for (const auto snapshot : ordered)
        {
            const auto entity = RestoreEntity(*snapshot, listPlacements);

            if (!snapshot->ParentId.IsValid())
            {
                continue;
            }

            const auto parent = Pine::Entities::Find(snapshot->ParentId);

            if (parent == nullptr)
            {
                PWarning(fmt::format("CreateDeleteEntityCommand: the parent of '{}' no longer exists, restoring it at the root.", snapshot->Name));
                continue;
            }

            InsertChild(parent, entity, snapshot->SiblingIndex);
        }

        // Also ascending, for the same reason as above.
        std::stable_sort(listPlacements.begin(), listPlacements.end(), [](const auto& a, const auto& b)
        {
            return a.first < b.first;
        });

        for (const auto& [index, entity] : listPlacements)
        {
            const auto lastIndex = Pine::Entities::GetList().size() - 1;

            Pine::Entities::MoveEntity(entity, std::min(index, lastIndex));
        }
    }

    void DeleteEntities(const std::vector<EntitySnapshot>& snapshots)
    {
        for (const auto& snapshot : snapshots)
        {
            const auto entity = Pine::Entities::Find(snapshot.Id);

            if (entity == nullptr)
            {
                PWarning(fmt::format("CreateDeleteEntityCommand: entity '{}' no longer exists, skipping.", snapshot.Name));
                continue;
            }

            Editor::Utilities::Entity::DeleteHierarchy(entity);
        }
    }
}

CreateDeleteEntityCommand::CreateDeleteEntityCommand(const std::vector<Pine::Entity*>& entities, const CommandType type)
    : m_CommandType(type)
{
    for (const auto entity : Editor::Utilities::Entity::GetTopmost(entities))
    {
        m_Entities.push_back(CaptureEntity(entity));
    }
}

void CreateDeleteEntityCommand::Apply(const CommandState commandState)
{
    const bool restore = (commandState == CommandState::PreCommand && m_CommandType == CommandType::Delete) ||
                         (commandState == CommandState::PostCommand && m_CommandType == CommandType::Create);

    if (restore)
    {
        RestoreEntities(m_Entities);
    }
    else
    {
        DeleteEntities(m_Entities);
    }
}

CreateComponentCommand::CreateComponentCommand(Pine::Component* component, CommandType type, bool isDragEvent)
{
    auto CreateCommand = [component, type]() -> EditorCommand*
    {
        switch (type)
        {
            case CommandType::Update:
                return new UpdateComponentCommand(component);
            case CommandType::Create:
            case CommandType::Delete:
                return new CreateDeleteComponentCommand(component, type);
            default:
                return nullptr;
        }
    };

    m_ItemUpdated = true;

    // For most controls, this code will get ran on mouse release (for example clicking buttons/checkboxes/dropdowns)
    // however for sliders, this code may get ran on every small step, and would therefore spam the action system with
    // mini-changes while the user would only want to see the start and the finish of the operation. As such, we
    // add this code to detect this state and handle it accordingly.

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (!m_IsSavingHeldState)
        {
            m_IsSavingHeldState = true;
            m_HeldStateCommand.reset(CreateCommand());
        }

        return;
    }

    // This is really annoying but for some reason I can't understand ImGuizmo decides to continue sending commands
    // even after releasing the mouse button, so we have this "hack" in place to stop that from happening.
    if (isDragEvent)
    {
        return;
    }

    m_Command = CreateCommand();
}

CreateComponentCommand::~CreateComponentCommand()
{
    if (m_Command == nullptr)
    {
        return;
    }

    // The caller mutates the component between constructing this scope object and it going out of scope, so now
    // is the moment to snapshot the resulting ("post") state - otherwise redo would have nothing to restore.
    // (For create/delete commands SaveState is a no-op; their data is captured in the constructor.)
    m_Command->SaveState(CommandState::PostCommand);

    RegisterCommand(std::unique_ptr<EditorCommand>(m_Command));
}

bool Editor::Actions::HasItemUpdated()
{
    return m_ItemUpdated;
}

void Editor::Actions::ClearItemUpdated()
{
    m_ItemUpdated = false;
}

void Editor::Actions::ClearHistory()
{
    m_CommandHistory.clear();
    m_CommandIndex = 0;

    m_HeldStateCommand.reset();
    m_IsSavingHeldState = false;

    m_HistoryGeneration = Pine::Entities::GetSceneGeneration();
}

void Editor::Actions::FinishHeldCommand()
{
    SynchronizeScene();

    if (m_HeldStateCommand != nullptr)
    {
        m_HeldStateCommand->SaveState(CommandState::PostCommand);

        AppendCommand(std::move(m_HeldStateCommand));
    }

    m_IsSavingHeldState = false;
}

void Editor::Actions::RegisterCommand(std::unique_ptr<EditorCommand> command)
{
    FinishHeldCommand();
    AppendCommand(std::move(command));
}

Editor::Actions::HistoryState Editor::Actions::GetHistoryState()
{
    SynchronizeScene();
    return { m_CommandIndex, m_CommandHistory.size() - m_CommandIndex };
}

namespace
{
    HistoryResult ApplyHistory(const bool redo)
    {
        if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
        {
            PWarning("Stop play mode before undo or redo.");
            return { false, "Stop play mode before undo or redo." };
        }

        try
        {
            FinishHeldCommand();

            if (redo ? m_CommandIndex == m_CommandHistory.size() : m_CommandIndex == 0)
            {
                return {};
            }

            const auto index = redo ? m_CommandIndex : m_CommandIndex - 1;

            m_CommandHistory[index]->Apply(redo ? CommandState::PostCommand : CommandState::PreCommand);

            m_CommandIndex = redo ? index + 1 : index;

            return { true, {} };
        }
        catch (const std::exception& exception)
        {
            // Application may have stopped midway. Do not offer earlier commands against that state.
            const std::string error = exception.what();
            ClearHistory();
            PError(fmt::format("History restoration failed; history cleared: {}", error));
            return { false, error };
        }
    }
}

Editor::Actions::HistoryResult Editor::Actions::ExecuteUndo()
{
    return ApplyHistory(false);
}

Editor::Actions::HistoryResult Editor::Actions::ExecuteRedo()
{
    return ApplyHistory(true);
}

void Editor::Actions::Update()
{
    SynchronizeScene();

    if (m_IsSavingHeldState && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        FinishHeldCommand();
    }

    m_ItemUpdated = false;
}

void Editor::Actions::Setup()
{
    ClearHistory();
}

void Editor::Actions::Shutdown()
{
    ClearHistory();
}
