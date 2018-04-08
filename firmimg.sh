#!/bin/sh
# Based on reverse engineering information of Matt blog
# Link : http://blog.stuffbymatt.ca/Firmware/dell_idrac6.html

if [ ! -f /tmp/ESM_Firmware_9GJYW_WN32_2.90_A00.EXE ]; then
	wget https://downloads.dell.com/FOLDER04448404M/1/ESM_Firmware_9GJYW_WN32_2.90_A00.EXE -P /tmp
	unzip -p /tmp/ESM_Firmware_9GJYW_WN32_2.90_A00.EXE payload/firmimg.d6 > firmimg.d6
fi

# Firmware image format
headerOffset=0
headerSize=512
headerReserved=512
uImageOffset=$((headerOffset + headerReserved))
uImageSize=4479904
uImageReserved=4480000
cramfsOffset=$((uImageOffset + uImageReserved))
cramfsSize=52203520
cramfsReserved=52203520
unknownDataOffset=$((cramfsOffset + cramfsReserved))
unknownDataSize=142904
unknownDataReserved=142904
firmimgOffset=$headerOffset
firmimgSize=$((unknownDataOffset + unknownDataReserved))
firmimgReserved=$((firmimgSize - 1))

fileSize=`du -b firmimg.d6 |cut -f1`

if [ $firmimgSize -eq $fileSize ]
then
	fileStatus="OK"
else
	fileStatus="FAILED"
fi

# Debug
#binwalk firmimg.d6

cat << EOF
----------------------------------------
Dell iDRAC Monolithic Release 2.90
----------------------------------------
Header offset		: $headerOffset
Header size		: $headerSize
uImage offset		: $uImageOffset
uImage size		: $uImageSize
cramfs offset		: $cramfsOffset
cramfs size		: $cramfsSize
Unknown data offset	: $unknownDataOffset
Unknown data size	: $unknownDataSize
Total size		: $firmimgSize

File size		: $fileSize
Firmware image status	: $fileStatus
----------------------------------------
EOF

echo "Extracting firmware..."

mkdir -p data

# Extract header
echo "[0/4] Extracting header..."
dd if=firmimg.d6 of=data/header.bin bs=1 skip=$headerSize count=$headerSize status=progress
echo "[1/4] Header extracted !"

# Extract uImage
echo "[1/4] Extracting uImage..."
dd if=firmimg.d6 of=data/uImage bs=1 skip=$uImageOffset count=$uImageSize status=progress
echo "[2/4] Complete !"

# Extract cramfs
echo "[2/4] Extracting cramfs..."
dd if=firmimg.d6 of=data/cramfs bs=1 skip=$cramfsOffset count=$cramfsSize status=progress
echo "[3/4] Complete !"

# Extract unknown data
echo "[3/4] Extracting unknown data..."
dd if=firmimg.d6 of=data/unknown_data.bin bs=1 skip=$unknownDataOffset count=$unknownDataSize status=progress
echo "[4/4] Complete !"

echo "Firmware extracted !"
