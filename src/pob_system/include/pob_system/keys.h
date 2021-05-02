#pragma once
#include <SDL.h>

#include <string_view>

#define SDL_BUTTON_WHEELUP 6
#define SDL_BUTTON_WHEELDOWN 7

SDL_Keycode get_keycode(std::string_view key);
std::string_view get_key_name(SDL_Keycode key);
int get_mouse_key(std::string_view key);
std::string_view get_mouse_name(int key);
