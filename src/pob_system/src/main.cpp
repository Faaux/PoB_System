#include <SDL.h>
#include <SDL_image.h>
#include <pob_system/keys.h>
#include <pob_system/lua_helper.h>
#include <pob_system/state.h>

#include <filesystem>
#include <lua.hpp>

int main(int argc, char* argv[])
{
    //std::filesystem::current_path("c:\\Projects\\PathOfBuilding\\src");
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
    state.lua_state.on_init();

    while (state.render_state.is_init)
    {
        state.lua_state.clear_command_list();
        // Get the next event
        SDL_Event event;
        if (SDL_WaitEvent(&event))
        {
            if (event.type == SDL_QUIT && state.lua_state.can_exit())
            {
                // Break out of the loop on quit
                state.lua_state.on_exit();
                break;
            }
            else if (event.type == SDL_TEXTINPUT)
            {
                char* text = event.text.text;
                while (*text)
                {
                    state.lua_state.on_char(*text);
                    text++;
                }
            }
            else if (event.type == SDL_KEYDOWN)
            {
                state.lua_state.on_key_down(event.key.keysym.sym);
            }
            else if (event.type == SDL_KEYUP)
            {
                state.lua_state.on_key_up(event.key.keysym.sym);
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                bool double_click = event.button.clicks == 2;
                state.lua_state.on_mouse_down(event.button.button, double_click);
            }
            else if (event.type == SDL_MOUSEBUTTONUP)
            {
                state.lua_state.on_mouse_up(event.button.button);
            }
            else if (event.type == SDL_MOUSEWHEEL)
            {
                if (event.wheel.y > 0)
                {
                    state.lua_state.on_mouse_down(SDL_BUTTON_WHEELUP, false);
                    state.lua_state.on_mouse_up(SDL_BUTTON_WHEELUP);
                }
                else if (event.wheel.y < 0)  // scroll down
                {
                    state.lua_state.on_mouse_down(SDL_BUTTON_WHEELDOWN, false);
                    state.lua_state.on_mouse_up(SDL_BUTTON_WHEELDOWN);
                }
            }
        }

        state.lua_state.on_frame();

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