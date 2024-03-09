# Macro which contains all bits to setup the compile flags correctly.
#
# compile_flags()
#
macro(compile_flags)
    if(MSVC)
        # "If /Zc:rvalueCast is specified, the compiler follows section 5.4 of the
        # C++11 standard". We need C++11 for the way we use threads.
        add_compile_options(/Zc:rvalueCast)

        if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            add_compile_options(
                /MP # Enable multi-threaded compilation.
                /FC # Display the full path of source code files passed to the compiler in diagnostics.
            )
        endif()
    endif()

    # Our strings are UTF-8.
    if(MSVC)
        add_compile_options(/utf-8)
    else()
        add_compile_options(-finput-charset=utf-8)
    endif()

    # Add some -D flags for Debug builds. We cannot use add_definitions(), because
    # it does not appear to support the $<> tags.
    add_compile_options(
        "$<$<CONFIG:Debug>:-D_DEBUG>"
        "$<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>" # FORTIFY_SOURCE should only be used in non-debug builds (requires -O1+)
    )
    if(MINGW)
        add_link_options(
            "$<$<NOT:$<CONFIG:Debug>>:-fstack-protector>" # Prevent undefined references when _FORTIFY_SOURCE > 0
        )
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            add_compile_options(
                "$<$<CONFIG:Debug>:-Wa,-mbig-obj>" # Switch to pe-bigobj-x86-64 as x64 Debug builds push pe-x86-64 to the limits (linking errors with ASLR, ...)
            )
        endif()
    endif()

    # Prepare a generator that checks if we are not a debug, and don't have asserts
    # on. We need this later on to set some compile options for stable releases.
    set(IS_STABLE_RELEASE "$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<BOOL:${OPTION_USE_ASSERTS}>>>")

    if(MSVC)
        add_compile_options(
            /W3
            /w34100 # 'identifier' : unreferenced formal parameter
            /w34189 # 'identifier' : local variable is initialized but not referenced
        )
        if(MSVC_VERSION GREATER 1929 AND MSVC_VERSION LESS 1937 AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            # Starting with version 19.30 (fixed in version 19.37), there is an optimisation bug, see #9966 for details
            # This flag disables the broken optimisation to work around the bug
            add_compile_options(/d2ssa-rse-)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        add_compile_options(
            -W
            -Wall
            -Wcast-qual
            -Wextra
            -Wsign-compare
            -Wundef
            -Wpointer-arith
            -Wwrite-strings
            -Wredundant-decls
            -Wformat-security
            -Wformat=2
            -Winit-self
            -Wnon-virtual-dtor
            -Wsuggest-override

            # We use 'ABCD' multichar for SaveLoad chunks identifiers
            -Wno-multichar
        )

        # Ninja processes the output so the output from the compiler
        # isn't directly to a terminal; hence, the default is
        # non-coloured output. We can override this to get nicely
        # coloured output, but since that might yield odd results with
        # IDEs, we extract it to an option.
        if(OPTION_FORCE_COLORED_OUTPUT)
            if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
                add_compile_options (-fdiagnostics-color=always)
            elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
                add_compile_options (-fcolor-diagnostics)
            endif()
        endif()

        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            include(CheckCXXCompilerFlag)
            check_cxx_compiler_flag("-flifetime-dse=1" LIFETIME_DSE_FOUND)

            add_compile_options(
                # GCC 4.2+ automatically assumes that signed overflows do
                # not occur in signed arithmetics, whereas we are not
                # sure that they will not happen. It furthermore complains
                # about its own optimized code in some places.
                "-fno-strict-overflow"

                # Prevent optimisation supposing enums are in a range specified by the standard
                # For details, see http://gcc.gnu.org/PR43680
                "-fno-tree-vrp"

                # -flifetime-dse=2 (default since GCC 6) doesn't play
                # well with our custom pool item allocator
                "$<$<BOOL:${LIFETIME_DSE_FOUND}>:-flifetime-dse=1>"

                # We have a fight between clang wanting std::move() and gcc not wanting it
                # and of course they both warn when the other compiler is happy
                "-Wno-redundant-move"
            )
        endif()

        if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
            if (NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
                include(CheckCXXCompilerFlag)
                check_cxx_compiler_flag("-mno-sse4" NO_SSE4_FOUND)

                if(NO_SSE4_FOUND)
                    add_compile_options(
                        # Don't use SSE4 for general sources to increase compatibility.
                        -mno-sse4
                    )
                endif()
            endif()
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
        add_compile_options(
            -Wall
            # warning #873: function ... ::operator new ... has no corresponding operator delete ...
            -wd873
            # warning #1292: unknown attribute "fallthrough"
            -wd1292
            # warning #1899: multicharacter character literal (potential portability problem)
            -wd1899
            # warning #2160: anonymous union qualifier is ignored
            -wd2160
        )
    else()
        message(FATAL_ERROR "No warning flags are set for this compiler yet; please consider creating a Pull Request to add support for this compiler.")
    endif()

    if(NOT WIN32 AND NOT HAIKU)
        # rdynamic is used to get useful stack traces from crash reports.
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")
    endif()
endmacro()
