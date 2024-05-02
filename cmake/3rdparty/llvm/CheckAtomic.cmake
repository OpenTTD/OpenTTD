#
# This was originally part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See (https://llvm.org/)LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Modifications have been made to suit building OpenTTD.
#

# atomic builtins are required for threading support.

INCLUDE(CheckCXXSourceCompiles)
INCLUDE(CheckLibraryExists)

# Sometimes linking against libatomic is required for atomic ops, if
# the platform doesn't support lock-free atomics.

function(check_working_cxx_atomics varname)
  set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++17")
  check_cxx_source_compiles("
#include <atomic>
std::atomic<int> x;
std::atomic<short> y;
std::atomic<char> z;
int main() {
  ++z;
  ++y;
  return ++x;
}
" ${varname})
  set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
endfunction(check_working_cxx_atomics)

function(check_working_cxx_atomics64 varname)
  set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
  set(CMAKE_REQUIRED_FLAGS "-std=c++17 ${CMAKE_REQUIRED_FLAGS}")
  check_cxx_source_compiles("
#include <atomic>
#include <cstdint>
std::atomic<uint64_t> x (0);
int main() {
  uint64_t i = x.load(std::memory_order_relaxed);
  (void)i;
  return 0;
}
" ${varname})
  set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
endfunction(check_working_cxx_atomics64)


# Check for (non-64-bit) atomic operations.
if(MSVC)
  set(HAVE_CXX_ATOMICS_WITHOUT_LIB True)
else()
  # First check if atomics work without the library.
  check_working_cxx_atomics(HAVE_CXX_ATOMICS_WITHOUT_LIB)
  # If not, check if the library exists, and atomics work with it.
  if(NOT HAVE_CXX_ATOMICS_WITHOUT_LIB)
    # check_library_exists requires the C-compiler as the atomic functions are built-in declared.
    enable_language(C)
    check_library_exists(atomic __atomic_fetch_add_4 "" HAVE_LIBATOMIC)
    if(HAVE_LIBATOMIC)
      list(APPEND CMAKE_REQUIRED_LIBRARIES "atomic")
      check_working_cxx_atomics(HAVE_CXX_ATOMICS_WITH_LIB)
      if (NOT HAVE_CXX_ATOMICS_WITH_LIB)
        message(FATAL_ERROR "Host compiler must support std::atomic!")
      endif()
    else()
      message(FATAL_ERROR "Host compiler appears to require libatomic, but cannot find it.")
    endif()
  endif()
endif()

# Check for 64 bit atomic operations.
if(MSVC)
  set(HAVE_CXX_ATOMICS64_WITHOUT_LIB True)
else()
  # First check if atomics work without the library.
  check_working_cxx_atomics64(HAVE_CXX_ATOMICS64_WITHOUT_LIB)
  # If not, check if the library exists, and atomics work with it.
  if(NOT HAVE_CXX_ATOMICS64_WITHOUT_LIB)
    # check_library_exists requires the C-compiler as the atomic functions are built-in declared.
    enable_language(C)
    check_library_exists(atomic __atomic_load_8 "" HAVE_CXX_LIBATOMICS64)
    if(HAVE_CXX_LIBATOMICS64)
      list(APPEND CMAKE_REQUIRED_LIBRARIES "atomic")
      check_working_cxx_atomics64(HAVE_CXX_ATOMICS64_WITH_LIB)
      if (NOT HAVE_CXX_ATOMICS64_WITH_LIB)
        message(FATAL_ERROR "Host compiler must support 64-bit std::atomic!")
      endif()
    else()
      message(FATAL_ERROR "Host compiler appears to require libatomic for 64-bit operations, but cannot find it.")
    endif()
  endif()
endif()

if(HAVE_CXX_ATOMICS_WITH_LIB OR HAVE_CXX_ATOMICS64_WITH_LIB)
  target_link_libraries(openttd_lib atomic)
endif()
