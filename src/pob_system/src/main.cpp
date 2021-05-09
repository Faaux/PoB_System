#include <SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL_image.h>
#include <pob_system/keys.h>
#include <pob_system/lua_helper.h>
#include <pob_system/state.h>

#include <Tracy.hpp>
#include <filesystem>
#include <lua.hpp>

void* operator new(std::size_t count)
{
    auto ptr = malloc(count);
    TracyAlloc(ptr, count);
    return ptr;
}
void operator delete(void* ptr) noexcept
{
    TracyFree(ptr);
    free(ptr);
}

int main(int argc, char* argv[])
{
    // std::filesystem::current_path("c:\\Projects\\PathOfBuilding\\src");
    std::filesystem::current_path("c:\\Projects\\PoB_System\\PoBData");

    SDL_Init(SDL_INIT_VIDEO);

    int flags = IMG_INIT_JPG | IMG_INIT_PNG;
    if ((flags & IMG_Init(flags)) != flags)
    {
        return -1;
    }

    if (TTF_Init() != 0)
    {
        return -1;
    }

    // Lives till application close
    auto state = std::make_unique<state_t>(argc, argv);
    state_t::instance = state.get();
    state->current_frame = 0;
    state->lua_state.do_file("Launch.lua");

    auto on_init_task = state->lua_state.on_init();
    state->render_thread.run_all_work(on_init_task);
    cb::sync_wait(on_init_task);

    while (state->render_state.is_init)
    {
        // Get the next event
        SDL_Event event;
        if (SDL_WaitEvent(&event))
        {
            if (event.type == SDL_QUIT && state->lua_state.can_exit())
            {
                // Break out of the loop on quit
                state->lua_state.on_exit();
                break;
            }
            else if (event.type == SDL_TEXTINPUT)
            {
                char* text = event.text.text;
                while (*text)
                {
                    state->lua_state.on_char(*text);
                    text++;
                }
            }
            else if (event.type == SDL_KEYDOWN)
            {
                state->lua_state.on_key_down(event.key.keysym.sym);
            }
            else if (event.type == SDL_KEYUP)
            {
                state->lua_state.on_key_up(event.key.keysym.sym);
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                bool double_click = event.button.clicks == 2;
                state->lua_state.on_mouse_down(event.button.button, double_click);
            }
            else if (event.type == SDL_MOUSEBUTTONUP)
            {
                state->lua_state.on_mouse_up(event.button.button);
            }
            else if (event.type == SDL_MOUSEWHEEL)
            {
                if (event.wheel.y > 0)
                {
                    state->lua_state.on_mouse_down(SDL_BUTTON_WHEELUP, false);
                    state->lua_state.on_mouse_up(SDL_BUTTON_WHEELUP);
                }
                else if (event.wheel.y < 0)  // scroll down
                {
                    state->lua_state.on_mouse_down(SDL_BUTTON_WHEELDOWN, false);
                    state->lua_state.on_mouse_up(SDL_BUTTON_WHEELDOWN);
                }
            }
        }

        auto on_frame_task = state->lua_state.on_frame();

        // Run all work that needs to be done on the render thread, e.g creating font textures
        state->render_thread.run_all_work(on_frame_task);
        cb::sync_wait(on_frame_task);

        // Fill the screen with the colour
        SDL_SetRenderDrawColor(state->render_state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(state->render_state.renderer);

        // Execute draw commands
        {
            ZoneScopedN("Render text");
            for (auto& command : state->lua_state.test_commands)
            {
                auto data = cb::sync_wait(command.texture);
                if (data.is_valid)
                {
                    SDL_RenderCopy(state->render_state.renderer, data.texture, nullptr, &data.rect);
                }
            }
            state->lua_state.test_commands.clear();
        }
        state->render_state.font_manager.clear_stale_caches();
        SDL_RenderPresent(state->render_state.renderer);
        state->current_frame++;
        FrameMark;
    }

    // Explicitly destruct state
    state.reset();

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}