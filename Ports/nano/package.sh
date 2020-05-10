#!/bin/bash ../.port_include.sh
port=nano
version=4.9.2
useconfigure="true"
files="https://www.nano-editor.org/dist/v4/nano-${version}.tar.xz nano-${version}.tar.xz
https://www.nano-editor.org/dist/v4/nano-${version}.tar.xz.asc nano-${version}.tar.xz.asc"
configopts="--target=i686-pc-serenity --disable-browser --disable-utf8"
depends="ncurses libpuffy"
auth_type="sig"
auth_import_key="BFD009061E535052AD0DF2150D28D4D2A0ACE884"
auth_opts="nano-${version}.tar.xz.asc nano-${version}.tar.xz"

export CPPFLAGS="-I${SERENITY_ROOT}/Root/usr/local/include/ncurses"
export LIBS="-lpuffy"
export PKG_CONFIG_PATH=${SERENITY_ROOT}/Root/usr/local/lib/pkgconfig
