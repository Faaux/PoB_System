#include <SDL.h>
#include <pob_system/lua_helper.h>
#include <pob_system/state.h>

#include <chrono>
#include <filesystem>
#include <lua.hpp>

lua_state_t::lua_state_t(state_t* state) : state(state)
{
    static int lua_id = 1;
    id = lua_id++;

    l = luaL_newstate();

    lua_pushlightuserdata(l, (void*)this);
    lua_rawseti(l, LUA_REGISTRYINDEX, 0);
    lua_pushcfunction(l, traceback);
    lua_pushvalue(l, -1);
    lua_setfield(l, LUA_REGISTRYINDEX, "traceback");

    luaL_openlibs(l);

    {
        lua_getglobal(l, "package");
        lua_getfield(l, -1, "path");                 // get field "path" from table at top of stack (-1)
        std::string cur_path = lua_tostring(l, -1);  // grab path string from top of stack
        auto cwd = std::filesystem::current_path().string();
        cur_path.append(";" + cwd + "\\lua\\?.lua");
        cur_path.append(";" + cwd + "\\lua\\?\\init.lua");
        cur_path.append(";" + cwd + "?.lua");
        cur_path.append(";" + cwd + "?\\init.lua");
        lua_pop(l, 1);                        // get rid of the string on the stack we just pushed on line 5
        lua_pushstring(l, cur_path.c_str());  // push the new one
        lua_setfield(l, -2, "path");          // set the field "path" in table at -2 with value at top of stack
        lua_pop(l, 1);                        // get rid of package table from top of stack
    }

    // Register callbacks

    // -- Global Functions

#define LUA_GLOBAL_FUNCTION(luaName, funcName)                                                  \
    lua_pushcfunction(l, [](lua_State* l) -> int { return get_current_state(l)->funcName(); }); \
    lua_setglobal(l, #luaName);

    LUA_GLOBAL_FUNCTION(SetWindowTitle, set_window_title);
    LUA_GLOBAL_FUNCTION(SetMainObject, set_main_object);
    LUA_GLOBAL_FUNCTION(GetTime, get_time);
    LUA_GLOBAL_FUNCTION(RenderInit, render_init);
    LUA_GLOBAL_FUNCTION(ConPrintf, con_print_f);
    LUA_GLOBAL_FUNCTION(LoadModule, load_module);
    LUA_GLOBAL_FUNCTION(PLoadModule, p_load_module);
    LUA_GLOBAL_FUNCTION(PCall, p_call);
    LUA_GLOBAL_FUNCTION(LaunchSubScript, launch_sub_script);
    LUA_GLOBAL_FUNCTION(GetScriptPath, get_script_path);
    LUA_GLOBAL_FUNCTION(GetRuntimePath, get_runtime_path);
    LUA_GLOBAL_FUNCTION(MakeDir, make_dir);
    LUA_GLOBAL_FUNCTION(GetScreenSize, screen_size);
#undef LUA_GLOBAL_FUNCTION

    // -- Class Like
    // -- -- Image
    lua_newtable(l);       // Image handle metatable
    lua_pushvalue(l, -1);  // Push image handle metatable

    lua_pushcclosure(
        l, [](lua_State* l) -> int { return get_current_state(l)->new_image_handle(); }, 1);
    lua_setglobal(l, "NewImageHandle");
    lua_pushvalue(l, -1);  // Push image handle metatable
    lua_setfield(l, -2, "__index");

#define LUA_IMAGE_FUNCTION(funcName)                                                                                  \
    lua_pushcfunction(l,                                                                                              \
                      [](lua_State* l) -> int                                                                         \
                      {                                                                                               \
                          auto state = get_current_state(l);                                                          \
                          state->assert(state->is_image_handle(1), "imgHandle:%s() must be used on an image handle"); \
                          return state->funcName(state->get_image_handle(1));                                         \
                      });

    LUA_IMAGE_FUNCTION(img_handle_gc);
    lua_setfield(l, -2, "__gc");
    LUA_IMAGE_FUNCTION(img_handle_load);
    lua_setfield(l, -2, "Load");
    LUA_IMAGE_FUNCTION(img_handle_is_valid);
    lua_setfield(l, -2, "IsValid");
    LUA_IMAGE_FUNCTION(img_handle_is_loading);
    lua_setfield(l, -2, "IsLoading");
    LUA_IMAGE_FUNCTION(img_handle_image_size);
    lua_setfield(l, -2, "ImageSize");
    lua_setfield(l, LUA_REGISTRYINDEX, IMAGE_META_HANDLE);

#undef LUA_IMAGE_FUNCTION

    // -- Stubs
#define STUB(n)                             \
    lua_pushcfunction(l,                    \
                      [](lua_State*) -> int \
                      {                     \
                          printf(n "\n");   \
                          return 0;         \
                      });                   \
    lua_setglobal(l, n);

    STUB("SetDrawLayer");
    STUB("SetViewport");
    STUB("SetDrawColor");
    STUB("DrawImage");
    STUB("DrawStringWidth");
    STUB("ConExecute");

#undef STUB

    // Push args
    if (state)
    {
        for (int i = 0; i < state->argc; i++)
        {
            lua_pushstring(l, state->argv[i]);
        }
        lua_createtable(l, state->argc - 1, 1);
        for (int i = 0; i < state->argc; i++)
        {
            lua_pushstring(l, state->argv[i]);
            lua_rawseti(l, -2, i);
        }
        lua_setglobal(l, "arg");
    }
}

lua_state_t::~lua_state_t() { lua_close(l); }

void lua_state_t::do_file(const char* file)
{
    [&]() -> cb::task<>
    {
        co_await state->main_lua_thread.schedule();
        if (luaL_dofile(l, file))
        {
            logLuaError();
        }
    }()
                 .join();
}

void lua_state_t::checkSubPrograms()
{
    auto subs = std::move(sub_programs);
    for (auto&& task : subs)
    {
        if (task.await_ready())
        {
            auto sub_state = task.join();
            auto sub_id = sub_state->get_id();
            // ToDo Finish SubScript here
        }
        else
        {
            // Task not yet ready, push to running sub programs
            sub_programs.push_back(std::move(task));
        }
    }
}

void lua_state_t::onInit()
{
    // Run this on the main lua thread and wait for it to finish
    // We do this because sub-scripts can also schedule on this thread to e.g update the ui
    [&]() -> cb::task<>
    {
        co_await state->main_lua_thread.schedule();
        callParameterlessFunction("OnInit");
    }()
                 .join();
}

void lua_state_t::onFrame()
{
    [&]() -> cb::task<>
    {
        co_await state->main_lua_thread.schedule();
        callParameterlessFunction("OnFrame");
    }()
                 .join();
}

void lua_state_t::assert(bool cond, const char* fmt, ...) const
{
    if (!cond)
    {
        va_list va;
        va_start(va, fmt);
        lua_pushvfstring(l, fmt, va);
        va_end(va);
        lua_error(l);
    }
}

lua_state_t* lua_state_t::get_current_state(lua_State* l)
{
    lua_rawgeti(l, LUA_REGISTRYINDEX, 0);
    auto state = (lua_state_t*)lua_touserdata(l, -1);
    lua_pop(l, 1);
    return state;
}

bool lua_state_t::is_image_handle(int index) const
{
    if (lua_type(l, index) != LUA_TUSERDATA || lua_getmetatable(l, index) == 0)
    {
        return false;
    }
    lua_getfield(l, LUA_REGISTRYINDEX, IMAGE_META_HANDLE);
    int ret = lua_rawequal(l, -2, -1);
    lua_pop(l, 2);
    return ret;
}

ImageHandle& lua_state_t::get_image_handle(int index) const
{
    auto handle = static_cast<ImageHandle*>(lua_touserdata(l, index));
    lua_remove(l, 1);
    return *handle;
}

int lua_state_t::set_window_title()
{
    luaL_checktype(l, 1, LUA_TSTRING);
    if (!state)
    {
        fprintf(stderr, "Can not set window_state with no valid state");
        return 0;
    }

    state->render_state.window_title = lua_tostring(l, 1);
    return 0;
}

int lua_state_t::set_main_object()
{
    luaL_checktype(l, 1, LUA_TTABLE);

    // Create table in registry
    lua_pushlightuserdata(l, (void*)this); /* push address */
    lua_newtable(l);
    lua_settable(l, LUA_REGISTRYINDEX);

    // Read table from registry
    lua_pushlightuserdata(l, (void*)this); /* push address */
    lua_gettable(l, LUA_REGISTRYINDEX);

    // Swap stack so that the table is at 1 and argument at 2
    lua_insert(l, 1);
    main_object_index = luaL_ref(l, 1);
    lua_pop(l, 1);
    return 0;
}

int lua_state_t::get_time()
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch());
    lua_pushnumber(l, static_cast<double>(ms.count()));
    return 1;
}

