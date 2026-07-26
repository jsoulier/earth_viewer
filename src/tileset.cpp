#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <CesiumAsync/CachingAssetAccessor.h>
#include <CesiumAsync/SqliteCache.h>
#include <CesiumCurl/CurlAssetAccessor.h>
#include <CesiumUtility/CreditSystem.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>

#include "camera.hpp"
#include "log_sink.hpp"
#include "prepare_renderer_resources.hpp"
#include "task_processor.hpp"
#include "tileset.hpp"

namespace Cesium3DTilesSelection
{

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    TilesetOptions,
    maximumScreenSpaceError,
    maximumSimultaneousTileLoads,
    loadingDescendantLimit,
    maximumCachedBytes,
    forbidHoles,
    preloadAncestors,
    preloadSiblings,
    enableFrustumCulling,
    enableOcclusionCulling,
    enableFogCulling)

}

namespace CesiumRasterOverlays
{

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    RasterOverlayOptions,
    maximumTextureSize,
    maximumScreenSpaceError)

}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    SDLTilesetConfig,
    IonAssetID,
    IonImageryID,
    TilesetOptions,
    RasterOverlayOptions)

static constexpr int kDefaultIonAssetID = 2275207;
static constexpr int kDefaultIonImageryID = -1;
static constexpr const char* kConfigFileName = "tileset_config.json";
static constexpr const char* kDefaultIonTokenFileName = "cesium_ion_token.txt";
static constexpr const char* kDatabaseFileName = "tileset.sqlite3";
static constexpr double kZero = 0.0;
static constexpr double kMaxSSE = 256.0;
static constexpr double kMaxRasterSSE = 64.0;
static constexpr int kMinTileLoads = 1;
static constexpr int kMaxTileLoads = 256;
static constexpr int kMinDescendantLimit = 0;
static constexpr int kMaxDescendantLimit = 256;
static constexpr int64_t kMinCacheMB = 0;
static constexpr int64_t kMaxCacheMB = 65536;
static constexpr int kMinRasterTextureSize = 64;
static constexpr int kMaxRasterTextureSize = 8192;
static constexpr int kMaxDatabaseCacheItems = 16384;

static std::string GetIonToken()
{
    const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME);
    if (!home)
    {
        SDL_Log("Failed to get home folder: %s", SDL_GetError());
        return "";
    }
    std::filesystem::path path = std::filesystem::path(home) / kDefaultIonTokenFileName;
    std::ifstream file(path);
    if (!file.is_open())
    {
        SDL_Log("Failed to open token file: %s", path.string().data());
        return "";
    }
    std::string token;
    std::getline(file, token);
    size_t end = token.find_last_not_of(" \n\r\t");
    if (end != std::string::npos)
    {
        token = token.substr(0, end + 1);
    }
    else
    {
        token.clear();
    }
    return token;
}

static std::string GetCacheDatabasePath()
{
    char* prefPath = SDL_GetPrefPath("jsoulier", "sdl_earth_viewer");
    if (!prefPath)
    {
        SDL_Log("Failed to get cache database folder: %s", SDL_GetError());
        return kDatabaseFileName;
    }
    const std::filesystem::path path = std::filesystem::path(prefPath) / kDatabaseFileName;
    SDL_free(prefPath);
    return path.string();
}

SDLTilesetConfig::SDLTilesetConfig()
    : IonAssetID{kDefaultIonAssetID}
    , IonImageryID{kDefaultIonImageryID}
{
    TilesetOptions.loadErrorCallback = [](const Cesium3DTilesSelection::TilesetLoadFailureDetails& details)
    {
        SDL_Log("%s", details.message.data());
    };
}

SDLTilesetConfig SDLTilesetConfig::Load()
{
    if (!std::filesystem::exists(kConfigFileName))
    {
        return SDLTilesetConfig();
    }
    try
    {
        std::ifstream file(kConfigFileName);
        nlohmann::json json;
        file >> json;
        return json.get<SDLTilesetConfig>();
    }
    catch (const std::exception& e)
    {
        SDL_Log("Failed to load %s: %s", kConfigFileName, e.what());
    }
    return {};
}

void SDLTilesetConfig::Save() const
{
    try
    {
        std::ofstream file(kConfigFileName);
        nlohmann::json json = *this;
        file << json.dump(4);
    }
    catch (const std::exception& e)
    {
        SDL_Log("Failed to save %s: %s", kConfigFileName, e.what());
    }
}

