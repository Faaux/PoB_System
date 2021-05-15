#pragma once
#include <SDL.h>
#include <pob_system/image.h>
#include <pob_system/lua_helper.h>
#include <tasks/static_thread_pool.h>
#include <tasks/task.h>
#include <pob_system/commands/viewport_command.h>

#include <cstdint>
#include <string>
#include <vector>

#include <pob_system/draw_layer.h>

#include "commands/command_list.h"

// Forward Declare
struct lua_State;
struct SDL_Window;
struct SDL_Renderer;

class state_t;

// All lua specific state with a pointer to application state
// Is also the home for all API callbacks
class lua_state_t
{
    friend std::vector<lua_value> pop_save_values(lua_State*, int);

   public:
    lua_state_t(state_t* state);
    ~lua_state_t();

    void do_file(const char* file);

    void checkSubPrograms();
    int get_id() const { return id; }
    void clear_command_list() const { command_list_t->clear(); }

    // Helpers to call into lua
    void on_init();
    void on_frame();
    void on_char(char c);
    void on_key_down(SDL_Keycode key);
    void on_key_up(SDL_Keycode key);
    void on_mouse_down(int mb, bool double_click);
    void on_mouse_up(int mb);
    void on_exit();
    bool can_exit();

   private:
    // Constants
    inline static const char* IMAGE_META_HANDLE = "uiimghandlemeta";

    // Helper
    void assert(bool cond, const char* fmt, ...) const;
    bool is_image_handle(int index) const;
    static lua_state_t* get_current_state(lua_State* l);
    ImageHandle& get_image_handle(int index) const;

    // Callbacks
    int set_window_title();
    int set_main_object();
    int get_time();
    int render_init();
    int con_print_f();
    int load_module();
    int p_load_module();
    int p_call();
    int launch_sub_script();
    int get_script_path();
    int get_runtime_path();
    int get_user_path();
    int make_dir();
    int screen_size();
    int is_key_down_callback();
    int cursor_pos();
    int copy();
    int paste();

    int set_draw_layer();
    int set_viewport();

    // Image Handling
    int new_image_handle();
    int img_handle_gc(ImageHandle& handle);
    int img_handle_load(ImageHandle& handle);
    int img_handle_is_valid(ImageHandle& handle);
    int img_handle_is_loading(ImageHandle& handle);
    int img_handle_set_loading_priority(ImageHandle& handle);
    int img_handle_image_size(ImageHandle& handle);
    int dummy();

    // Internal Helper to access the main lua table
    void pushMainObjectOntoStack();
    void pushCallableOntoStack(const char* name);
    void callParameterlessFunction(const char* name);
    void log_lua_error();

    // SubScript Helpers
    int call_main_from_sub(bool sync);

    int id;
    state_t* state;
    lua_State* l;
    draw_layer_t draw_layer;
    int main_object_index = -1;
    std::string user_path;

    command_list* command_list_t;
};

struct render_state_t
{
    ~render_state_t();

    void init();
    bool is_init = false;
    std::string window_title;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
};

// The global state of the application.
// For testing we do not create a singleton, we do however allocate an instance on startup and make it
// accessible via a static var
class state_t
{
   public:
    state_t(int argc, char* argv[]) : argc(argc), argv(argv), lua_state(this) {}
    int argc;
    char** argv;
    // No getters and setters, know what you change
    lua_state_t lua_state;
    render_state_t render_state;
    cb::static_thread_pool global_thread_pool{std::thread::hardware_concurrency() - 1};
    cb::static_thread_pool main_lua_thread{1};

    static state_t* instance;
};