int lua_state_t::render_init()
{
    if (!state)
    {
        fprintf(stderr, "Can not create rendersystem with no valid state");
        return 0;
    }
    state->render_state.init();
    return 0;
}

int lua_state_t::con_print_f()
{
    int n = lua_gettop(l);

    lua_getglobal(l, "string");
    lua_getfield(l, -1, "format");
    lua_insert(l, 1);
    lua_pop(l, 1);

    // Call on frame
    if (lua_pcall(l, n, 1, 0))
    {
        logLuaError();
        return 0;
    }
    printf("%s\n", luaL_checkstring(l, 1));

    return 0;
}

int lua_state_t::load_module()
{
    int n = lua_gettop(l);
    assert(n >= 1 && lua_isstring(l, 1), "LoadModule wrong call.");

    std::string fileName(lua_tostring(l, 1));
    constexpr std::string_view ext(".lua");
    if (!std::equal(ext.rbegin(), ext.rend(), fileName.rbegin()))
    {
        fileName = fileName + ".lua";
    }

    int err = luaL_loadfile(l, fileName.c_str());
    assert(err == 0, "LoadModule() error loading '%s':\n%s", fileName.c_str(), lua_tostring(l, -1));
    lua_replace(l, 1);  // Replace module name with module main chunk
    lua_call(l, n - 1, LUA_MULTRET);
    return lua_gettop(l);
}

