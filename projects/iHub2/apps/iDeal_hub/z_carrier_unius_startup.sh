#!/bin/sh
awk ' BEGIN {a=0}; /ip address/ { a=1 };{if (a == 1) {system("ifconfig eth0 down");system("ifconfig eth0 " $3);system("ifconfig eth0 up");a=0}} ' /sys/bus/i2c/devices/0-0057/eeprom
#/bin/iDeal_hub &