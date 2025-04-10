#!/bin/bash 

# change to current working directory
cd `dirname $0`

# just for output
clear

# target device (for supported devices see stm8l15x.h)
DEVICE=STM8L05X_MD_VL

# set make tool (if not in PATH, set complete path)
MAKE=make

# set serial upload tool and serial port (stm8gal from https://github.com/gicking/stm8gal)
BSL_LOADER=~/Öffentlich/GitHub/stm8gal/stm8gal/stm8gal
BSL_PORT=/dev/ttyUSB0

# set SWIM upload tool and device name (stm8flash from https://github.com/vdudouyt/stm8flash)
SWIM_LOADER=stm8flash
SWIM_TOOL=stlinkv2
SWIM_DEVICE=stm8l052c6

# target hexfile
TARGET=./$DEVICE/$DEVICE.hex

# make project
echo "make application"
$MAKE -f Makefile DEVICE=$DEVICE
if [ $? -ne 0 ]; then
    echo " "
    read -p "press key to clean build..."
    ./_UX_clean.sh
  else
    echo "done with application"
    echo " "

    # upload using STM8 serial bootloader (stm8gal from https://github.com/gicking/stm8gal)
    #$BSL_LOADER -p $BSL_PORT -w $TARGET -v

    # upload using SWIM debug interface (stm8flash from https://github.com/vdudouyt/stm8flash)
    $SWIM_LOADER -c $SWIM_TOOL -w $TARGET -p $SWIM_DEVICE
fi

echo " "
echo " "
