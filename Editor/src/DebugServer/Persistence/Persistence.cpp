#include "Persistence.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <memory>

#include "../Editing/Values/Values.hpp"
#include "Gui/Panels/AssetBrowser/AssetBrowserPanel.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Projects/Projects.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/World/World.hpp"

namespace
{
    using namespace Editor::DebugServer;
    namespace Values = Editing::Values;
    namespace fs = std::filesystem;
    using nlohmann::json;

    struct DisposeLevel
    {
        void operator()(Pine::Level* level) const
        {
            level->Dispose();
            delete level;
        }
    };
    using OwnedLevel = std::unique_ptr<Pine::Level, DisposeLevel>;

    // Compare the full serialized payload, including opaque fields, without asset timestamps/IDs.
    struct AssetPayload : Pine::Serialization::Serializer
    {
        PINE_SERIALIZE_DATA(Data);
    };

    Pine::ByteSpan Payload(const Pine::ByteSpan& file)
    {
        AssetPayload serializer;
        if (!serializer.Read(file))
        {
            throw std::runtime_error("Could not read serialized level data.");
        }
        return serializer.Data.Read();
    }

    bool SameBytes(const Pine::ByteSpan& left, const Pine::ByteSpan& right)
    {
        return left.size == right.size && (left.size == 0 || std::memcmp(left.data, right.data, left.size) == 0);
    }

    OwnedLevel Capture()
    {
        OwnedLevel snapshot(new Pine::Level());
        if (const auto level = Pine::World::GetActiveLevel())
        {
            snapshot->GetLevelSettings() = level->GetLevelSettings();
        }
        snapshot->CreateFromWorld();
        return snapshot;
    }

    fs::path ProjectRoot()
    {
        return fs::weakly_canonical(fs::path(Editor::Projects::GetProjectPath()) / "assets");
    }

    fs::path Destination(const std::string& path)
    {
        const fs::path relative(path);
        Values::Require(!relative.is_absolute() && !relative.empty() && relative.extension() != ".passet"
            && path.find('\\') == std::string::npos && relative.generic_string() == path,
            "/path", "Expected a project-relative virtual path without .passet.");
        for (const auto& part : relative)
        {
            Values::Require(part != "." && part != ".." && !part.empty(), "/path", "Invalid path component.");
        }
        Values::Require(path.back() != '/', "/path", "Expected an asset name.");
        const auto root = ProjectRoot();
        const auto destination = root / (path + ".passet");
        const auto resolved = fs::weakly_canonical(destination);
        const auto within = resolved.lexically_relative(root);
        Values::Require(!within.empty() && !within.is_absolute() && *within.begin() != "..", "/path",
            "Destination must remain inside the project's assets directory.");
        // Keep the requested virtual path and filesystem destination unambiguous.
        auto current = root;
        for (const auto& part : fs::path(path + ".passet"))
        {
            current /= part;
            Values::Require(!fs::is_symlink(current), "/path", "Destination must not traverse symbolic links.");
        }
        return destination;
    }

    struct TemporaryFile
    {
        fs::path Path;
        ~TemporaryFile()
        {
            std::error_code error;
            fs::remove(Path, error);
        }
    };

    void WriteLevel(const fs::path& destination, const Pine::ByteSpan& bytes, const bool overwrite)
    {
        fs::create_directories(destination.parent_path());
        TemporaryFile temporary{ destination.string() + "." + Pine::UId::New().ToString() + ".tmp" };
        Pine::File::WriteCompressed(temporary.Path, bytes);
        if (!fs::is_regular_file(temporary.Path) || !SameBytes(Pine::File::ReadCompressed(temporary.Path), bytes))
        {
            throw std::runtime_error("Could not write and verify the temporary level file.");
        }
        if (overwrite)
        {
            fs::rename(temporary.Path, destination);
        }
        else
        {
            // Linking fails if another writer created the destination after validation.
            fs::create_hard_link(temporary.Path, destination);
        }
    }