int lua_state_t::p_load_module()
{
    int n = lua_gettop(l);
    assert(n >= 1, "Usage: PLoadModule(name[, ...])");
    assert(lua_isstring(l, 1), "PLoadModule() argument 1: expected string, got %t", 1);
    std::string fileName(lua_tostring(l, 1));
    constexpr std::string_view ext(".lua");
    if (!std::equal(ext.rbegin(), ext.rend(), fileName.rbegin()))
    {
        fileName = fileName + ".lua";
    }
    int err = luaL_loadfile(l, fileName.c_str());
    if (err)
    {
        return 1;
    }
    lua_replace(l, 1);  // Replace module name with module main chunk
    err = lua_pcall(l, n - 1, LUA_MULTRET, 0);
    if (err)
    {
        return 1;
    }
    lua_pushnil(l);
    lua_insert(l, 1);
    //    lua_replace(L, 1); // Replace traceback function with nil
    return lua_gettop(l);
}

int lua_state_t::p_call()
{
    int n = lua_gettop(l);
    assert(n >= 1, "Usage: PCall(func[, ...])");
    assert(lua_isfunction(l, 1), "PCall() argument 1: expected function, got %t", 1);
    lua_getfield(l, LUA_REGISTRYINDEX, "traceback");
    lua_insert(l, 1);  // Insert traceback function at start of stack
    int err = lua_pcall(l, n - 1, LUA_MULTRET, 1);
    if (err)
    {
        lua_error(l);
        return 1;
    }
    lua_pushnil(l);
    lua_replace(l, 1);  // Replace traceback function with nil
    return lua_gettop(l);
}

