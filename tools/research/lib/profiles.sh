# Build profile -> CMake configuration mapping for tools/research/research.
# Deliberately a set of case statements, not a bash associative array: this
# repo's tooling (tools/gate.sh) targets plain portable bash, and associative
# arrays require bash 4+ (macOS's /bin/bash is still 3.2).

profile_list() {
  printf '%s\n' "research-debug" "baseline-release" "research-asan"
}

profile_build_dir() {
  case "$1" in
    research-debug) printf '%s' "$ROOT_DIR/build-research" ;;
    baseline-release) printf '%s' "$ROOT_DIR/build-research-baseline-release" ;;
    research-asan) printf '%s' "$ROOT_DIR/build-research-asan" ;;
    *) return 1 ;;
  esac
}

profile_cmake_build_type() {
  case "$1" in
    research-debug) printf 'Debug' ;;
    baseline-release) printf 'RelWithDebInfo' ;;
    research-asan) printf 'Debug' ;;
    *) return 1 ;;
  esac
}

profile_research_instrumentation() {
  case "$1" in
    research-debug) printf 'ON' ;;
    baseline-release) printf 'OFF' ;;
    research-asan) printf 'ON' ;;
    *) return 1 ;;
  esac
}

# Extra -D flags beyond build type / research instrumentation, space-separated
# (each flag itself must not contain internal spaces -- true for all of these).
profile_extra_cmake_args() {
  case "$1" in
    research-debug) printf '' ;;
    baseline-release) printf -- '-DOPTION_USE_ASSERTS=OFF' ;;
    research-asan) printf -- '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined' ;;
    *) return 1 ;;
  esac
}

profile_description() {
  case "$1" in
    research-debug)
      printf 'Debug build, asserts on, research instrumentation ON (RESEARCH-ONLY console commands compiled in). The experimental configuration.' ;;
    baseline-release)
      printf 'RelWithDebInfo, asserts off, research instrumentation OFF. Comparison baseline -- closest to a real release build this fork'"'"'s CLI manages.' ;;
    research-asan)
      printf 'Debug + AddressSanitizer/UndefinedBehaviorSanitizer, research instrumentation ON. Configure-flags only in this fork so far -- not built or run (see research/tooling.md limitations).' ;;
    *) return 1 ;;
  esac
}

profile_is_buildable() {
  # research-asan is intentionally configure-only for now; `build`/`run`
  # refuse it with a clear message rather than silently taking a long time
  # and then being reported as untested anyway.
  case "$1" in
    research-asan) return 1 ;;
    *) return 0 ;;
  esac
}
