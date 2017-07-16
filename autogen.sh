#!/bin/sh -e
rm -rf build
exec meson . build -Dprefix=/usr/local/stow/libcm4all-common --werror -Db_sanitize=address "$@"
