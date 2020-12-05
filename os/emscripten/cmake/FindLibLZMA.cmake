# LibLZMA is a recent addition to the emscripten SDK, so it is possible
# someone hasn't updated his SDK yet. Test out if the SDK supports LibLZMA.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-sUSE_LIBLZMA=1")

check_cxx_source_compiles("
    #include <lzma.h>
    int main() { return 0; }"
    LIBLZMA_FOUND
)

if (LIBLZMA_FOUND)
        add_library(LibLZMA::LibLZMA INTERFACE IMPORTED)
        set_target_properties(LibLZMA::LibLZMA PROPERTIES
                INTERFACE_COMPILE_OPTIONS "-sUSE_LIBLZMA=1"
                INTERFACE_LINK_LIBRARIES "-sUSE_LIBLZMA=1"
        )
else()
        message(WARNING "You are using an emscripten SDK without LibLZMA support. Many savegames won't be able to load in OpenTTD. Please apply 'emsdk-liblzma.patch' to your local emsdk installation.")
endif()
