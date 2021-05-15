#pragma once

struct viewport_command_t
{
    int x;
    int y;

    int width;
    int height;

    SDL_Renderer* renderer;

    void execute()
    {
        //Build and set viewport
        SDL_Rect viewport{x, y, width, height};
        SDL_RenderSetViewport(renderer, &viewport);
    }
};