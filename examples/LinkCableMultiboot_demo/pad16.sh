#!/bin/bash

SIZE=$(wc -c < $1)
DIFF=$(($SIZE % 16))
if (($DIFF > 0)); then
	PAD_NEEDED=$((16 - $DIFF))
	dd if=/dev/zero bs=1 count=$PAD_NEEDED >> $1
fi
