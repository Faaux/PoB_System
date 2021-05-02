#include <SDL.h>
#include <SDL_image.h>
#include <pob_system/keys.h>
#include <pob_system/lua_helper.h>
#include <pob_system/state.h>

#include <filesystem>
#include <lua.hpp>

int main(int argc, char* argv[])
{
    std::filesystem::current_path("c:\\Projects\\\PathOfBuilding\\src");
    // std::filesystem::current_path("c:\\Projects\\\PoB_System\\PoBData");

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
            if (event.type == SDL_QUIT && state.lua_state.canExit())
            {
                // Break out of the loop on quit
                break;
            }
            else if (event.type == SDL_TEXTINPUT)
            {
                char* text = event.text.text;
                while (*text)
                {
                    state.lua_state.onChar(*text);
                    text++;
                }
            }
            else if (event.type == SDL_KEYDOWN)
            {
                state.lua_state.onKeyDown(event.key.keysym.sym);
            }
            else if (event.type == SDL_KEYUP)
            {
                state.lua_state.onKeyUp(event.key.keysym.sym);
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                bool double_click = event.button.clicks == 2;
                state.lua_state.onMouseDown(event.button.button, double_click);
            }
            else if (event.type == SDL_MOUSEBUTTONUP)
            {
                state.lua_state.onMouseUp(event.button.button);
            }
            else if (event.type == SDL_MOUSEWHEEL)
            {
                if (event.wheel.y > 0)
                {
                    state.lua_state.onMouseDown(SDL_BUTTON_WHEELUP, false);
                    state.lua_state.onMouseUp(SDL_BUTTON_WHEELUP);
                }
                else if (event.wheel.y < 0)  // scroll down
                {
                    state.lua_state.onMouseDown(SDL_BUTTON_WHEELDOWN, false);
                    state.lua_state.onMouseUp(SDL_BUTTON_WHEELDOWN);
                }
            }
        }

        // state.lua_state.onFrame();

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