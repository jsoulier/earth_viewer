#include <Cesium3DTilesSelection/ViewState.h>
#include <CesiumGeometry/Transforms.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <print>

#include "camera.hpp"

static constexpr glm::dvec3 kUp = glm::dvec3(0.0, 0.0, 1.0);
static constexpr double kMaxPitch = glm::pi<double>() / 2.0 - 0.001;
static constexpr double kNear = 1.0;
static constexpr double kFar = kNear * 1e8;
static constexpr double kFovY = glm::radians(45.0);
static constexpr double kEarthRadius = 6378.137e3;
static constexpr double kArcSpeed = 0.1e-9;
static constexpr double kZoomSpeed = 0.1;
static constexpr double kMinSpeed = 500.0;

SDLCamera::SDLCamera()
    : Position{0.0, 0.0, 0.0}
    , Forward{0.0, -1.0, 0.0}
    , Up{kUp}
    , Viewport{0, 0}
    , Distance{20000.0e3}
    , Pitch{0.0}
    , Yaw{glm::half_pi<double>()}
{
    Update();
}

void SDLCamera::Handle(const SDL_Event& event)
{
    double altitude = glm::length(Position) - kEarthRadius;
    double speed = std::max(kMinSpeed, altitude);
    switch (event.type)
    {
    case SDL_EVENT_MOUSE_MOTION:
    {
        if (event.motion.state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))
        {
            Yaw -= event.motion.xrel * speed * kArcSpeed;
            Pitch = std::clamp(Pitch + event.motion.yrel * speed * kArcSpeed, -kMaxPitch, kMaxPitch);
            Update();
        }
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL:
    {
        Distance -= event.wheel.y * speed * kZoomSpeed;
        Update();
        break;
    }
    }
}

void SDLCamera::Update()
{
    Position = Distance * EulerToDirection(Pitch, Yaw);
    Forward = glm::normalize(-Position);
    Up = kUp;
}

void SDLCamera::Resize(int width, int height)
{
    Viewport = {width, height};
}

Cesium3DTilesSelection::ViewState SDLCamera::GetViewState() const
{
    return Cesium3DTilesSelection::ViewState(GetViewMatrix(), GetProjMatrix(), glm::dvec2(Viewport));
}

glm::dmat4 SDLCamera::GetProjMatrix() const
{
    return CesiumGeometry::Transforms::createPerspectiveMatrix(GetFovX(), kFovY, kNear, glm::length(GetPosition()));
}

glm::dmat4 SDLCamera::GetViewMatrix() const
{
    return CesiumGeometry::Transforms::createViewMatrix(Position, Forward, Up);
}

glm::dvec3 SDLCamera::GetPosition() const
{
    return Position;
}

double SDLCamera::GetDistance() const
{
    return Distance;
}

double SDLCamera::GetPitch() const
{
    return Pitch;
}

double SDLCamera::GetYaw() const
{
    return Yaw;
}

uint32_t SDLCamera::GetWidth() const
{
    return Viewport.x;
}

uint32_t SDLCamera::GetHeight() const
{
    return Viewport.y;
}

double SDLCamera::GetAspectRatio() const
{
    return double(Viewport.x) / double(Viewport.y);
}

double SDLCamera::GetFovX() const
{
    return 2.0 * std::atan(std::tan(kFovY / 2.0) * GetAspectRatio());
}

glm::dvec3 SDLCamera::EulerToDirection(double pitch, double yaw)
{
    return {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
}
