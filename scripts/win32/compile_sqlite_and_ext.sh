#!/usr/bin/env bash

# This script is intended for compilation from MSYS2 bash shell (dedicated for clang64, clangarm64 or mingw64).
# It downloads SQLite from https://github.com/pawelsalawa/sqlite3-letos/releases
# and compiles SQLite extensions usually bounded with Letos official distribution.
#
# As result it creates 2 directories (or uses existing):
# ../../../sqlite
# ../../../ext

if [ "$2" == "" ]; then
	echo "$0 <SQLite DLL URL> <SQLite extension sources URL>"
	echo ""
	echo "SQLite DLL URL is for example:"
	echo "https://github.com/pawelsalawa/sqlite3-letos/releases/download/v3.53.1/sqlite3-windows-x64-3530100.zip"
	echo ""
	echo "SQLite extension sources URL is for example:"
	echo "https://github.com/pawelsalawa/sqlite3-letos/releases/download/v3.53.1/sqlite3-extensions-src-3530100.zip"
	exit 1
fi

DLL_URL="$1"
EXT_URL="$2"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
pushd "$SCRIPT_DIR/../.." > /dev/null || exit 1
TOP_DIR="$(pwd)"

cleanup() {
	cd $TOP_DIR
	rm -rf sqlite.zip sqlite_ext.zip ext-src
    popd > /dev/null || true
}
trap cleanup EXIT

REQUIRED_TOOLS=(
    gendef
    dlltool
	curl
	clang
	realpath
)

for tool in "${REQUIRED_TOOLS[@]}"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Error: required command '$tool' is not in PATH"
        exit 1
    fi
done

##############################################
echo "Getting SQLite3"
curl -L $DLL_URL --output sqlite.zip
unzip sqlite.zip sqlite3.dll sqlite3.h sqlite3ext.h -d ../sqlite
rm sqlite.zip

cd ../sqlite
gendef sqlite3.dll
dlltool -d sqlite3.def -l libsqlite3.dll.a


##############################################
echo "Getting and compiling SQLite extensions"
cd $TOP_DIR
curl -L $EXT_URL --output sqlite_ext.zip

unzip sqlite_ext.zip -d ext-src
rm sqlite_ext.zip
cd ext-src
FLAGS="-shared -O2 -I$TOP_DIR/../sqlite -L$TOP_DIR/../sqlite -lsqlite3"

mkdir -p $TOP_DIR/../ext
for f in compress sqlar; do
	echo "Compiling misc/$f.c to $(realpath -m $TOP_DIR/../ext/$f.dll)"
	clang misc/$f.c -Imisc $FLAGS -lz -o $TOP_DIR/../ext/$f.dll
done

for f in csv decimal eval ieee754 percentile rot13 series uint uuid zorder; do
	echo "Compiling misc/$f.c to $(realpath -m $TOP_DIR/../ext/$f.dll)"
	clang misc/$f.c -Imisc $FLAGS -o $TOP_DIR/../ext/$f.dll
done

for f in icu; do
	echo "Compiling icu/$f.c to $(realpath -m $TOP_DIR/../ext/$f.dll)"
	clang icu/$f.c `pkg-config --libs --cflags icu-uc icu-io` $FLAGS -o $TOP_DIR/../ext/$f.dll
done

cd ..
rm -rf ext-src
