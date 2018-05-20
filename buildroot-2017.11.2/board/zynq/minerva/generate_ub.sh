#!/bin/sh

set -e
linux-dtc -I dts -O dtb -b 0 -Wno-unit_address_vs_reg -o output/images/minerva.dtb ../xilinx/top.dts
mkimage -f board/zynq/minerva/fdt.its output/images/minerva.ub
