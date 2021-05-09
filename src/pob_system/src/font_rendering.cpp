#include <SDL2/SDL_ttf.h>
#include <pob_system/font_rendering.h>
#include <pob_system/state.h>

#include <Tracy.hpp>
#include <cctype>
font_manager_t::~font_manager_t()
{
    for (auto& [_, fonts] : fonts_)
    {
        for (auto& [_ignore, font] : fonts)
        {
            TTF_CloseFont(font);
        }
    }
}

draw_string_command font_manager_t::create_draw_string_command(int left, int top, font_alignment align, int height,
                                                               font_type font_type, std::string_view text)
{
    ZoneScoped;

    auto ttf_font = ensure_font(font_type, height);

    return draw_string_command{render_font_to_texture(left, top, align, ttf_font, text.data())};
}

int font_manager_t::string_width(int height, font_type font_type, std::string_view text)
{
    ZoneScoped;

    auto font = ensure_font(font_type, height);
    {
        std::shared_lock lock(length_cache_lock_);
        auto it = string_length_cache_[font].find(text.data());
        if (it != string_length_cache_[font].end())
        {
            return it->second.w;
        }
    }
    auto escaped_text = escape_color(text, nullptr);

    int w = cb::sync_wait(
        [this](std::string escaped_text, TTF_Font* font) -> cb::task<int>
        {
            co_await state_t::instance->render_thread.schedule();
            int w = -1;
            TTF_SizeText(font, escaped_text.c_str(), &w, nullptr);
            co_return w;
        }(escaped_text, font));

    {
        std::unique_lock lock(length_cache_lock_);
        string_length_cache_[font][text.data()] = {w, state_t::instance->current_frame};
    }
    return w;
}