bool SDLTilesetConfig::RenderImGui()
{
    int ionAssetID = IonAssetID;
    if (ImGui::InputInt("Ion Asset ID", &ionAssetID))
    {
        IonAssetID = ionAssetID;
    }
    int ionImageryID = IonImageryID;
    if (ImGui::InputInt("Ion Imagery ID", &ionImageryID))
    {
        IonImageryID = ionImageryID;
    }
    ImGui::DragScalar("Maximum SSE", ImGuiDataType_Double, &TilesetOptions.maximumScreenSpaceError, 0.1f, &kZero, &kMaxSSE);
    int maxTileLoads = TilesetOptions.maximumSimultaneousTileLoads;
    if (ImGui::DragInt("Max Tile Loads", &maxTileLoads, 1.0f, kMinTileLoads, kMaxTileLoads))
    {
        TilesetOptions.maximumSimultaneousTileLoads = maxTileLoads;
    }
    int loadingLimit = TilesetOptions.loadingDescendantLimit;
    if (ImGui::DragInt("Loading Descendant Limit", &loadingLimit, 1.0f, kMinDescendantLimit, kMaxDescendantLimit))
    {
        TilesetOptions.loadingDescendantLimit = loadingLimit;
    }
    int64_t maxCachedMB = TilesetOptions.maximumCachedBytes / (1024 * 1024);
    if (ImGui::DragScalar("Max Cache (MB)", ImGuiDataType_S64, &maxCachedMB, 1.0f, &kMinCacheMB, &kMaxCacheMB))
    {
        TilesetOptions.maximumCachedBytes = maxCachedMB * 1024 * 1024;
    }
    ImGui::Checkbox("Forbid Holes", &TilesetOptions.forbidHoles);
    ImGui::Checkbox("Preload Ancestors", &TilesetOptions.preloadAncestors);
    ImGui::Checkbox("Preload Siblings", &TilesetOptions.preloadSiblings);
    ImGui::Checkbox("Frustum Culling", &TilesetOptions.enableFrustumCulling);
    ImGui::Checkbox("Occlusion Culling", &TilesetOptions.enableOcclusionCulling);
    ImGui::Checkbox("Fog Culling", &TilesetOptions.enableFogCulling);
    ImGui::DragInt("Max Texture Size", &RasterOverlayOptions.maximumTextureSize, 1.0f, kMinRasterTextureSize, kMaxRasterTextureSize);
    ImGui::DragScalar("Max SSE", ImGuiDataType_Double, &RasterOverlayOptions.maximumScreenSpaceError, 0.1f, &kZero, &kMaxRasterSSE);
    if (ImGui::Button("Save"))
    {
        Save();
    }
    ImGui::SameLine();
    return ImGui::Button("Create");
}

SDLTileset::SDLTileset()
: AsyncSystem{nullptr}
{
}

const Cesium3DTilesSelection::ViewUpdateResult& SDLTileset::Update(const SDLCamera& camera)
{
    Tileset->updateViewGroup(Tileset->getDefaultViewGroup(), {camera.GetViewState()});
    Tileset->loadTiles();
    AsyncSystem.dispatchMainThreadTasks();
    return Tileset->getDefaultViewGroup().getViewUpdateResult();
}

std::shared_ptr<SDLTileset> SDLTileset::Create(const SDLTilesetConfig& config)
{
    if (config.IonAssetID == -1)
    {
        SDL_Log("Ion asset ID is invalid");
        return nullptr;
    }
    const std::string ionToken = GetIonToken();
    if (ionToken.empty())
    {
        SDL_Log("Ion token is empty");
        return nullptr;
    }
    std::shared_ptr<SDLTileset> tileset = std::make_shared<SDLTileset>();
    std::shared_ptr<SDLLogSink<std::mutex>> logSink = std::make_shared<SDLLogSink<std::mutex>>();
    std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::logger>("cesium", logSink);
    std::shared_ptr<SDLTaskProcessor> taskProcessor = std::make_shared<SDLTaskProcessor>();
    std::shared_ptr<CesiumAsync::IAssetAccessor> curlAssetAccessor = std::make_shared<CesiumCurl::CurlAssetAccessor>();
    std::shared_ptr<CesiumAsync::ICacheDatabase> cacheDatabase = std::make_shared<CesiumAsync::SqliteCache>(logger, GetCacheDatabasePath(), kMaxDatabaseCacheItems);
    std::shared_ptr<CesiumAsync::IAssetAccessor> cachingAssetAccessor = std::make_shared<CesiumAsync::CachingAssetAccessor>(logger, curlAssetAccessor, cacheDatabase);
    std::shared_ptr<CesiumUtility::CreditSystem> creditSystem = std::make_shared<CesiumUtility::CreditSystem>();
    tileset->AsyncSystem = CesiumAsync::AsyncSystem(taskProcessor);
    Cesium3DTilesSelection::TilesetExternals externals{cachingAssetAccessor, config.PrepareRendererResources, tileset->AsyncSystem, creditSystem, logger};
    Cesium3DTilesSelection::TilesetOptions tilesetOptions = config.TilesetOptions;
    if (config.PrepareRendererResources)
    {
        tilesetOptions.contentOptions.ktx2TranscodeTargets = config.PrepareRendererResources->GetKtx2TranscodeTargets();
    }
    tileset->Tileset = std::make_unique<Cesium3DTilesSelection::Tileset>(externals, config.IonAssetID, ionToken, tilesetOptions);
    if (config.IonImageryID != -1)
    {
        tileset->Tileset->getOverlays().add(new CesiumRasterOverlays::IonRasterOverlay("overlay", config.IonImageryID, ionToken, config.RasterOverlayOptions));
    }
    return tileset;
}

void SDLTileset::RenderImGui() const
{
    const Cesium3DTilesSelection::ViewUpdateResult& result = Tileset->getDefaultViewGroup().getViewUpdateResult();
    ImGui::Text("Tiles to Render: %zu", result.tilesToRenderThisFrame.size());
    ImGui::Text("Tiles Visited: %u", result.tilesVisited);
    ImGui::Text("Tiles Culled: %u", result.tilesCulled);
    ImGui::Text("Tiles Occluded: %u", result.tilesOccluded);
    ImGui::Text("Max Depth Visited: %u", result.maxDepthVisited);
    ImGui::Text("Worker Queue: %d", result.workerThreadTileLoadQueueLength);
    ImGui::Text("Main Queue: %d", result.mainThreadTileLoadQueueLength);
}
