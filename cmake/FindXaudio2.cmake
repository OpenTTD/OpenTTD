# Autodetect if xaudio2 can be used.

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "")

check_cxx_source_compiles("
    #include <cstdio>
    #include <windows.h>

    #undef NTDDI_VERSION
    #undef _WIN32_WINNT

    #define NTDDI_VERSION    NTDDI_WIN8
    #define _WIN32_WINNT     _WIN32_WINNT_WIN8

    #include <xaudio2.h>
    int main() { printf(\"%s\\\\n\", XAUDIO2_DLL_A); return 0; }"
    XAUDIO2_FOUND
)
