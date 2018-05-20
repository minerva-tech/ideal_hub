#!/bin/bash
set -ex

HDF_FILE=$1
UBOOT_FILE=$2
BUILD_DIR=build_boot_bin
OUTPUT_DIR=output_boot_bin

usage () {
	echo usage: $0 system_top.hdf u-boot.elf [output-archive]
	exit 1
}

depends () {
	echo Xilinx $1 must be installed and in your PATH
	echo try: source /opt/Xilinx/Vivado/201x.x/settings64.sh
	exit 1
}

### Check command line parameters
echo $HDF_FILE | grep -q ".hdf" || usage
echo $UBOOT_FILE | grep -q -e ".elf" -e "uboot" || usage

if [ ! -f $HDF_FILE ]; then
    echo $HDF_FILE: File not found!
    usage
fi

if [ ! -f $UBOOT_FILE ]; then
    echo $UBOOT_FILE: File not found!
    usage
fi

### Check for required Xilinx tools
command -v xsdk >/dev/null 2>&1 || depends xsdk
command -v bootgen >/dev/null 2>&1 || depends bootgen

rm -Rf $BUILD_DIR $OUTPUT_DIR
mkdir -p $OUTPUT_DIR
mkdir -p $BUILD_DIR

cp $HDF_FILE $BUILD_DIR/
cp $UBOOT_FILE $OUTPUT_DIR/u-boot.elf
cp $HDF_FILE $OUTPUT_DIR/

### Create create_fsbl_project.tcl file used by xsdk to create the fsbl
echo "hsi open_hw_design `basename $HDF_FILE`" > $BUILD_DIR/create_fsbl_project.tcl
echo 'set cpu_name [lindex [hsi get_cells -filter {IP_TYPE==PROCESSOR}] 0]' >> $BUILD_DIR/create_fsbl_project.tcl
echo 'sdk setws ./build/sdk' >> $BUILD_DIR/create_fsbl_project.tcl
echo "sdk createhw -name hw_0 -hwspec `basename $HDF_FILE`" >> $BUILD_DIR/create_fsbl_project.tcl
echo 'sdk createapp -name fsbl -hwproject hw_0 -proc $cpu_name -os standalone -lang C -app {Zynq FSBL}' >> $BUILD_DIR/create_fsbl_project.tcl
echo 'configapp -app fsbl build-config release' >> $BUILD_DIR/create_fsbl_project.tcl

echo "hsi open_hw_design `basename $HDF_FILE`" > $BUILD_DIR/build_fsbl_project.tcl
echo 'set cpu_name [lindex [hsi get_cells -filter {IP_TYPE==PROCESSOR}] 0]' >> $BUILD_DIR/build_fsbl_project.tcl
echo 'sdk setws ./build/sdk' >> $BUILD_DIR/build_fsbl_project.tcl
echo 'sdk projects -build -type all' >> $BUILD_DIR/build_fsbl_project.tcl

### Build fsbl.elf
(
	cd $BUILD_DIR
	xsdk -batch -source create_fsbl_project.tcl
	xsdk -batch -source build_fsbl_project.tcl
	if [ -n "$3" ]; then
	    eval $3
	    xsdk -batch -source build_fsbl_project.tcl
	fi
)

### Copy fsbl and *.bit into the output folder
cp $BUILD_DIR/build/sdk/fsbl/Release/fsbl.elf $OUTPUT_DIR
cp $BUILD_DIR/build/sdk/hw_0/*.bit $OUTPUT_DIR

BITSTREAM=`basename $(ls ${OUTPUT_DIR}/*.bit)`

### Create zynq.bif file used by bootgen
echo 'the_ROM_image:' > $OUTPUT_DIR/zynq.bif
echo '{' >> $OUTPUT_DIR/zynq.bif
echo '[bootloader] fsbl.elf' >> $OUTPUT_DIR/zynq.bif
echo "${BITSTREAM}" >> $OUTPUT_DIR/zynq.bif
echo 'u-boot.elf' >> $OUTPUT_DIR/zynq.bif
echo '}' >> $OUTPUT_DIR/zynq.bif

### Build BOOT.BIN
(
	cd $OUTPUT_DIR
	bootgen -arch zynq -image zynq.bif -o BOOT.BIN -w
)