int lua_state_t::launch_sub_script()
{
    int sub_id = -1;
    sub_programs.emplace_back(
        [&]() -> cb::task<std::shared_ptr<lua_state_t>, true>
        {
            int n = lua_gettop(l);
            assert(n >= 3, "Usage: LaunchSubScript(scriptText, funcList, subList[, ...])");
            for (int i = 1; i <= 3; i++)
            {
                assert(lua_isstring(l, i), "LaunchSubScript() argument %d: expected string, got %t", i, i);
            }
            for (int i = 4; i <= n; i++)
            {
                assert(lua_isnil(l, i) || lua_isboolean(l, i) || lua_isnumber(l, i) || lua_isstring(l, i),
                       "LaunchSubScript() argument %d: only nil, boolean, number and string types can be passed to sub "
                       "script",
                       i);
            }

            auto sub_state = std::make_shared<lua_state_t>(nullptr);
            sub_id = sub_state->get_id();
            auto& sub = *sub_state;

            int argsCount = n - 3;

            const char* script = lua_tostring(l, 1);
            luaL_loadstring(sub.l, script);
            lua_xmove(l, sub.l, argsCount);

            // Use string here to copy the value since it can be GCed otherwise.
            std::string sync_override_calls = lua_tostring(l, 2);   // Sync call to main with return
            std::string async_override_calls = lua_tostring(l, 3);  // Async call to main

            // Put us on another thread, do not use any captured reference beyond this point
            co_await state->global_thread_pool.schedule();

            // Override global functions that should be called on the main thread
            {
                std::string token;
                std::istringstream tokenStream(sync_override_calls);
                while (std::getline(tokenStream, token, ','))
                {
                    lua_pushstring(sub.l, token.c_str());
                    lua_pushcclosure(
                        sub.l,
                        [](lua_State* local) -> int { return get_current_state(local)->call_main_from_sub(true); }, 1);
                    lua_setglobal(sub.l, token.c_str());
                }
            }

            {
                std::string token;
                std::istringstream tokenStream(async_override_calls);
                while (std::getline(tokenStream, token, ','))
                {
                    lua_pushstring(sub.l, token.c_str());
                    lua_pushcclosure(
                        sub.l,
                        [](lua_State* local) -> int { return get_current_state(local)->call_main_from_sub(false); }, 1);
                    lua_setglobal(sub.l, token.c_str());
                }
            }

            // On the other thread start the lua script
            if (lua_pcall(sub.l, argsCount, LUA_MULTRET, 0))
            {
                sub.logLuaError();
            }

            co_return sub_state;
        }());

    lua_pushinteger(l, sub_id);

    return 1;
}

int lua_state_t::get_script_path()
{
    lua_pushstring(l, std::filesystem::current_path().string().c_str());
    return 1;
}
int lua_state_t::get_runtime_path()
{
    lua_pushstring(l, std::filesystem::current_path().string().c_str());
    return 1;
}
int lua_state_t::make_dir()
{
    int n = lua_gettop(l);
    assert(n >= 1, "Usage: MakeDir(path)");
    assert(lua_isstring(l, 1), "MakeDir() argument 1: expected string, got %t", 1);

    std::error_code ec;
    if (!std::filesystem::create_directory(lua_tostring(l, 1), ec))
    {
        lua_pushnil(l);
        lua_pushstring(l, ec.message().c_str());
        return 2;
    }
    else
    {
        lua_pushboolean(l, true);
        return 1;
    }
}
int lua_state_t::screen_size()
{
    assert(state, "Can only be called from the main thread");
    int w, h;
    SDL_GetWindowSize(state->render_state.window, &w, &h);

    lua_pushinteger(l, w);
    lua_pushinteger(l, h);
    return 2;
}
int lua_state_t::new_image_handle()
{
    ImageHandle* imgHandle = (ImageHandle*)lua_newuserdata(l, sizeof(ImageHandle));
    new (imgHandle) ImageHandle();
    lua_pushvalue(l, lua_upvalueindex(1));
    lua_setmetatable(l, -2);
    return 1;
}
int lua_state_t::img_handle_gc(ImageHandle& handle)
{
    handle.~ImageHandle();
    return 0;
}
int lua_state_t::img_handle_load(ImageHandle& handle)
{
    int n = lua_gettop(l);
    assert(n >= 1, "Usage: imgHandle:Load(fileName[, flag1[, flag2...]])");
    assert(lua_isstring(l, 1), "imgHandle:Load() argument 1: expected string, got %t", 1);

    const char* fileName = lua_tostring(l, 1);
    bool mipmaps = true;
    bool async = false;
    bool clamp = false;
    for (int f = 2; f <= n; f++)
    {
        if (!lua_isstring(l, f))
        {
            continue;
        }
        const char* flag = lua_tostring(l, f);
        if (flag == Image::ASYNC_FLAG)
        {
            async = true;
        }
        else if (flag == Image::CLAMP_FLAG)
        {
            clamp = true;
        }
        else if (flag == Image::MIPMAP_FLAG)
        {
            mipmaps = false;
        }
        else
        {
            assert(false, "imgHandle:Load(): unrecognised flag '%s'", flag);
        }
    }
    handle.image = std::make_unique<Image>(fileName, mipmaps, clamp, async);

    return 0;
}

