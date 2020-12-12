add_subdirectory(api)

if(OPTION_TOOLS_ONLY)
    return()
endif()

add_files(
    script_config.cpp
    script_config.hpp
    script_fatalerror.hpp
    script_info.cpp
    script_info.hpp
    script_info_dummy.cpp
    script_instance.cpp
    script_instance.hpp
    script_scanner.cpp
    script_scanner.hpp
    script_storage.hpp
    script_suspend.hpp
    squirrel.cpp
    squirrel.hpp
    squirrel_class.hpp
    squirrel_helper.hpp
    squirrel_helper_type.hpp
    squirrel_std.cpp
    squirrel_std.hpp
)