    Response SaveLevel(const Request& request, const bool saveAs)
    {
        if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
        {
            return Error(409, "Stop play mode before saving a level.");
        }
        Pine::Level* target = nullptr;
        std::string path;
        fs::path destination;
        bool overwrite = !saveAs;
        try
        {
            Values::Require(request.Body.size() <= 4096, "", "Save request exceeds 4096 bytes.");
            const auto body = request.Body.empty() ? json::object() : json::parse(request.Body, nullptr, false);
            if (saveAs)
            {
                Values::Object(body, "", { "path", "overwrite" }, { "path" });
                path = Values::String(body.at("path"), "/path");
                Values::Require(!body.contains("overwrite") || body.at("overwrite").is_boolean(),
                    "/overwrite", "Expected a boolean.");
                overwrite = body.value("overwrite", false);
                // Asset lookup uses lowercase virtual paths throughout Pine.
                Values::Require(std::none_of(path.begin(), path.end(), [](unsigned char c)
                    { return c >= 'A' && c <= 'Z'; }), "/path", "Use a lowercase virtual path.");
                destination = Destination(path);
                const auto asset = Pine::Assets::GetAssetByPath(path);
                if (asset != nullptr)
                {
                    target = dynamic_cast<Pine::Level*>(asset);
                    Values::Require(target != nullptr, "/path", "Destination belongs to a different asset type.");
                    Values::Require(fs::weakly_canonical(target->GetFilePath()) == destination, "/path",
                        "Destination conflicts with an asset outside this project.");
                }
                if (fs::exists(destination) || target != nullptr)
                {
                    Values::Require(body.value("overwrite", false), "/overwrite", "Destination exists; set overwrite to true.");
                    Values::Require(target != nullptr && fs::is_regular_file(destination), "/path",
                        "Overwrite requires a loaded project Level with a regular file.");
                }
            }
            else
            {
                Values::Object(body, "", {});
                target = Pine::World::GetActiveLevel();
                if (target == nullptr || target->GetFilePath().empty())
                {
                    return Error(409, "The active level has no destination. Use /level/save-as first.");
                }
                path = target->GetPath();
                destination = Destination(path);
                Values::Require(fs::weakly_canonical(target->GetFilePath()) == destination, "/path",
                    "The active level must belong to this project's assets directory.");
            }
        }
        catch (const std::exception& exception)
        {
            return Error(400, exception.what());
        }

        bool fileWritten = false;
        try
        {
            OwnedLevel created;
            if (target == nullptr)
            {
                created.reset(new Pine::Level());
                target = created.get();
                target->SetupNew(destination);
                target->SetPath(path);
            }
            if (const auto active = Pine::World::GetActiveLevel())
            {
                target->GetLevelSettings() = active->GetLevelSettings();
            }
            target->MarkAsModified();
            target->CreateFromWorld();
            WriteLevel(destination, target->Save(), overwrite);
            fileWritten = true;

            // Loading registers a new asset, or returns the already updated object with this ID.
            const auto loaded = dynamic_cast<Pine::Level*>(Pine::Assets::LoadAssetFromFile(path + ".passet"));
            if (loaded == nullptr)
            {
                throw std::runtime_error("Level file was written but could not be registered.");
            }
            loaded->MarkAsSaved();
            Pine::World::SetActiveLevel(loaded, true);
            Panels::AssetBrowser::BuildAssetHierarchy();
            return { 200, { { "path", path }, { "id", loaded->GetUId().ToString() },
                { "fileWritten", true }, { "unsavedChanges", false } } };
        }
        catch (const std::exception& exception)
        {
            return { 500, { { "error", exception.what() }, { "fileWritten", fileWritten },
                { "stateMayHaveChanged", true } } };
        }
    }
}

Editor::DebugServer::Response Editor::DebugServer::Persistence::Get(const Request&)
{
    const auto level = Pine::World::GetActiveLevel();
    const bool hasFile = level != nullptr && !level->GetFilePath().empty();
    json body = { { "path", hasFile ? json(level->GetPath()) : json(nullptr) },
        { "id", hasFile ? json(level->GetUId().ToString()) : json(nullptr) },
        { "hasDestination", hasFile }, { "unsavedChanges", nullptr } };
    if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
    {
        body["reason"] = "Stop play mode to compare the authored scene with disk.";
        return { 200, body };
    }
    try
    {
        if (!hasFile || !fs::is_regular_file(level->GetFilePath()))
        {
            body["unsavedChanges"] = true;
        }
        else
        {
            const auto snapshot = Capture();
            body["unsavedChanges"] = !SameBytes(Payload(snapshot->Save()),
                Payload(Pine::File::ReadCompressed(level->GetFilePath())));
        }
        return { 200, body };
    }
    catch (const std::exception& exception)
    {
        body["error"] = exception.what();
        return { 500, body };
    }
}

Editor::DebugServer::Response Editor::DebugServer::Persistence::Save(const Request& request)
{
    return SaveLevel(request, false);
}

Editor::DebugServer::Response Editor::DebugServer::Persistence::SaveAs(const Request& request)
{
    return SaveLevel(request, true);
}
