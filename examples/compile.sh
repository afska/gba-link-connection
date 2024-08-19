#!/bin/bash

# Usage:
#   ./compile.sh
#   ./compile.sh multiboot

if [ "$1" = "multiboot" ]; then
    args="bMB=1"
    suffix=".mb"
else
    args="bMB=0"
    suffix=""
fi

if [ "$(uname)" = "Darwin" ]; then
    # macOS
    sed_inplace_option="-i \"\""
else
    sed_inplace_option="-i"
fi

cd LinkCable_basic/
make rebuild $args
cp LinkCable_basic$suffix.gba ../
cd ..

cd LinkCable_full/
make rebuild $args
cp LinkCable_full$suffix.gba ../
cd ..

cd LinkCable_stress/
make rebuild $args
cp LinkCable_stress$suffix.gba ../
cd ..

cd LinkCableMultiboot_demo/
make rebuild
cp LinkCableMultiboot_demo.mb.gba ../
cd ..

cd LinkGPIO_demo/
make rebuild $args
cp LinkGPIO_demo$suffix.gba ../
cd ..

cd LinkMobile_demo/
make rebuild $args
cp LinkMobile_demo$suffix.gba ../
cd ..

cd LinkPS2Keyboard_demo/
make rebuild $args
cp LinkPS2Keyboard_demo$suffix.gba ../
cd ..

cd LinkPS2Mouse_demo/
make rebuild $args
cp LinkPS2Mouse_demo$suffix.gba ../
cd ..

cd LinkRawCable_demo/
make rebuild $args
cp LinkRawCable_demo$suffix.gba ../
cd ..

cd LinkRawWireless_demo/
sed $sed_inplace_option -e "s/\/\/ #define LINK_RAW_WIRELESS_ENABLE_LOGGING/#define LINK_RAW_WIRELESS_ENABLE_LOGGING/g" ../../lib/LinkRawWireless.hpp
make rebuild $args
sed $sed_inplace_option -e "s/#define LINK_RAW_WIRELESS_ENABLE_LOGGING/\/\/ #define LINK_RAW_WIRELESS_ENABLE_LOGGING/g" ../../lib/LinkRawWireless.hpp
cp LinkRawWireless_demo$suffix.gba ../
cd ..

cd LinkSPI_demo/
make rebuild $args
cp LinkSPI_demo$suffix.gba ../
cd ..

cd LinkUART_demo/
make rebuild $args
cp LinkUART_demo$suffix.gba ../
cd ..

cd LinkUniversal_basic/
sed $sed_inplace_option -e "s/\/\/ #define LINK_WIRELESS_PUT_ISR_IN_IWRAM/#define LINK_WIRELESS_PUT_ISR_IN_IWRAM/g" ../../lib/LinkWireless.hpp
make rebuild $args
cp LinkUniversal_basic$suffix.gba ../
sed $sed_inplace_option -e "s/#define LINK_WIRELESS_PUT_ISR_IN_IWRAM/\/\/ #define LINK_WIRELESS_PUT_ISR_IN_IWRAM/g" ../../lib/LinkWireless.hpp
cd ..

cd LinkCable_full/
sed $sed_inplace_option -e "s/\/\/ #define USE_LINK_UNIVERSAL/#define USE_LINK_UNIVERSAL/g" src/main.h
sed $sed_inplace_option -e "s/\/\/ #define LINK_WIRELESS_PUT_ISR_IN_IWRAM/#define LINK_WIRELESS_PUT_ISR_IN_IWRAM/g" ../../lib/LinkWireless.hpp
mv LinkCable_full$suffix.gba backup.gba
make rebuild $args
cp LinkCable_full$suffix.gba ../LinkUniversal_full$suffix.gba
mv backup.gba LinkCable_full$suffix.gba
sed $sed_inplace_option -e "s/#define USE_LINK_UNIVERSAL/\/\/ #define USE_LINK_UNIVERSAL/g" src/main.h
sed $sed_inplace_option -e "s/#define LINK_WIRELESS_PUT_ISR_IN_IWRAM/\/\/ #define LINK_WIRELESS_PUT_ISR_IN_IWRAM/g" ../../lib/LinkWireless.hpp
cd ..

