#!/bin/sh

( cd project/buildroot/output/build/linux; rm -f .stamp_built .stamp_images_installed .stamp_initramfs_rebuilt .stamp_target_installed )

./build.sh
