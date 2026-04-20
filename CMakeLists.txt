
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUA REQUIRED "lua >= 5.3")
include_directories(${LUA_INCLUDE_DIRS})
link_directories(${LUA_LIBRARY_DIRS})

# static library
add_library(ulog_static STATIC ulog.c)
target_link_libraries(ulog_static PRIVATE
    ${LUA_LIBRARIES}
)

set_target_properties(ulog_static PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# lua module
add_library(lua-ulog MODULE ulog.c)
target_link_libraries(lua-ulog PRIVATE
    ${LUA_LIBRARIES}
)
set_target_properties(lua-ulog PROPERTIES
    OUTPUT_NAME "ulog"
    PREFIX ""
    POSITION_INDEPENDENT_CODE ON
)

