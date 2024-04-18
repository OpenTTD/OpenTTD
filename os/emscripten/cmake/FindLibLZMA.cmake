# LibLZMA is a custom addition to the emscripten SDK, so it is possible
# someone patched their SDK. Test out if the SDK supports LibLZMA.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "--use-port=contrib.liblzma")

check_cxx_source_compiles("
    #include <lzma.h>
    int main() { return 0; }"
    LIBLZMA_FOUND
)

if (LIBLZMA_FOUND)
        add_library(LibLZMA::LibLZMA INTERFACE IMPORTED)
        set_target_properties(LibLZMA::LibLZMA PROPERTIES
                INTERFACE_COMPILE_OPTIONS "--use-port=contrib.liblzma"
                INTERFACE_LINK_LIBRARIES "--use-port=contrib.liblzma"
        )
else()
        message(WARNING "You are using an emscripten SDK without LibLZMA support. Many savegames won't be able to load in OpenTTD. Please copy liblzma.py to your ports/contrib folder in your local emsdk installation.")
endif()
