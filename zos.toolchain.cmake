set(CMAKE_SYSTEM_NAME OS390)
set(CMAKE_C_COMPILER ibm-clang64)

set(ZOPEN_PREFIX "/u/ssuser1/zopen/usr/local")
set(ZOSLIB_PREFIX "${ZOPEN_PREFIX}/zopen/zoslib/zoslib")

set(CMAKE_PREFIX_PATH "${ZOPEN_PREFIX}")
set(CMAKE_REQUIRED_INCLUDES "${ZOPEN_PREFIX}/include")
set(CMAKE_LIBRARY_PATH "${ZOPEN_PREFIX}/lib")

# Do NOT define ZOSLIB_OVERRIDE_CLIB. The override macro causes zoslib's
# wrapper headers to rename libc functions (#define fopen __fopen_replaced)
# before #include_next, which changes the branch paths inside the z/OS
# system headers and hides type definitions (time_t, pid_t, mode_t, etc.)
# when combined with -std=gnu11 (which CMake adds automatically).
#
# Without the override, we still get zoslib's link-time symbols (strndup,
# pipe2, realpath, nanosleep, clock_gettime, timegm, etc.) just by linking
# -lzoslib.
set(CMAKE_C_FLAGS_INIT "-m64 -fzos-le-char-mode=ascii -mno-short-name \
  -D_XOPEN_SOURCE=600 -D_POSIX_SOURCE -D_ALL_SOURCE \
  -D_OPEN_SYS_FILE_EXT=1 -D_OPEN_SYS_EXT=1 \
  -D_XPLATFORM_SOURCE \
  -Wno-bitwise-instead-of-logical \
  -Dbswap16=__builtin_bswap16 -Dbswap32=__builtin_bswap32 -Dbswap64=__builtin_bswap64 \
  -I${ZOPEN_PREFIX}/include")

# Link zoslib (C++ internally) and the LE C++ runtime side-decks it depends on.
# notcurses itself is pure C (USE_CXX=off) but zoslib uses C++ exceptions,
# std::string, iostream, etc. internally.  Since we link with the C driver
# (ibm-clang64), it won't add the C++ LE side-decks automatically.
# On z/OS, the C++ runtime lives in MVS datasets (CEE.SCEELIB), not Unix .a files.
set(ZOS_CXX_SIDEDECKS "\
  \"//'CEE.SCEELIB(CRTDQCXH)'\" \
  \"//'CEE.SCEELIB(CRTDQCXG)'\" \
  \"//'CEE.SCEELIB(CRTDQCXE)'\" \
  \"//'CEE.SCEELIB(CRTDQCXS)'\" \
  \"//'CEE.SCEELIB(CRTDQCXP)'\" \
  \"//'CEE.SCEELIB(CRTDQCXA)'\" \
  \"//'CEE.SCEELIB(CRTDQXLA)'\" \
  \"//'CEE.SCEELIB(CRTDQUNW)'\"")

# Link the zopen portable GNU getopt (libgetopt.a) so that getopt_long()
# works correctly in ASCII mode (-fzos-le-char-mode=ascii).  The system
# z/OS getopt operates on EBCDIC argv, which causes option parsing to fail
# when the binary is compiled in ASCII mode.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-m64 \
  -L${ZOSLIB_PREFIX}/lib -lzoslib \
  -L${ZOPEN_PREFIX}/lib -lgetopt \
  ${ZOS_CXX_SIDEDECKS}")

set(CMAKE_SHARED_LINKER_FLAGS_INIT "-m64 \
  -L${ZOSLIB_PREFIX}/lib -lzoslib \
  -L${ZOPEN_PREFIX}/lib \
  ${ZOS_CXX_SIDEDECKS}")
