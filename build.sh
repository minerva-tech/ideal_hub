#!/bin/sh

set -e

export PATH=$PATH:${PWD}/buildroot/output/host/bin

mkdir -p images

cd buildroot

rm -f output/build/linux-custom/.stamp_built output/build/linux-custom/.stamp_*_installed

if [ "x$1" = "xclean" ]; then
    rm -fr output
    ./make.sh minerva_ideal_defconfig
fi

./make.sh

cd ../dts
#linux-dtc -I dts -O dtb -b 0 -Wno-unit_address_vs_reg -o ../images/ideal.dtb system-top.dts
linux-dtc -I dts -O dtb -b 0 -Wno-unit_address_vs_reg -o ../images/ideal.dtb zynq-zed.dts

cd ../images
mkimage -f ../cfgs/ideal_fdt.its ideal.ub
cp -f ../buildroot/output/images/boot.bin fsbl-uboot.bin
dd if=../buildroot/output/images/u-boot.img of=fsbl-uboot.bin seek=128 bs=1K

cd ../