void font_manager_t::clear_stale_caches()
{
    ZoneScoped;
    // Textures
    int texture_cleared = 0;
    const auto current_frame = state_t::instance->current_frame;
    {
        std::unique_lock lock(texture_cache_lock_);
        for (auto& [_, cache] : string_texture_cache_)
        {
            for (auto it = cache.begin(); it != cache.end();)
            {
                const auto dist = current_frame - it->second.frame_last_used;
                if (dist > 100)
                {
                    texture_cleared++;
                    SDL_DestroyTexture(it->second.texture);
                    it = cache.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    // Widths
    int string_widths_cleared = 0;
    {
        std::unique_lock lock(length_cache_lock_);
        for (auto& [_, cache] : string_length_cache_)
        {
            for (auto it = cache.begin(); it != cache.end();)
            {
                const auto dist = current_frame - it->second.frame_last_used;
                if (dist > 1000)
                {
                    string_widths_cleared++;
                    it = cache.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }
}

std::string font_manager_t::escape_color(std::string_view text, SDL_Color* color)
{
    ZoneScoped;
    std::string result;
    result.reserve(text.size());

    std::string::size_type pos;
    while ((pos = text.find('^')) != std::string_view::npos)
    {
        result.append(text.data(), pos);

        if (isdigit(text[pos + 1]))
        {
            if (color)
            {
                switch (text[pos + 1] - '0')
                {
                    case 0:
                        color->r = 0x00;
                        color->g = 0x00;
                        color->b = 0x00;
                        color->a = 0xFF;
                        break;
                    case 1:
                        color->r = 0xFF;
                        color->g = 0x00;
                        color->b = 0x00;
                        color->a = 0xFF;
                        break;
                    case 2:
                        color->r = 0x00;
                        color->g = 0xFF;
                        color->b = 0x00;
                        color->a = 0xFF;
                        break;
                    case 3:
                        color->r = 0x00;
                        color->g = 0x00;
                        color->b = 0xFF;
                        color->a = 0xFF;
                        break;
                    case 4:
                        color->r = 0xFF;
                        color->g = 0xFF;
                        color->b = 0x00;
                        color->a = 0xFF;
                        break;
                    case 5:
                        color->r = 0xFF;
                        color->g = 0x00;
                        color->b = 0xFF;
                        color->a = 0xFF;
                        break;
                    case 6:
                        color->r = 0x00;
                        color->g = 0xFF;
                        color->b = 0xFF;
                        color->a = 0xFF;
                        break;
                    case 7:
                        color->r = 0xFF;
                        color->g = 0xFF;
                        color->b = 0xFF;
                        color->a = 0xFF;
                        break;
                    case 8:
                        color->r = 0xB3;
                        color->g = 0xB3;
                        color->b = 0xB3;
                        color->a = 0xFF;
                        break;
                    case 9:
                        color->r = 0x66;
                        color->g = 0x66;
                        color->b = 0x66;
                        color->a = 0xFF;
                        break;
                    default:
                        assert(false);
                        return "";
                }
            }
            text.remove_prefix(2 + pos);
        }
        else if (text[pos + 1] == 'x' || text[pos + 1] == 'X')
        {
            bool is_valid = true;
            for (int c = 0; c < 6; c++)
            {
                if (!isxdigit(text[pos + c + 2]))
                {
                    is_valid = false;
                    break;
                }
            }
            if (!is_valid)
            {
                return text.data();
            }
            if (color)
            {
                int xr, xg, xb;
                sscanf_s(text.data() + 2, "%2x%2x%2x", &xr, &xg, &xb);
                color->r = static_cast<Uint8>(xr);
                color->g = static_cast<Uint8>(xg);
                color->b = static_cast<Uint8>(xb);
                color->a = 0xFF;
            }
            text.remove_prefix(8 + pos);
        }
    }

    result.append(text.data());

    return result;
}

TTF_Font* font_manager_t::ensure_font(font_type font_type, int height)
{
    ZoneScoped;
    auto& font = fonts_[font_type][height];
    if (!font)
    {
        cb::sync_wait(
            [&]() -> cb::task<>
            {
                co_await state_t::instance->render_thread.schedule();
                const char* font_path = "VeraMono.ttf";
                if (font_type == font_type::LIB)
                {
                    font_path = "LiberationSans-Regular.ttf";
                }
                else if (font_type == font_type::LIB_BOLD)
                {
                    font_path = "LiberationSans-Bold.ttf";
                }
                font = TTF_OpenFont(font_path, height);
            }());
    }

    return font;
}

cb::task<draw_data> font_manager_t::render_font_to_texture(int left, int top, font_alignment align, TTF_Font* font,
                                                           std::string original_text)
{
    co_await state_t::instance->render_thread.schedule();
    ZoneScoped;

    draw_data result;
    result.is_valid = true;
    result.rect.x = left;
    result.rect.y = top;
    result.align = align;

    SDL_Color color;
    auto escaped_text = escape_color(original_text, &color);
    if (escaped_text.empty())
    {
        result.is_valid = false;
        co_return result;
    }

    {
        std::shared_lock lock(texture_cache_lock_);
        auto it = string_texture_cache_[font].find(escaped_text);
        if (it != string_texture_cache_[font].end())
        {
            result.texture = it->second.texture;
            result.rect.w = it->second.w;
            result.rect.h = it->second.h;
            it->second.frame_last_used = state_t::instance->current_frame;
            co_return result;
        }
    }

    auto surface = TTF_RenderText_Solid(font, escaped_text.c_str(), color);

    if (!surface)
    {
        printf("Cannot render text: %s with error: %s\n", escaped_text.c_str(), TTF_GetError());
        co_return {false};
    }
    result.rect.w = surface->w;
    result.rect.h = surface->h;
    // We are on the main thread here so we can render the font
    result.texture = SDL_CreateTextureFromSurface(state_t::instance->render_state.renderer, surface);

    SDL_FreeSurface(surface);

    // Put into cache
    {
        std::unique_lock lock(texture_cache_lock_);
        string_texture_cache_[font][escaped_text] = {result.texture, result.rect.w, result.rect.h,
                                                     state_t::instance->current_frame};
    }

    co_return result;
}
