#define ULOG_IMPLEMENTATION
#include "ulog.h"


#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define LUA_RET_CHECK_VALUE(L, ret) \
if (ret) { \
        lua_pushnil(L); \
        lua_pushstring(L, strerror(ret)); \
        return 2; \
} \
    struct __hack


#define LUA_RET_BOOLEAN(L, ret) \
if (ret) { \
        lua_pushboolean(L, 0); \
        lua_pushstring(L, strerror(ret)); \
        return 2; \
} else { \
        lua_pushboolean(L, 1); \
        return 1; \
} \
    struct __hack

static int lua_ulog_output(lua_State *L)
{
    int level = luaL_checkinteger(L, 1);
    luaL_argcheck(L, (level >= ULOG_LL_DEBUG && level < ULOG_LL__COUNT), 1, "invalid level");
    const char *msg = luaL_checkstring(L, 2);

    const char *file = NULL;
    int line = 0;
    const char *func = NULL;
    lua_Debug ar;
    if (lua_getstack(L, 2, &ar)) {
        lua_getinfo(L, "nSl", &ar);
        file = ar.short_src ?: "<lua>";
        line = ar.currentline;
        func = ar.name ?: "<main>";
    }
    ulog_output(level, file, line, func, msg);
    return 0;
}

static int lua_ulog_set_level(lua_State *L)
{
    int level = -1;
    if (lua_isinteger(L, 1)) {
        level = lua_tointeger(L, 1);
    } else if(lua_isstring(L, 1)) {
        const char *s = lua_tostring(L, 1);
        if (strcasecmp(s, "debug") == 0) {
            level = 0;
        } else if (strcasecmp(s, "info") == 0) {
            level = 1;
        } else if (strcasecmp(s, "notice") == 0) {
            level = 2;
        } else if (strcasecmp(s, "warn") == 0 || strcasecmp(s, "warning") == 0) {
            level = 3;
        } else if (strcasecmp(s, "error") == 0) {
            level = 4;
        }
    }
    luaL_argcheck(L, level >= ULOG_LL_DEBUG && level <= ULOG_LL__COUNT, 1, "invalid level(0,1,2,3,4)");
    int ret = ulog_set_level(level);
    LUA_RET_BOOLEAN(L, ret);
}

static int lua_ulog_set_timefmt(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    int fmt = -1;
    if (strcasecmp(str, "longdate") == 0) {
        fmt = ULOG_TIME_LONGDATE;
    } else if (strcasecmp(str, "shortdate") == 0) {
        fmt = ULOG_TIME_SHORTDATE;
    } else if (strcasecmp(str, "timeonly") == 0) {
        fmt = ULOG_TIME_TIMEONLY;
    } else if (strcasecmp(str, "mono") == 0) {
        fmt = ULOG_TIME_MONO;
    } else {
        luaL_argerror(L, 1, "uknown timefmt(longdate,shortdate,timeonly,mono)");
    }
    int ret = ulog_set_timefmt(fmt);
    LUA_RET_BOOLEAN(L, ret);
}

static int lua_ulog_set_srcfmt(lua_State *L)
{
    const char *str = luaL_checkstring(L, 1);
    int fmt = -1;
    if (strcasecmp(str, "long") == 0) {
        fmt = ULOG_SRC_LONG;
    } else if (strcasecmp(str, "short") == 0) {
        fmt = ULOG_SRC_SHORT;
    } else if (strcasecmp(str, "full") == 0) {
        fmt = ULOG_SRC_FULL;
    } else if (strcasecmp(str, "none") == 0) {
        fmt = ULOG_SRC_NONE;
    } else {
        luaL_argerror(L, 1, "uknown srcfmt(long,short,full,none)");
    }
    int ret = ulog_set_srcfmt(fmt);
    LUA_RET_BOOLEAN(L, ret);
}

static int lua_ulog_tofile(lua_State *L)
{
    const char *newfile = luaL_optstring(L, 1, NULL);
    int ret = ulog_tofile(newfile);
    LUA_RET_BOOLEAN(L, ret);
}

static int lua_ulog_getfile(lua_State *L)
{
    luaL_Buffer b;
    char *buff = luaL_buffinitsize(L, &b, PATH_MAX);
    if (ulog_getfile(buff, PATH_MAX)) {
        luaL_pushresultsize(&b, strlen(buff));
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

static int lua_ulog_size(lua_State *L)
{
    long size;
    int ret = ulog_size(&size);
    LUA_RET_CHECK_VALUE(L, ret);
    lua_pushinteger(L, size);
    return 1;
}

static int lua_ulog_flush(lua_State *L)
{
    ulog_flush();
    return 0;
}

static const luaL_Reg sysx_ulog_methods[] = {
    {"flush",       lua_ulog_flush },
    {"size",        lua_ulog_size },
    {"getfile",     lua_ulog_getfile },
    {"tofile",      lua_ulog_tofile },
    {"set_level",   lua_ulog_set_level },
    {"set_timefmt", lua_ulog_set_timefmt },
    {"set_srcfmt",  lua_ulog_set_srcfmt },
    {NULL, NULL}
};

#define LUA_SCRIPT(...) #__VA_ARGS__

static const char *ulog_lua_wrapper = LUA_SCRIPT(
    local raw_output = ...
    local M = {}
    local function make_log(level)
        return function(fmt, ...)
            local msg = (...) and string.format(fmt, ...) or fmt
            raw_output(level, msg)
        end
    end
    M.debug  = make_log(0)
    M.info   = make_log(1)
    M.notice = make_log(2)
    M.warn   = make_log(3)
    M.error  = make_log(4)
    return M
);

int luaopen_ulog(lua_State *L)
{
    if (luaL_loadstring(L, ulog_lua_wrapper) != LUA_OK) {
        return lua_error(L);
    }
    lua_pushcfunction(L, lua_ulog_output);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        return lua_error(L);
    }
    luaL_setfuncs(L, sysx_ulog_methods, 0);
    return 1;
}

