#!/bin/bash

set -e

MODE="${1:-native}" # native | docker

cmd_make() {
  if [ "$MODE" = docker ]; then
    docker run -it \
      --user "$(id -u):$(id -g)" \
      -v "$(pwd)/../..":/opt/gba \
      devkitpro/devkitarm:20241104 \
      bash -c "cd /opt/gba/examples/$(basename "$PWD") && make \"\$@\"" -- "$@"
  else
    make "$@"
  fi
}

compile() {
  if [ "$MODE" = docker ]; then
    echo "Compiling in Docker mode..."
  else
    echo "Compiling in Native mode..."
  fi

  if [ "$1" = "multiboot" ]; then
    args="bMB=1"
    suffix=".mb"
    folder="multiboot"
  else
    args="bMB=0"
    suffix=""
    folder="."
  fi

  # LinkCable_basic
  cd LinkCable_basic/
  cmd_make rebuild $args
  cp LinkCable_basic$suffix.gba ../$folder/
  cd ..

  # LinkCable_full
  cd LinkCable_full/
  cmd_make rebuild $args
  cp LinkCable_full$suffix.gba ../$folder/
  cd ..

  # LinkCable_stress
  cd LinkCable_stress/
  cmd_make rebuild $args
  cp LinkCable_stress$suffix.gba ../$folder/
  cd ..

  # LinkCard_demo
  cd LinkCard_demo/
  cmd_make rebuild $args
  cp LinkCard_demo$suffix.out.gba ../$folder/LinkCard_demo$suffix.gba
  cd ..

  # LinkCube_demo
  cd LinkCube_demo/
  cmd_make rebuild $args
  cp LinkCube_demo$suffix.gba ../$folder/
  cd ..

  # LinkGPIO_demo
  cd LinkGPIO_demo/
  cmd_make rebuild $args
  cp LinkGPIO_demo$suffix.gba ../$folder/
  cd ..

  # LinkIR_demo
  cd LinkIR_demo/
  cmd_make rebuild $args
  cp LinkIR_demo$suffix.gba ../$folder/
  cd ..

  # LinkMobile_demo
  cd LinkMobile_demo/
  cmd_make rebuild $args
  cp LinkMobile_demo$suffix.gba ../$folder/
  cd ..

  # LinkPS2Keyboard_demo
  cd LinkPS2Keyboard_demo/
  cmd_make rebuild $args
  cp LinkPS2Keyboard_demo$suffix.gba ../$folder/
  cd ..

  # LinkPS2Mouse_demo
  cd LinkPS2Mouse_demo/
  cmd_make rebuild $args
  cp LinkPS2Mouse_demo$suffix.gba ../$folder/
  cd ..

  # LinkRawCable_demo
  cd LinkRawCable_demo/
  cmd_make rebuild $args
  cp LinkRawCable_demo$suffix.gba ../$folder/
  cd ..

  # LinkRawWireless_demo
  cd LinkRawWireless_demo/
  cmd_make rebuild $args
  cp LinkRawWireless_demo$suffix.gba ../$folder/
  cd ..

  # LinkSPI_demo
  cd LinkSPI_demo/
  cmd_make rebuild $args
  cp LinkSPI_demo$suffix.gba ../$folder/
  cd ..

  # LinkUART_demo
  cd LinkUART_demo/
  cmd_make rebuild $args
  cp LinkUART_demo$suffix.gba ../$folder/
  cd ..

  # LinkUniversal_basic
  cd LinkUniversal_basic/
  cmd_make rebuild $args USERFLAGS="-DLINK_WIRELESS_PUT_ISR_IN_IWRAM=1"
  cp LinkUniversal_basic$suffix.gba ../$folder/
  cd ..

  # LinkUniversal_full
  cd LinkCable_full/
  mv LinkCable_full$suffix.gba backup.gba || :
  cmd_make rebuild $args USERFLAGS="-DUSE_LINK_UNIVERSAL=1 -DLINK_WIRELESS_PUT_ISR_IN_IWRAM=1"
  cp LinkCable_full$suffix.gba ../$folder/LinkUniversal_full$suffix.gba
  mv backup.gba LinkCable_full$suffix.gba || :
  cd ..

  # LinkUniversal_stress
  cd LinkCable_stress/
  mv LinkCable_stress$suffix.gba backup.gba || :
  cmd_make rebuild $args USERFLAGS="-DUSE_LINK_UNIVERSAL=1 -DLINK_WIRELESS_PUT_ISR_IN_IWRAM=1"
  cp LinkCable_stress$suffix.gba ../$folder/LinkUniversal_stress$suffix.gba
  mv backup.gba LinkCable_stress$suffix.gba || :
  cd ..

  # LinkWireless_demo
  cd LinkWireless_demo/
  cmd_make rebuild $args USERFLAGS="-DLINK_WIRELESS_PUT_ISR_IN_IWRAM=1"
  cp LinkWireless_demo$suffix.gba ../$folder/
  cd ..

  # LinkWireless_prof_code_iwram
  cd LinkWireless_demo/
  mv LinkWireless_demo$suffix.gba backup.gba || :
  cmd_make rebuild $args USERFLAGS="-DLINK_WIRELESS_PUT_ISR_IN_IWRAM=1 -DLINK_WIRELESS_PROFILING_ENABLED=1"
  cp LinkWireless_demo$suffix.gba ../$folder/LinkWireless_prof_code_iwram$suffix.gba
  mv backup.gba LinkWireless_demo$suffix.gba || :
  cd ..

  # LinkWireless_prof_code_rom
  cd LinkWireless_demo/
  mv LinkWireless_demo$suffix.gba backup.gba || :
  cmd_make rebuild $args USERFLAGS="-DLINK_WIRELESS_PROFILING_ENABLED=1"
  cp LinkWireless_demo$suffix.gba ../$folder/LinkWireless_prof_code_rom$suffix.gba
  mv backup.gba LinkWireless_demo$suffix.gba || :
  cd ..
}

# Cleanup
rm -rf multiboot/
mkdir -p multiboot/
rm -f *.gba *.sav *.sa2

# Compile all ROMs as multiboot
compile "multiboot"
cd multiboot/
cp ../hello.gbfs .
ungbfs hello.gbfs
rm hello.gbfs
for file in *.gba; do
  ../pad16.sh "$file"
done
mv Hello.gba _hello.gba
for file in Link*.gba; do
  mv "$file" "${file#Link}"
done
gbfs roms.gbfs *
for file in *.gba; do
  mv "$file" "Link$file"
done
cd ..

# Bundle all multiboot ROMs in the multiboot launchers
cp multiboot/roms.gbfs LinkCableMultiboot_demo/content.gbfs
cp multiboot/roms.gbfs LinkWirelessMultiboot_demo/content.gbfs

# LinkCableMultiboot_demo
cd LinkCableMultiboot_demo/
cmd_make rebuild
cp LinkCableMultiboot_demo.out.gba ../LinkCableMultiboot_demo.gba
cp ../hello.gbfs content.gbfs
cd ..

# LinkWirelessMultiboot_demo
cd LinkWirelessMultiboot_demo/
cmd_make rebuild USERFLAGS="-DLINK_WIRELESS_MULTIBOOT_ENABLE_LOGGING=1"
cp LinkWirelessMultiboot_demo.out.gba ../LinkWirelessMultiboot_demo.gba
cp ../hello.gbfs content.gbfs
cd ..

# Compile all ROMs as normal
compile
