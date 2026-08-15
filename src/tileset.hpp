#pragma once

#include <Cesium3DTilesSelection/SampleHeightResult.h>
#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <CesiumAsync/Future.h>
#include <CesiumCurl/CurlAssetAccessor.h>
#include <CesiumGeospatial/Cartographic.h>
#include <CesiumRasterOverlays/IonRasterOverlay.h>
#include <CesiumUtility/CreditSystem.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

class Camera;
class PrepareRendererResources;
class TaskProcessor;

class TilesetConfig
{
public:
    TilesetConfig();
    static TilesetConfig Load();
    void Save() const;
    bool RenderImGui();

    Cesium3DTilesSelection::TilesetOptions TilesetOptions;
    CesiumRasterOverlays::RasterOverlayOptions RasterOverlayOptions;
    std::shared_ptr<PrepareRendererResources> PrepareRendererResourcesHandle;
    int64_t IonAssetID;
    int64_t IonImageryID;
};

class Tileset
{
public:
    Tileset();
    const Cesium3DTilesSelection::ViewUpdateResult& Update(const Camera& camera);
    CesiumAsync::Future<Cesium3DTilesSelection::SampleHeightResult> Sample(const CesiumGeospatial::Cartographic& position);
    static std::shared_ptr<Tileset> Create(const TilesetConfig& config);
    void RenderImGui() const;

private:
    std::unique_ptr<Cesium3DTilesSelection::Tileset> TilesetHandle;
    CesiumAsync::AsyncSystem AsyncSystem;
};
