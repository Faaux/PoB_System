#pragma once
#include <SDL2/SDL_pixels.h>
#include <tasks/task.h>

#include <Tracy.hpp>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

typedef struct _TTF_Font TTF_Font;
struct SDL_Texture;

enum class font_alignment
{
    LEFT,
    CENTER,
    RIGHT,
    CENTER_X,
    RIGHT_X
};

enum class font_type
{
    VERA,
    LIB,
    LIB_BOLD
};

struct draw_data
{
    bool is_valid;
    SDL_Rect rect;
    font_alignment align;
    SDL_Texture* texture;
};

struct draw_string_command
{
    cb::task<draw_data> texture;
};

struct font_texture_cache
{
    SDL_Texture* texture = nullptr;
    int w;
    int h;
    uint64_t frame_last_used;
};

struct length_texture_cache
{
    int w;
    uint64_t frame_last_used;
};

class font_manager_t
{
   public:
    inline static const char* alignMap[6] = {"LEFT", "CENTER", "RIGHT", "CENTER_X", "RIGHT_X", nullptr};
    inline static const char* fontMap[4] = {"FIXED", "VAR", "VAR BOLD", nullptr};

    ~font_manager_t();

    draw_string_command create_draw_string_command(int left, int top, font_alignment align, int height,
                                                   font_type font_type, std::string_view text);
    int string_width(int height, font_type font_type, std::string_view text);
    void clear_stale_caches();

   private:
    std::string escape_color(std::string_view text, SDL_Color* color);
    TTF_Font* ensure_font(font_type font_type, int height);
    cb::task<draw_data> render_font_to_texture(int left, int top, font_alignment align, TTF_Font* font,
                                               std::string original_text);

    TracySharedLockable(std::shared_mutex, length_cache_lock_);
    std::unordered_map<TTF_Font*, std::unordered_map<std::string, length_texture_cache>> string_length_cache_;
    TracySharedLockable(std::shared_mutex, texture_cache_lock_);
    std::unordered_map<TTF_Font*, std::unordered_map<std::string, font_texture_cache>> string_texture_cache_;
    std::unordered_map<font_type, std::unordered_map<int, TTF_Font*>> fonts_;
};