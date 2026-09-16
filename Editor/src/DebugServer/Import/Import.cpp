#include "Import.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>

#include "../Editing/Values/Values.hpp"
#include "Gui/Dialogs/AssetImport/AssetImportDialog.hpp"
#include "Gui/Panels/AssetBrowser/AssetBrowserPanel.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Projects/Projects.hpp"
#include "Utilities/Assets/AssetUtilities.hpp"
#include "Pine/Assets/Assets.hpp"

namespace
{
    using namespace Editor::DebugServer;
    using nlohmann::json;
    namespace Values = Editing::Values;
    namespace fs = std::filesystem;

    using ImportContext = std::unique_ptr<Pine::Importer::ImportContext,
        decltype(&Pine::Importer::DeleteContext)>;

    fs::path DestinationDirectory(const std::string& directory)
    {
        const fs::path relative(directory);
        Values::Require(!relative.is_absolute() && directory.find('\\') == std::string::npos &&
            relative.generic_string() == directory, "/directory", "Expected a project-relative directory.");
        for (const auto& part : relative)
        {
            Values::Require(part != "." && part != ".." && !part.empty(),
                "/directory", "Invalid directory component.");
        }
        Values::Require(std::none_of(directory.begin(), directory.end(), [](unsigned char c)
            { return c >= 'A' && c <= 'Z'; }), "/directory", "Use a lowercase asset directory.");

        // Keep the project-relative spelling: the importer uses the working-directory prefix
        // to derive virtual asset paths and names for the content copies.
        auto destination = fs::path(Editor::Projects::GetProjectPath()) / "assets";
        for (const auto& part : relative)
        {
            destination /= part;
            Values::Require(!fs::is_symlink(destination), "/directory", "Destination must not traverse symbolic links.");
            Values::Require(!fs::exists(destination) || fs::is_directory(destination),
                "/directory", "Destination must be a directory.");
        }
        return destination;
    }

    const char* ActionName(Pine::AssetImportAction action)
    {
        switch (action)
        {
            case Pine::AssetImportAction::Create: return "create";
            case Pine::AssetImportAction::Update: return "update";
            case Pine::AssetImportAction::Unsupported: return "unsupported";
            case Pine::AssetImportAction::Conflict: return "conflict";
            default: return "undetermined";
        }
    }

    json Results(const Pine::Importer::ImportContext& context)
    {
        auto results = json::array();
        for (const auto& entry : context.Imports)
        {
            const bool imported = entry->ImportStatus == Pine::AssetImportStatus::Imported;
            json result = {
                { "source", entry->SourcePaths.front().string() },
                { "action", ActionName(entry->Action) },
                { "status", imported ? "imported" : "failed" },
                { "type", Pine::AssetTypeToString(entry->Type) }
            };
            if (imported && entry->AssetPtr != nullptr)
            {
                result["id"] = entry->AssetPtr->GetUId().ToString();
                result["path"] = entry->AssetPtr->GetPath();
                result["file"] = entry->AssetPtr->GetFilePath().string();
                result["sources"] = json::array();
                for (const auto& source : entry->AssetPtr->GetSources())
                {
                    result["sources"].push_back(source.FilePath);
                }
            }
            results.push_back(std::move(result));
        }
        return results;
    }
}

Editor::DebugServer::Response Editor::DebugServer::Import::Execute(const Request& request)
{
    if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
    {
        return Error(409, "Stop play mode before importing an asset.");
    }
    if (Gui::Dialog::AssetImport::IsPending())
    {
        return Error(409, "Finish or cancel the editor's import dialog first.");
    }

    ImportContext context(nullptr, Pine::Importer::DeleteContext);
    try
    {
        Values::Require(request.Parameters.empty(), "", "Import does not accept query parameters.");
        Values::Require(request.Body.size() <= 16 * 1024, "", "Import request exceeds 16 KiB.");
        const auto depthLimit = [](int depth, json::parse_event_t, json&)
        {
            Values::Require(depth <= 8, "", "JSON nesting exceeds 8 levels.");
            return true;
        };
        const auto body = json::parse(request.Body, depthLimit, false);
        Values::Object(body, "", { "source", "directory", "overwrite" }, { "source" });
        const auto source = fs::path(Values::String(body.at("source"), "/source"));
        Values::Require(!source.empty() && fs::is_regular_file(source), "/source", "Expected an existing source file.");
        Values::Require(source.filename().string().find('\\') == std::string::npos,
            "/source", "Source filename must not contain a backslash.");
        Values::Require(!body.contains("directory") || body.at("directory").is_string(),
            "/directory", "Expected a string.");
        const auto directory = body.value("directory", std::string());
        Values::Require(directory.find('\0') == std::string::npos, "/directory", "Directory must not contain null characters.");
        const auto destination = DestinationDirectory(directory);
        Values::Require(!body.contains("overwrite") || body.at("overwrite").is_boolean(),
            "/overwrite", "Expected a boolean.");

        context.reset(Utilities::Asset::CreateImportContext({ fs::absolute(source).string() }, destination));
        Pine::Importer::Resolve(context.get());
        const auto& entry = *context->Imports.front();
        if (entry.Action == Pine::AssetImportAction::Unsupported)
        {
            return Error(400, "No importer is registered for this source file type.");
        }
        if (entry.Action == Pine::AssetImportAction::Conflict)
        {
            return Error(409, "Destination conflicts with an unloaded asset or a different asset type.");
        }
        if (entry.Action == Pine::AssetImportAction::Update && !body.value("overwrite", false))
        {
            return Error(409, "Destination exists; set overwrite to true to re-import it, preserving its ID.");
        }

        const auto expectedFile = fs::path(entry.ResolvedEnginePath).replace_extension(".passet");
        Values::Require(!fs::is_symlink(expectedFile), "/directory", "Destination must not be a symbolic link.");
        Values::Require(!fs::exists(expectedFile) || fs::is_regular_file(expectedFile),
            "/directory", "Asset destination must be a regular file.");
        Values::Require(fs::weakly_canonical(entry.AssetPtr->GetFilePath()) == fs::weakly_canonical(expectedFile),
            "/directory", "Destination conflicts with an asset outside this directory.");
    }
    catch (const Values::ValidationError& exception)
    {
        return { 400, { { "error", exception.what() }, { "path", exception.Path }, { "phase", "validation" } } };
    }
    catch (const std::exception& exception)
    {
        return Error(400, exception.what());
    }

    try
    {
        // Execute writes each .passet, reloads it on the main thread, and registers new assets.
        // Refresh the browser directly; a full project reload is unnecessary here.
        Pine::Importer::Execute(context.get());
        Panels::AssetBrowser::BuildAssetHierarchy();
        const auto results = Results(*context);
        const bool failed = std::any_of(context->Imports.begin(), context->Imports.end(), [](const auto& entry)
            { return entry->ImportStatus != Pine::AssetImportStatus::Imported; });
        if (failed)
        {
            return { 500, { { "error", "One or more imports failed. Inspect imports and /logs for details." },
                { "imports", results }, { "failedOperationMayHaveChangedState", true } } };
        }
        return { 200, { { "imports", results } } };
    }
    catch (const std::exception& exception)
    {
        return { 500, { { "error", exception.what() }, { "imports", Results(*context) },
            { "failedOperationMayHaveChangedState", true } } };
    }
}
