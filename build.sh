#!/bin/sh

set -e

export PATH=$PATH:${PWD}/buildroot/output/host/bin

CLEAN=""

print_red_msg ()
{
    echo -e "\033[0;31m"$1"\033[0m"
}

print_green_msg ()
{
    echo -e "\033[1;32m""$1""\033[0m"
}

print_blue_msg ()
{
    echo -e "\033[0;34m""$1""\033[0m"
}

usage () {
    echo -n "$1 [options] [project name]"
    echo -e "\t-c --- clean project building directories"
}

# parse command line into arguments
res=`getopt ch $*`
rc=$?
# check result of parsing
if [ $rc != 0 ]
then
    usage $0
    exit 1
fi

set -- $res

# set options
while [ $1 != -- ]
do
    case $1 in
    -c)	# clean
	CLEAN=yes;;
    -h)	# print help
	usage $0
	exit 0;;
    esac
    shift   # next flag
done
shift

# turn on exit-on-error mode
set -e

if [ -z "$1" ]; then
    if [ ! -d projects/$1 ]; then
	print_red_message "Project folder $1 does not exist !"
	exit 2
    fi
else
    rm -fr project
    ln -s projects/$1 project
fi

if [ ! -d project ]; then
    print_red_msg "Error: link to the project directory does not exist !!!"
    print_red_msg "Please create it manually. Example:"
    print_green_msg "ln -s projects/iHub2 project"
    print_red_msg "Also you can specify project name in $0 command line. Example:"
    print_green_msg "$0 iHub2"
    exit 3
fi

mkdir -p buildroot/dl

TOP_DIR=${PWD}
BUILDROOT=${PWD}/buildroot

cd project

PROJECT=${PWD}

rm -fr buildroot/configs

if [ ! -f xilinx/top.dts -a ! -d xilinx/top.dts ]; then
    print_red_msg "Error: project/xilinx/top.dts file does not exist !!!"
    exit 4
fi

mkdir -p buildroot/configs
cd buildroot
rm -fr board
mkdir -p board/zynq
cp -dpfr ${BUILDROOT}/board/zynq/minerva board/zynq
cp -f ../defconfig configs/project_defconfig

rm -f output/build/linux-custom/.stamp_built output/build/linux-custom/.stamp_*_installed

LINK_LIST="arch boot Config.in docs linux Makefile.legacy support toolchain Config.in.legacy dl fs Makefile make.sh package system utils"

for i in $LINK_LIST
do
    rm -f $i
    ln -sr ${BUILDROOT}/$i $i
done

if [ -n "$CLEAN" ]; then
    print_blue_msg "Cleaning all ..."
    rm -fr output
    ./make.sh project_defconfig
fi

[ ! -f .config ] && ./make.sh project_defconfig

print_blue_msg "Building buildroot ..."
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
    exit 5
fi

print_blue_msg "Building FSBL ..."

mkdir -p fsbl
cd fsbl
rm -f u-boot.elf
ln -s ../buildroot/output/images/u-boot u-boot.elf
HDF=`ls ../xilinx/*.hdf`
if [ -z "$HDF" ]; then
    print_red_msg "Error: there is no HDF file in ${PWD}/../xilinx folder !"
    exit 6
elif [ `echo $HDF | wc -w` -ne 1 ]; then
    print_red_msg "Error: there are more than one HDF file in ${PWD}/../xilinx folder !"
    exit 7
fi

PATCH=${PWD}/patch.sh
if [ -f ${PATCH} -o -d ${PATCH} ]; then
    ${TOP_DIR}/common/scripts/build_boot_bin.sh $HDF u-boot.elf ${PATCH}
else
    ${TOP_DIR}/common/scripts/build_boot_bin.sh $HDF u-boot.elf
fi

cp -f output_boot_bin/BOOT.BIN ../images

cd ${TOP_DIR}

print_blue_msg "DONE."
