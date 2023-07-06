# nlohmann-json is a custom addition to the emscripten SDK, so it is possible
# someone patched their SDK. Test out if the SDK supports nlohmann-json.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-sUSE_NLOHMANN_JSON=1")

check_cxx_source_compiles("
    #include <nlohmann/json.hpp>
    int main() { return 0; }"
    nlohmann_json_FOUND
)

if (nlohmann_json_FOUND)
        add_library(nlohmann_json INTERFACE IMPORTED)
        set_target_properties(nlohmann_json PROPERTIES
                INTERFACE_COMPILE_OPTIONS "-sUSE_NLOHMANN_JSON=1"
                INTERFACE_LINK_LIBRARIES "-sUSE_NLOHMANN_JSON=1"
        )
else()
        message(WARNING "You are using an emscripten SDK without nlohmann-json support. Please apply 'emsdk-nlohmann_json.patch' to your local emsdk installation.")
endif()