cd LinkCable_stress/
sed $sed_inplace_option -e "s/\/\/ #define USE_LINK_UNIVERSAL/#define USE_LINK_UNIVERSAL/g" src/main.h
sed $sed_inplace_option -e "s/\/\/ #define LINK_WIRELESS_PUT_ISR_IN_IWRAM/#define LINK_WIRELESS_PUT_ISR_IN_IWRAM/g" ../../lib/LinkWireless.hpp
mv LinkCable_stress$suffix.gba backup.gba
make rebuild $args
cp LinkCable_stress$suffix.gba ../LinkUniversal_stress$suffix.gba
mv backup.gba LinkCable_stress$suffix.gba
sed $sed_inplace_option -e "s/#define USE_LINK_UNIVERSAL/\/\/ #define USE_LINK_UNIVERSAL/g" src/main.h
sed $sed_inplace_option -e "s/#define LINK_WIRELESS_PUT_ISR_IN_IWRAM/\/\/ #define LINK_WIRELESS_PUT_ISR_IN_IWRAM/g" ../../lib/LinkWireless.hpp
cd ..

cd LinkWireless_demo/
sed $sed_inplace_option -e "s/\/\/ #define LINK_WIRELESS_PUT_ISR_IN_IWRAM/#define LINK_WIRELESS_PUT_ISR_IN_IWRAM/g" ../../lib/LinkWireless.hpp
sed $sed_inplace_option -e "s/\/\/ #define LINK_WIRELESS_USE_SEND_RECEIVE_LATCH/#define LINK_WIRELESS_USE_SEND_RECEIVE_LATCH/g" ../../lib/LinkWireless.hpp
make rebuild $args
sed $sed_inplace_option -e "s/#define LINK_WIRELESS_PUT_ISR_IN_IWRAM/\/\/ #define LINK_WIRELESS_PUT_ISR_IN_IWRAM/g" ../../lib/LinkWireless.hpp
sed $sed_inplace_option -e "s/#define LINK_WIRELESS_USE_SEND_RECEIVE_LATCH/\/\/ #define LINK_WIRELESS_USE_SEND_RECEIVE_LATCH/g" ../../lib/LinkWireless.hpp
cp LinkWireless_demo$suffix.gba ../
cd ..

cd LinkWireless_demo/
sed $sed_inplace_option -e "s/\/\/ #define LINK_WIRELESS_PUT_ISR_IN_IWRAM/#define LINK_WIRELESS_PUT_ISR_IN_IWRAM/g" ../../lib/LinkWireless.hpp
sed $sed_inplace_option -e "s/\/\/ #define LINK_WIRELESS_USE_SEND_RECEIVE_LATCH/#define LINK_WIRELESS_USE_SEND_RECEIVE_LATCH/g" ../../lib/LinkWireless.hpp
sed $sed_inplace_option -e "s/\/\/ #define PROFILING_ENABLED/#define PROFILING_ENABLED/g" ../../lib/LinkWireless.hpp
mv LinkWireless_demo$suffix.gba backup.gba
make rebuild
cp LinkWireless_demo.gba ../LinkWireless_demo_profiler.gba
mv backup.gba LinkWireless_demo$suffix.gba
sed $sed_inplace_option -e "s/#define LINK_WIRELESS_PUT_ISR_IN_IWRAM/\/\/ #define LINK_WIRELESS_PUT_ISR_IN_IWRAM/g" ../../lib/LinkWireless.hpp
sed $sed_inplace_option -e "s/#define LINK_WIRELESS_USE_SEND_RECEIVE_LATCH/\/\/ #define LINK_WIRELESS_USE_SEND_RECEIVE_LATCH/g" ../../lib/LinkWireless.hpp
sed $sed_inplace_option -e "s/#define PROFILING_ENABLED/\/\/ #define PROFILING_ENABLED/g" ../../lib/LinkWireless.hpp
cd ..

cd LinkWirelessMultiboot_demo/
sed $sed_inplace_option -e "s/\/\/ #define LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING/#define LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING/g" ../../lib/LinkWirelessMultiboot.hpp
make rebuild
cp LinkWirelessMultiboot_demo.out.mb.gba ../LinkWirelessMultiboot_demo.mb.gba
sed $sed_inplace_option -e "s/#define LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING/\/\/ #define LINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING/g" ../../lib/LinkWirelessMultiboot.hpp
cd ..
