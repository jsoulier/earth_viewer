#include <Cesium3DTilesSelection/ViewState.h>
#include <CesiumGeometry/Transforms.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>

#include "camera.hpp"
#include "tileset.hpp"

static constexpr glm::dvec3 kUp = glm::dvec3(0.0, 0.0, 1.0);
static constexpr double kMaxPitch = glm::pi<double>() / 2.0 - 0.001;
static constexpr double kNear = 1.0;
static constexpr double kFar = kNear * 1e8;
static constexpr double kFovY = glm::radians(45.0);
static constexpr double kEarthRadius = 6378.137e3;
static constexpr double kArcSpeed = 0.1e-9;
static constexpr double kZoomSpeed = 0.1;
static constexpr double kMinSpeed = 500.0;
static constexpr double kMinAltitude = 2.0;

Camera::Camera()
    : Position{0.0, 0.0, 0.0}
    , Forward{0.0, -1.0, 0.0}
    , Up{kUp}
    , Viewport{0, 0}
    , Distance{20000.0e3}
    , MinDistance{kEarthRadius}
    , Pitch{0.0}
    , Yaw{glm::half_pi<double>()}
{
    Update(nullptr);
}

void Camera::Handle(const SDL_Event& event)
{
    double speed = std::max(Distance - MinDistance, 0.0);
    switch (event.type)
    {
    case SDL_EVENT_MOUSE_MOTION:
    {
        if (event.motion.state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))
        {
            Yaw -= event.motion.xrel * speed * kArcSpeed;
            Pitch = std::clamp(Pitch + event.motion.yrel * speed * kArcSpeed, -kMaxPitch, kMaxPitch);
        }
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL:
    {
        Distance -= event.wheel.y * speed * kZoomSpeed;
        break;
    }
    }
}

void Camera::Update(const std::shared_ptr<Tileset>& tileset)
{
    if (Future && Future->isReady())
    {
        try
        {
            Cesium3DTilesSelection::SampleHeightResult result = Future->wait();
            if (!result.sampleSuccess.empty() && result.sampleSuccess[0])
            {
                MinDistance = glm::length(CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian(result.positions[0]));
            }
        }
        catch (const std::exception& exception)
        {
            SDL_Log("Failed to sample height: %s", exception.what());
        }
        Future.reset();
    }
    glm::dvec3 direction = EulerToDirection(Pitch, Yaw);
    Distance = std::max(Distance, MinDistance + kMinAltitude);
    Position = Distance * direction;
    Forward = glm::normalize(-Position);
    Up = kUp;
    std::optional<CesiumGeospatial::Cartographic> cartographic = CesiumGeospatial::Ellipsoid::WGS84.cartesianToCartographic(Position);
    if (cartographic && tileset && !Future)
    {
        Future = tileset->Sample(*cartographic);
    }
}

void Camera::Resize(int width, int height)
{
    Viewport = {width, height};
}

Cesium3DTilesSelection::ViewState Camera::GetViewState() const
{
    return Cesium3DTilesSelection::ViewState(GetViewMatrix(), GetProjMatrix(), glm::dvec2(Viewport));
}

glm::dmat4 Camera::GetProjMatrix() const
{
    return CesiumGeometry::Transforms::createPerspectiveMatrix(GetFovX(), kFovY, kNear, glm::length(GetPosition()));
}

glm::dmat4 Camera::GetViewMatrix() const
{
    return CesiumGeometry::Transforms::createViewMatrix(Position, Forward, Up);
}

glm::dvec3 Camera::GetPosition() const
{
    return Position;
}

double Camera::GetDistance() const
{
    return Distance;
}

double Camera::GetPitch() const
{
    return Pitch;
}

double Camera::GetYaw() const
{
    return Yaw;
}

uint32_t Camera::GetWidth() const
{
    return Viewport.x;
}

uint32_t Camera::GetHeight() const
{
    return Viewport.y;
}

double Camera::GetAspectRatio() const
{
    return double(Viewport.x) / double(Viewport.y);
}

double Camera::GetFovX() const
{
    return 2.0 * std::atan(std::tan(kFovY / 2.0) * GetAspectRatio());
}

glm::dvec3 Camera::EulerToDirection(double pitch, double yaw)
{
    return {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
}