int lua_state_t::img_handle_is_valid(ImageHandle& handle)
{
    lua_pushboolean(l, static_cast<bool>(handle.image));
    return 1;
}

int lua_state_t::img_handle_is_loading(ImageHandle& handle)
{
    lua_pushboolean(l, handle.image->is_loading());
    return 1;
}

int lua_state_t::img_handle_image_size(ImageHandle& handle)
{
    lua_pushinteger(l, handle.image->width());
    lua_pushinteger(l, handle.image->height());
    return 2;
}

int lua_state_t::dummy()
{
    dumpstack(l);
    return 0;
}

void lua_state_t::pushMainObjectOntoStack()
{
    lua_pushlightuserdata(l, (void*)this);
    lua_gettable(l, LUA_REGISTRYINDEX);

    // Read Main object table
    lua_pushinteger(l, main_object_index);
    lua_gettable(l, -2);
    lua_remove(l, -2);
}

void lua_state_t::pushCallableOntoStack(const char* name)
{
    pushMainObjectOntoStack();

    // Get function
    lua_pushstring(l, name);
    lua_gettable(l, -2);

    // Table has to be first argument
    lua_insert(l, -2);
}

void lua_state_t::callParameterlessFunction(const char* name)
{
    pushCallableOntoStack(name);

    // Call on frame
    if (lua_pcall(l, 1, 0, 0))
    {
        logLuaError();
        lua_settop(l, 0);
        return;
    }

    // clear stack
    lua_settop(l, 0);
}

void lua_state_t::logLuaError() { fprintf(stderr, "%d: %s\n", id, lua_tostring(l, -1)); }

int lua_state_t::call_main_from_sub(bool sync)
{
    const char* name = lua_tostring(l, lua_upvalueindex(1));

    // Schedule call and wait for it
    auto task = [](lua_State* l, const char* name, bool sync) -> cb::task<std::vector<lua_value> >
    {
        std::vector<lua_value> values = pop_save_values(l, 1);
        std::string funcName = name;

        co_await state_t::instance->main_lua_thread.schedule();
        auto main_l = state_t::instance->lua_state.l;
        int ret_n = lua_gettop(main_l) + 1;

        // Build Function call
        state_t::instance->lua_state.pushCallableOntoStack("OnSubCall");
        lua_pushstring(main_l, funcName.c_str());

        push_saved_values(main_l, values);

        if (lua_pcall(main_l, static_cast<int>(values.size() + 2), LUA_MULTRET, 0))
        {
            state_t::instance->lua_state.logLuaError();
        }

        std::vector<lua_value> return_values;
        if (sync)
        {
            // Get return values
            int n = lua_gettop(main_l);
            for (int i = ret_n; i <= n; i++)
            {
                state_t::instance->lua_state.assert(lua_isnil(main_l, i) || lua_isboolean(main_l, i) ||
                                                        lua_isnumber(main_l, i) || lua_isstring(main_l, i),
                                                    "OnSubCall() return %d: only nil, boolean, number and "
                                                    "string can be returned to sub script",
                                                    i - ret_n + 1);
            }

            return_values = pop_save_values(main_l, ret_n);
        }
        co_return return_values;
    }(l, name, sync);

    if (sync)
    {
        auto return_values = task.join();
        push_saved_values(l, return_values);
        return static_cast<int>(return_values.size());
    }

    return 0;
}

render_state_t::~render_state_t()
{
    if (renderer)
    {
        SDL_DestroyRenderer(renderer);
    }
    if (window)
    {
        SDL_DestroyWindow(window);
    }
}

void render_state_t::init()
{
    if (is_init)
        return;
    // TODO: Safe and load position size etc.

    window = SDL_CreateWindow(window_title.c_str(),
                              SDL_WINDOWPOS_UNDEFINED,  // initial x position
                              SDL_WINDOWPOS_UNDEFINED,  // initial y position
                              640,                      // width, in pixels
                              480,                      // height, in pixels
                              SDL_WINDOW_OPENGL         // flags - see below
    );

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr)
    {
        fprintf(stderr, "Could not create a renderer: %s", SDL_GetError());
    }

    is_init = true;
}

state_t* state_t::instance = nullptr;
