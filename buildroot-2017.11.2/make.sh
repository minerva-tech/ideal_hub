#!/bin/sh

export LD_LIBRARY_PATH=/lib:/usr/lib
export PATH=/bin:/sbin:/usr/bin:/usr/sbin

make $@
