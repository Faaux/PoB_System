#include <pob_system/lua_helper.h>
#include <pob_system/state.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_image.h>

#include <filesystem>
#include <lua.hpp>

int main(int argc, char* argv[])
{
    // std::filesystem::current_path("c:\\Projects\\\PathOfBuilding\\src");
    std::filesystem::current_path("c:\\Projects\\\PoB_System\\PoBData");

    SDL_Init(SDL_INIT_VIDEO);

    int flags = IMG_INIT_JPG | IMG_INIT_PNG;
    if ((flags & IMG_Init(flags)) != flags)
    {
        return -1;
    }

    // Lives till application close
    state_t state(argc, argv);
    state_t::instance = &state;

    state.lua_state.do_file("Launch.lua");
    state.lua_state.onInit();

    while (state.render_state.is_init)
    {
        // Get the next event
        SDL_Event event;
        if (SDL_WaitEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                // Break out of the loop on quit
                break;
            }
        }

        state.lua_state.onFrame();

        // Randomly change the colour
        Uint8 red = rand() % 255;
        Uint8 green = rand() % 255;
        Uint8 blue = rand() % 255;
        // Fill the screen with the colour
        SDL_SetRenderDrawColor(state.render_state.renderer, red, green, blue, 255);
        SDL_RenderClear(state.render_state.renderer);
        SDL_RenderPresent(state.render_state.renderer);
    }

    IMG_Quit();
    SDL_Quit();
    return 0;
}