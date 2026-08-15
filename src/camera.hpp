#pragma once

#include <Cesium3DTilesSelection/SampleHeightResult.h>
#include <Cesium3DTilesSelection/ViewState.h>
#include <CesiumAsync/Future.h>
#include <CesiumGeospatial/Cartographic.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include <memory>
#include <optional>

class Tileset;

class Camera
{
public:
    Camera();
    void Handle(const SDL_Event& event);
    void Update(const std::shared_ptr<Tileset>& tileset);
    void Resize(int width, int height);
    Cesium3DTilesSelection::ViewState GetViewState() const;
    glm::dmat4 GetProjMatrix() const;
    glm::dmat4 GetViewMatrix() const;
    glm::dvec3 GetPosition() const;
    double GetDistance() const;
    double GetPitch() const;
    double GetYaw() const;
    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    double GetAspectRatio() const;
    double GetFovX() const;
    static glm::dvec3 EulerToDirection(double pitch, double yaw);

private:
    glm::dvec3 Position;
    glm::dvec3 Forward;
    glm::dvec3 Up;
    glm::uvec2 Viewport;
    double Distance;
    double MinDistance;
    double Pitch;
    double Yaw;
    std::optional<CesiumAsync::Future<Cesium3DTilesSelection::SampleHeightResult>> Future;
};
