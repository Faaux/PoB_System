#pragma once
#include <SDL.h>
#include <pob_system/commands/viewport_command.h>
#include <pob_system/draw_layer.h>
#include <pob_system/font_rendering.h>
#include <pob_system/image.h>
#include <pob_system/lua_helper.h>
#include <pob_system/work_pool.h>
#include <tasks/static_thread_pool.h>
#include <tasks/task.h>

#include <cstdint>
#include <lua.hpp>
#include <string>
#include <vector>

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

    // Helpers to call into lua
    cb::task<> on_init();
    cb::task<> on_frame();
    void on_char(char c);
    void on_key_down(SDL_Keycode key);
    void on_key_up(SDL_Keycode key);
    void on_mouse_down(int mb, bool double_click);
    void on_mouse_up(int mb);
    void on_exit();
    bool can_exit();

    std::vector<draw_string_command> test_commands;

   private:
    // Constants
    inline static const char* IMAGE_META_HANDLE = "uiimghandlemeta";

    // Helper
    void assert_internal(bool cond, const char* fmt, ...) const;
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

    int string_width();
    int draw_string();

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
    template <bool SYNC>
    int call_main_from_sub();
    void force_exit();
    void cleanup_subtasks();

    struct sub_task_t
    {
        std::shared_ptr<lua_state_t> state;
        cb::task<> task;
    };
    std::vector<sub_task_t> sub_tasks;

    int id;
    state_t* state;
    lua_State* l;
    draw_layer_t draw_layer;
    int main_object_index = -1;
    std::string user_path;

    void append_cmd(viewport_command_t);
};

struct render_state_t
{
    ~render_state_t();

    void init();
    bool is_init = false;
    std::string window_title;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    font_manager_t font_manager;
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
    uint64_t current_frame = 0;
    lua_state_t lua_state;
    render_state_t render_state;
    cb::static_thread_pool global_thread_pool{4};
    cb::static_thread_pool main_lua_thread{1};
    work_pool render_thread;

    static state_t* instance;
};

template <bool SYNC>
inline int lua_state_t::call_main_from_sub()
{
    using return_type = std::conditional<SYNC, std::vector<lua_value>, void>::type;

    const char* name = lua_tostring(l, lua_upvalueindex(1));

    // Schedule call and wait for it
    auto task = [](lua_State* l, const char* name) -> cb::task<return_type>
    {
        std::vector<lua_value> values = pop_save_values(l, 1);
        std::string funcName = name;

        co_await state_t::instance->main_lua_thread.schedule();
        auto main_l = state_t::instance->lua_state.l;
        int ret_n = lua_gettop(main_l) + 1;

        // Build Function call
        state_t::instance->lua_state.pushCallableOntoStack("OnSubCall");
        lua_pushstring(main_l, funcName.c_str());

        int pushed_count = push_saved_values(main_l, values);

        if (lua_pcall(main_l, pushed_count + 2, LUA_MULTRET, 0))
        {
            state_t::instance->lua_state.log_lua_error();
        }

        if constexpr (SYNC)
        {
            // Get return values
            int n = lua_gettop(main_l);
            for (int i = ret_n; i <= n; i++)
            {
                state_t::instance->lua_state.assert_internal(lua_isnil(main_l, i) || lua_isboolean(main_l, i) ||
                                                                 lua_isnumber(main_l, i) || lua_isstring(main_l, i),
                                                             "OnSubCall() return %d: only nil, boolean, number and "
                                                             "string can be returned to sub script",
                                                             i - ret_n + 1);
            }

            co_return pop_save_values(main_l, ret_n);
        }
        else
        {
            (void)ret_n;
        }
    }(l, name);

    if constexpr (SYNC)
    {
        auto return_values = cb::sync_wait(task);
        return push_saved_values(l, return_values);
    }
    else
    {
        sub_task_t sub;
        sub.task = std::move(task);
        sub_tasks.push_back(std::move(sub));

        return 0;
    }
}