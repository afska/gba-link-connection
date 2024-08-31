#!/bin/bash

FILE_INPUT="$1"
FILE_TMP="tmp.gba"
DATA="$2"
FILE_OUTPUT="$3"

if [ ! -f "$DATA" ]; then
    echo ""
    echo "The file $DATA does not exist. Run make import first!"
    echo ""
    exit 1
fi

KB=$((1024))
MAX_ROM_SIZE_KB=$((32 * $KB - 1))

ROM_SIZE=$(wc -c < $FILE_INPUT)
if [ $? -ne 0 ]; then
  exit 1
fi
GBFS_SIZE=$(wc -c < $DATA)
if [ $? -ne 0 ]; then
  exit 1
fi
ROM_SIZE_KB=$(($ROM_SIZE / $KB))
GBFS_SIZE_KB=$(($GBFS_SIZE / $KB))
MAX_REQUIRED_SIZE_KB=$(($MAX_ROM_SIZE_KB - $GBFS_SIZE_KB))
if (( $MAX_REQUIRED_SIZE_KB < $ROM_SIZE_KB )); then
    echo ""
    echo "[!] ERROR:"
    echo "ROM/GBFS file too big."
    echo ""
    echo "GBFS_SIZE_KB=$GBFS_SIZE_KB"
    echo "ROM_SIZE_KB=$ROM_SIZE_KB"
    echo "(MAX_ROM_SIZE_KB=$MAX_ROM_SIZE_KB)"
    echo ""
    exit 1
fi

cp $FILE_INPUT $FILE_TMP
if [ $? -ne 0 ]; then
  exit 1
fi
SIZE=$(wc -c < $FILE_TMP)
DIFF=$(($SIZE % 1024))
if (($DIFF > 0)); then
	PAD_NEEDED=$((1024 - $DIFF))
	dd if=/dev/zero bs=1 count=$PAD_NEEDED >> $FILE_TMP
fi
if [ $? -ne 0 ]; then
  exit 1
fi
cat $FILE_TMP $DATA > $FILE_OUTPUT
if [ $? -ne 0 ]; then
  exit 1
fi
rm $FILE_TMP
