#pragma once

#include <SDL3/SDL.h>

#define DebugGroup(commandBuffer) 
#define DebugGroupBlock(commandBuffer, name) 

class DebugGroupClass
{
public:
    DebugGroupClass(SDL_GPUCommandBuffer* commandBuffer, const char* name);
    ~DebugGroupClass();

private:
    SDL_GPUCommandBuffer* CommandBuffer;
};
