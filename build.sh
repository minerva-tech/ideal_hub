#!/bin/sh

export PATH=$PATH:${PWD}/buildroot/output/host/bin

mkdir -p images

cd buildroot

rm -f output/build/linux-custom/.stamp_built output/build/linux-custom/.stamp_*_installed
./make.sh

cd ../dts
linux-dtc -I dts -O dtb -b 0 -Wno-unit_address_vs_reg -o ../images/zynq-zed.dtb system-top.dts

cd ../images
mkimage -f ../cfgs/ideal_fdt.its ideal.ub

cd ../