#!/bin/sh

export PATH=$PATH:${PWD}/buildroot/output/host/bin

mkdir -p images

cd buildroot

rm -f output/build/linux-custom/.stamp_built output/build/linux-custom/.stamp_*_installed
./make.sh

cd ../dts
dtc -I dts -O dtb -p 2000 -o ../images/ideal.dtb system-top.dts

cd ../images
mkimage -f ../cfgs/ideal_fdt.its ideal.ub

cd ../