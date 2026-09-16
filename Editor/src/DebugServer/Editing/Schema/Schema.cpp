#include "Schema.hpp"

#include "../Components/Components.hpp"

namespace
{
    using nlohmann::json;

    json Object(const json& required, const json& fields)
    {
        return {
            { "type", "object" }, { "required", required },
            { "additionalFields", false }, { "fields", fields }
        };
    }

    json Operation(const char* name, const json& required, json fields)
    {
        fields["op"] = { { "type", "string" }, { "const", name } };
        return Object(required, fields);
    }

    const json NonemptyString = {
        { "type", "string" }, { "minLength", 1 }, { "allowNullCharacters", false }
    };
}

nlohmann::json Editor::DebugServer::Editing::Schema::Request(const std::size_t maxOperations)
{
    return Object({ "version", "operations" }, {
        { "version", { { "type", "integer" }, { "const", 1 } } },
        { "operations", {
            { "type", "array" }, { "minItems", 1 }, { "maxItems", maxOperations },
            { "items", { { "type", "operation" }, { "selectedBy", "op" } } }
        } }
    });
}

nlohmann::json Editor::DebugServer::Editing::Schema::Operations()
{
    json componentTypes = json::array();
    json addableTypes = json::array();
    for (const auto adapter : Components::GetAdapters())
    {
        componentTypes.push_back(adapter->Name);
        if (adapter->AllowAddRemove)
        {
            addableTypes.push_back(adapter->Name);
        }
    }

    const json entityTarget = {
        { "type", "reference" }, { "kind", "entity" }, { "forms", { "id" } }
    };
    const json componentTarget = {
        { "type", "reference" }, { "kind", "component" }, { "forms", { "id" } }
    };
    const json parent = {
        { "type", "reference" }, { "kind", "entity" }, { "forms", { "id", "ref" } }
    };
    json creationParent = parent;
    creationParent["whenOmitted"] = "sceneRoot";
    json reparentParent = parent;
    reparentParent["nullable"] = true;
    reparentParent["whenNull"] = "sceneRoot";

    json rootRef = NonemptyString;
    rootRef["role"] = "declareBatchRef";
    rootRef["whenOmitted"] = "noBatchRef";
    json creationName = NonemptyString;
    creationName["default"] = "Entity";

    const json initialProperties = {
        { "type", "componentProperties" }, { "selectedBy", "type" },
        { "whenOmitted", "defaults" }, { "omittedProperties", "defaults" }
    };
    const json componentEntry = Object({ "type" }, {
        { "type", { { "type", "enum" }, { "values", componentTypes } } },
        { "properties", initialProperties }
    });

    return {
        { "entity.create", Operation("entity.create", { "op" }, {
            { "name", creationName }, { "ref", rootRef }, { "parent", creationParent },
            { "components", {
                { "type", "array" }, { "items", componentEntry }, { "uniqueBy", "type" },
                { "whenOmitted", "requiredTransformOnly" },
                { "description", "Transform configures the entity's required Transform; other entries add components." }
            } }
        }) },
        { "entity.update", Operation("entity.update", { "op", "target", "properties" }, {
            { "target", entityTarget },
            { "properties", {
                { "type", "entityProperties" }, { "omittedProperties", "retain" }
            } }
        }) },
        { "entity.reparent", Operation("entity.reparent", { "op", "target", "parent" }, {
            { "target", entityTarget }, { "parent", reparentParent }
        }) },
        { "entity.delete", Operation("entity.delete", { "op", "target" }, {
            { "target", entityTarget }
        }) },
        { "entity.duplicate", Operation("entity.duplicate", { "op", "target" }, {
            { "target", entityTarget }, { "ref", rootRef }
        }) },
        { "component.add", Operation("component.add", { "op", "target", "type" }, {
            { "target", entityTarget },
            { "type", { { "type", "enum" }, { "values", addableTypes } } },
            { "properties", initialProperties }
        }) },
        { "component.update", Operation("component.update", { "op", "target", "properties" }, {
            { "target", componentTarget },
            { "properties", {
                { "type", "componentProperties" }, { "selectedBy", "targetComponentType" },
                { "omittedProperties", "retain" }
            } }
        }) },
        { "component.remove", Operation("component.remove", { "op", "target" }, {
            { "target", componentTarget }
        }) }
    };
}

nlohmann::json Editor::DebugServer::Editing::Schema::References()
{
    json id = NonemptyString;
    id["format"] = "pineUid";
    id["pattern"] = "^[0-9a-fA-F]{1,16}-[0-9a-fA-F]{16}$";
    id["description"] = "Persistent Pine UId; the hexadecimal group before the dash must be nonzero.";

    return {
        { "exactlyOneForm", true }, { "additionalFields", false },
        { "forms", { { "id", id }, { "ref", NonemptyString }, { "path", NonemptyString } } },
        { "entity", {
            { "scope", "currentScene" }, { "excludeTemporaryHierarchies", true },
            { "idMustExistBeforeBatch", true }, { "removedEarlierInBatch", "reject" }
        } },
        { "component", {
            { "scope", "currentScene" }, { "excludeTemporaryHierarchies", true },
            { "idMustExistBeforeBatch", true }, { "removedEarlierInBatch", "reject" },
            { "description", "Use a component UId, not its entity ID or pool index. Update requires an adapter; removal also requires removable=true." }
        } },
        { "asset", {
            { "forms", { "id", "path" } }, { "mustBeLoaded", true },
            { "typeSelectedBy", "assetType" }, { "responseForm", "id" },
            { "description", "Paths are virtual asset paths. Null clears only properties advertising nullable=true." }
        } },
        { "batchRefs", {
            { "scope", "request" }, { "unique", true }, { "earlierOperationsOnly", true },
            { "declaredBy", { "entity.create", "entity.duplicate" } },
            { "declarationField", "ref" }, { "refersTo", "operationRoot" },
            { "acceptedBy", { "entity.create.parent", "entity.reparent.parent" } },
            { "deletedEarlierInBatch", "reject" },
            { "description", "Read allocated IDs from results for use as targets in a subsequent request." }
        } }
    };
}
