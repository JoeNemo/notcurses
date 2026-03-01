#!/bin/bash
# Build script for notcurses on z/OS
# Source this or run: bash ~/git2026/notcurses/build_zos.sh

export _BPXK_AUTOCVT=ON
export _CEE_RUNOPTS="FILETAG(AUTOCVT,AUTOTAG) POSIX(ON)"
export _TAG_REDIR_ERR=txt
export _TAG_REDIR_IN=txt
export _TAG_REDIR_OUT=txt

# Source zopen environment
. $HOME/zopen/etc/zopen-config --override-zos-tools

# OpenXL compiler
if [ -d /rsusr/openxl/by-zos/v301/openxl/bin ]; then
  export PATH=/rsusr/openxl/by-zos/v301/openxl/bin:$PATH
fi
if [ -d /rsusr/openxl/by-zos/v301/openxl/lib ]; then
  export LIBPATH=/rsusr/openxl/by-zos/v301/openxl/lib:${LIBPATH:-/lib:/usr/lib}
fi

export ZOPEN=/u/ssuser1/zopen/usr/local
export PKG_CONFIG_PATH="$ZOPEN/lib/pkgconfig"
export LANG=en_US.UTF-8.lp64
export LC_ALL=en_US.UTF-8.lp64

echo "=== Environment ==="
echo "PATH starts with: $(echo $PATH | cut -d: -f1-3)"
echo "cmake: $(which cmake)"
echo "ibm-clang64: $(which ibm-clang64)"
echo "cmake version: $(cmake --version | head -1)"
echo ""

cd /u/ssuser1/git2026/notcurses
mkdir -p build
cd build

echo "=== Running cmake ==="
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../zos.toolchain.cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DUSE_CXX=off \
  -DUSE_MULTIMEDIA=none \
  -DUSE_DEFLATE=off \
  -DUSE_PANDOC=off \
  2>&1

if [ $? -ne 0 ]; then
  echo "=== cmake FAILED ==="
  exit 1
fi

echo ""
echo "=== Running make ==="
make -j4 2>&1

if [ $? -ne 0 ]; then
  echo "=== make FAILED ==="
  exit 1
fi

echo ""
echo "=== Build SUCCESS ==="
ls -la lib* notcurses-info notcurses-demo 2>/dev/null
