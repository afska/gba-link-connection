#!/bin/bash

cd LinkCable_basic/
make rebuild
cp LinkCable_basic.gba ../
cd ..

cd LinkCable_full/
make rebuild
cp LinkCable_full.gba ../
cd ..

cd LinkCable_stress/
make rebuild
cp LinkCable_stress.gba ../
cd ..

cd LinkCableMultiboot_demo/
make rebuild
cp LinkCableMultiboot_demo.mb.gba ../
cd ..

cd LinkGPIO_demo/
make rebuild
cp LinkGPIO_demo.gba ../
cd ..

cd LinkSPI_demo/
make rebuild
cp LinkSPI_demo.gba ../
cd ..

cd LinkUniversal_basic/
make rebuild
cp LinkUniversal_basic.gba ../
cd ..

cd LinkCable_full/
sed -i -e "s/\/\/ #define USE_LINK_UNIVERSAL/#define USE_LINK_UNIVERSAL/g" src/main.h
make rebuild
cp LinkCable_full.gba ../LinkUniversal_full.gba
sed -i -e "s/#define USE_LINK_UNIVERSAL/\/\/ #define USE_LINK_UNIVERSAL/g" src/main.h
cd ..

cd LinkCable_stress/
sed -i -e "s/\/\/ #define USE_LINK_UNIVERSAL/#define USE_LINK_UNIVERSAL/g" src/main.h
make rebuild
cp LinkCable_stress.gba ../LinkUniversal_stress.gba
sed -i -e "s/#define USE_LINK_UNIVERSAL/\/\/ #define USE_LINK_UNIVERSAL/g" src/main.h
cd ..

cd LinkWireless_demo/
make rebuild
cp LinkWireless_demo.gba ../
cd ..
