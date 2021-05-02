#include <SDL.h>
#include <pob_system/keys.h>

#include <algorithm>
#include <unordered_map>

template <typename K, typename V>
std::unordered_map<V, K> inverse_map(const std::unordered_map<K, V> &map)
{
    std::unordered_map<V, K> inv;
    std::for_each(map.begin(), map.end(),
                  [&inv](const std::pair<K, V> &p) { inv.insert(std::make_pair(p.second, p.first)); });
    return inv;
}

const static std::unordered_map<std::string_view, SDL_Keycode> view_to_keys{{"BACK", SDLK_BACKSPACE},
                                                                            {"TAB", SDLK_TAB},
                                                                            {"RETURN", SDLK_RETURN},
                                                                            {"ESCAPE", SDLK_ESCAPE},
                                                                            {"SHIFT", SDLK_LSHIFT},
                                                                            {"CTRL", SDLK_LCTRL},
                                                                            {"ALT", SDLK_LALT},
                                                                            {"PAUSE", SDLK_PAUSE},
                                                                            {"PAGEUP", SDLK_PAGEUP},
                                                                            {"PAGEDOWN", SDLK_PAGEDOWN},
                                                                            {"END", SDLK_END},
                                                                            {"HOME", SDLK_HOME},
                                                                            {"PRINTSCREEN", SDLK_PRINTSCREEN},
                                                                            {"INSERT", SDLK_INSERT},
                                                                            {"DELETE", SDLK_DELETE},
                                                                            {"UP", SDLK_UP},
                                                                            {"DOWN", SDLK_DOWN},
                                                                            {"LEFT", SDLK_LEFT},
                                                                            {"RIGHT", SDLK_RIGHT},
                                                                            {"F1", SDLK_F1},
                                                                            {"F2", SDLK_F2},
                                                                            {"F3", SDLK_F3},
                                                                            {"F4", SDLK_F4},
                                                                            {"F5", SDLK_F5},
                                                                            {"F6", SDLK_F6},
                                                                            {"F7", SDLK_F7},
                                                                            {"F8", SDLK_F8},
                                                                            {"F9", SDLK_F9},
                                                                            {"F10", SDLK_F10},
                                                                            {"F11", SDLK_F11},
                                                                            {"F12", SDLK_F12},
                                                                            {"F13", SDLK_F13},
                                                                            {"F14", SDLK_F14},
                                                                            {"F15", SDLK_F15},
                                                                            {"NUMLOCK", SDLK_NUMLOCKCLEAR},
                                                                            {"SCROLLLOCK", SDLK_SCROLLLOCK}};

const static std::unordered_map<SDL_Keycode, std::string_view> keys_to_view = inverse_map(view_to_keys);

const static std::unordered_map<std::string_view, int> view_to_mouse_key{
    {"LEFTBUTTON", SDL_BUTTON_LEFT},    {"MIDDLEBUTTON", SDL_BUTTON_MIDDLE}, {"RIGHTBUTTON", SDL_BUTTON_RIGHT},
    {"MOUSE4", SDL_BUTTON_X1},          {"MOUSE5", SDL_BUTTON_X2},           {"WHEELUP", SDL_BUTTON_WHEELUP},
    {"WHEELDOWN", SDL_BUTTON_WHEELDOWN}};

const static std::unordered_map<int, std::string_view> mouse_key_to_view = inverse_map(view_to_mouse_key);

SDL_Keycode get_keycode(std::string_view key)
{
    auto it = view_to_keys.find(key);
    if (it == view_to_keys.end())
    {
        return SDLK_UNKNOWN;
    }
    return it->second;
}

std::string_view get_key_name(SDL_Keycode key)
{
    auto it = keys_to_view.find(key);
    if (it == keys_to_view.end())
    {
        return std::string_view();
    }
    return it->second;
}

int get_mouse_key(std::string_view key)
{
    auto it = view_to_mouse_key.find(key);
    if (it == view_to_mouse_key.end())
    {
        return 0;
    }
    return it->second;
}

std::string_view get_mouse_name(int key)
{
    auto it = mouse_key_to_view.find(key);
    if (it == mouse_key_to_view.end())
    {
        return std::string_view();
    }
    return it->second;
}
