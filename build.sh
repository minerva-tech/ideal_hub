#!/bin/sh

set -e

export PATH=$PATH:${PWD}/buildroot/output/host/bin

print_red_msg ()
{
    echo -e "\033[0;31m"$1"\033[0m"
}

print_green_msg ()
{
    echo -e "\033[1;32m""$1""\033[0m"
}

if [ ! -d project ]; then
    print_red_msg "Error: link to the project directory does not exist !!!"
    print_red_msg "Please create it. Example:"
    print_green_msg "ln -s projects/iHub2 project"
    exit 1
fi

mkdir -p buildroot/dl

TOP_DIR=${PWD}
BUILDROOT=${PWD}/buildroot

cd project

PROJECT=${PWD}

rm -fr buildroot/configs

if [ ! -f xilinx/top.dts -a ! -d xilinx/top.dts ]; then
    print_red_msg "Error: project/xilinx/top.dts file does not exist !!!"
    exit 2
fi

mkdir -p buildroot/configs
cd buildroot
rm -fr board
mkdir -p board/zynq
cp -dpfr ${BUILDROOT}/board/zynq/minerva board/zynq
cp -f ../defconfig configs/project_defconfig

rm -f output/build/linux-custom/.stamp_built output/build/linux-custom/.stamp_*_installed

LINK_LIST="arch boot Config.in docs linux Makefile.legacy support toolchain build Config.in.legacy dl fs Makefile make.sh package system utils"

for i in $LINK_LIST
do
    rm -f $i
    ln -sr ${BUILDROOT}/$i $i
done

if [ "x$1" = "xclean" ]; then
    rm -fr output
    ./make.sh project_defconfig
fi

[ ! -f .config ] && ./make.sh project_defconfig

./make.sh

cd ../
rm -fr images
ln -s buildroot/output/images images

XILINX_SETTINGS=${TOP_DIR}/xilinx_settings.sh

if [ -d ${XILINX_SETTINGS} -o -f ${XILINX_SETTINGS} ]; then
    source ${TOP_DIR}/xilinx_settings.sh
else
    print_red_msg "Warning: ${XILINX_SETTINGS} does not exist, 'BOOT.BIN' image won't be build !!!"
    print_red_msg "Please install Xilinx Vivado SDK and create a link. Example:"
    print_green_msg "ln -s /opt/Xilinx/Vivado/2018.1/settings64.sh xilinx_settings.sh"
    exit 3
fi

mkdir -p fsbl
cd fsbl
rm -f u-boot.elf
ln -s ../buildroot/output/images/u-boot u-boot.elf
HDF=`ls ../xilinx/*.hdf`
if [ -z "$HDF" ]; then
    print_red_msg "Error: there is no HDF file in ${PWD}/../xilinx folder !"
    exit 4
elif [ `echo $HDF | wc -w` -ne 1 ]; then
    print_red_msg "Error: there are more than one HDF file in ${PWD}/../xilinx folder !"
    exit 5
fi

PATCH=${PWD}/patch.sh
if [ -f ${PATCH} -o -d ${PATCH} ]; then
    ${TOP_DIR}/common/scripts/build_boot_bin.sh $HDF u-boot.elf ${PATCH}
else
    ${TOP_DIR}/common/scripts/build_boot_bin.sh $HDF u-boot.elf
fi

cp -f output_boot_bin/BOOT.BIN ../images

cd ${TOP_DIR}
