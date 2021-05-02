#include <pob_system/lua_helper.h>
#include <pob_system/state.h>

#include <lua.hpp>

static void printValueAtIndex(lua_State* L, int i)
{
    switch (lua_type(L, i))
    {
        case LUA_TNUMBER:
            printf("%g\n", lua_tonumber(L, i));
            break;
        case LUA_TSTRING:
            printf("%s\n", lua_tostring(L, i));
            break;
        case LUA_TBOOLEAN:
            printf("%s\n", (lua_toboolean(L, i) ? "true" : "false"));
            break;
        case LUA_TNIL:
            printf("%s\n", "nil");
            break;
        default:
            printf("%p\n", lua_topointer(L, i));
            break;
    }
}

void dumpstack(lua_State* L)
{
    int top = lua_gettop(L);
    for (int i = 1; i <= top; i++)
    {
        printf("%d\t%s\t", i, luaL_typename(L, i));
        printValueAtIndex(L, i);
    }
    printf("----- End of Stack\n");
}

void dumptable(lua_State* L, int tableIndex)
{
    lua_pushnil(L); /* first key */
    while (lua_next(L, tableIndex) != 0)
    {
        printf("Key: %s\t", luaL_typename(L, -2));
        printValueAtIndex(L, -2);
        printf("Value: %s\t", luaL_typename(L, -1));
        printValueAtIndex(L, -1);

        lua_pop(L, 1);
    }
    printf("----- End of Table\n");
}

int traceback(lua_State* L)
{
    if (!lua_isstring(L, 1)) /* 'message' not a string? */
        return 1;            /* keep it intact */
    lua_getglobal(L, "debug");
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        return 1;
    }
    lua_getfield(L, -1, "traceback");
    if (!lua_isfunction(L, -1))
    {
        lua_pop(L, 2);
        return 1;
    }
    lua_pushvalue(L, 1);   /* pass error message */
    lua_pushinteger(L, 2); /* skip this function and traceback */
    lua_call(L, 2, 1);     /* call debug.traceback */
    return 1;
}

std::vector<lua_value> pop_save_values(lua_State* l, int start)
{
    int n = lua_gettop(l);
    auto state = lua_state_t::get_current_state(l);
    for (int i = start; i <= n; i++)
    {
        state->assert(lua_isnil(l, i) || lua_isboolean(l, i) || lua_isnumber(l, i) || lua_isstring(l, i),
                      "argument %d: only nil, boolean, number and string can be passed to the main script", i);
    }

    std::vector<lua_value> values;
    for (int i = start; i <= n; i++)
    {
        auto& data = values.emplace_back();
        switch (lua_type(l, i))
        {
            case LUA_TNIL:
                data.type = lua_value::NIL;
                break;
            case LUA_TBOOLEAN:
                data.type = lua_value::BOOLEAN;
                data.b = (lua_toboolean(l, i) != 0);
                break;
            case LUA_TNUMBER:
                data.type = lua_value::NUMBER;
                data.n = lua_tonumber(l, i);
                break;
            case LUA_TSTRING:
                data.type = lua_value::STRING;
                data.s = lua_tostring(l, i);
                break;
        }
    }
    lua_settop(l, start - 1);
    return values;
}

int push_saved_values(lua_State* l, const std::vector<lua_value>& values)
{
    for (auto& value : values)
    {
        switch (value.type)
        {
            case lua_value::NIL:
            {
                lua_pushnil(l);
                break;
            }
            case lua_value::BOOLEAN:
            {
                lua_pushboolean(l, value.b);
                break;
            }
            case lua_value::NUMBER:
            {
                lua_pushnumber(l, value.n);
                break;
            }
            case lua_value::STRING:
            {
                lua_pushstring(l, value.s.c_str());
                break;
            }
            default:
            {
                __debugbreak();
            }
        }
    }

    return static_cast<int>(values.size());
}