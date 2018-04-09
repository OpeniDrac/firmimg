#!/bin/sh
# Based on reverse engineering information of Matt blog
# Link : http://blog.stuffbymatt.ca/Firmware/dell_idrac6.html

PWD=`pwd`

if [ ! -f /tmp/ESM_Firmware_9GJYW_WN32_2.90_A00.EXE ]; then
	wget https://downloads.dell.com/FOLDER04448404M/1/ESM_Firmware_9GJYW_WN32_2.90_A00.EXE -P /tmp
	unzip -p /tmp/ESM_Firmware_9GJYW_WN32_2.90_A00.EXE payload/firmimg.d6 > firmimg.d6
fi

cramfsToolsDir=${PWD}/cramfs
dataDir=${PWD}/data

firmimgFile=${PWD}/firmimg.d6
headerDataFile=${dataDir}/header.bin
uImageDataFile=${dataDir}/uImage
cramfsDataFile=${dataDir}/cramfs
unknownDataFile=${dataDir}/unknown.bin

idracfsDir=${PWD}/idracfs

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

fileSize=`du -b $firmimgFile |cut -f1`

if [ $firmimgSize -eq $fileSize ]; then
	fileStatus="OK"
else
	fileStatus="Incorrect file size"
fi

# Debug
#binwalk firmimg.d6

reverse_crc32() {
	_crc32=()
        for i in {3..9..2}; do
                 _crc32+=(`echo $1 | tail -c$i | head -c2`)
        done

	returnCRC32=`echo ${_crc32[*]} | tr -d ' '`

	eval "$2='$returnCRC32'"
}

extract() {
	make -C $cramfsToolsDir

	echo "Extracting firmware..."

	mkdir -p $dataDir

	# Extract header
	echo "[0/4] Extracting header..."
	if [ ! -f $headerDataFile ]; then
		dd if=$firmimgFile of=$headerDataFile bs=1 skip=$headerOffset count=$headerSize status=progress
	fi

	echo "[1/4] Header extracted !"

	# Extract uImage
	echo "[1/4] Extracting uImage..."
	if [ ! -f $uImageDataFile ]; then
		dd if=$firmimgFile of=$uImageDataFile bs=1 skip=$uImageOffset count=$uImageSize status=progress
	fi

	echo "[2/4] Complete !"

	# Extract cramfs
	echo "[2/4] Extracting cramfs..."
	if [ ! -f $cramfsDataFile ]; then
		dd if=$firmimgFile of=$cramfsDataFile bs=1 skip=$cramfsOffset count=$cramfsSize status=progress
	fi

	echo "[3/4] Complete !"

	# Extract unknown data
	echo "[3/4] Extracting unknown data..."
	if [ ! -f $unknownDataFile ]; then
		dd if=$firmimgFile of=$unknownDataFile bs=1 skip=$unknownDataOffset count=$unknownDataSize status=progress
	fi

	echo "[4/4] Complete !"

	echo "Firmware extracted !"
	echo "Extracting cramfs..."

	# Extract cramfs
	if [ ! -d ${idracfsDir} ]; then
		sudo ${cramfsToolsDir}/cramfsck -x ${idracfsDir} ${cramfsDataFile}
	fi

	echo "cramfs extracted !"

	headerDataCRC32=`crc32 $headerDataFile`
	uImageDataCRC32=`crc32 $uImageDataFile`
	cramfsDataCRC32=`crc32 $cramfsDataFile`
	unknownDataCRC32=`crc32 $unknownDataFile`

	cramfsCRC32=''
	reverse_crc32 $cramfsDataCRC32 cramfsCRC32

	headerCramfsCRC32=`hexdump -s 0x34 -n 4 -e '4/1 "%x" "\n"' $headerDataFile`

	if [ "$cramfsCRC32" == "$headerCramfsCRC32" ]; then
		cramfsChecksumStatus="OK"
	else
		cramfsChecksumStatus="CHECK FAILED"
	fi

cat << EOF
----------------------------------------
CRC32 checksum list :
Header file		: $headerDataCRC32
uImage file		: $uImageDataCRC32
cramfs file		: $cramfsDataCRC32
unknown	data file	: $unknownDataCRC32
----------------------------------------
Header :
cramfs reversed CRC32	: $headerCramfsCRC32
----------------------------------------
cramfs checksum status	: $cramfsChecksumStatus
----------------------------------------
EOF
}

build() {
	echo "Build"
}

help() {
cat << EOF
firmimg.sh [COMMAND]

Command:
	extract		Extract cramfs of firmimg.d6
	build		Build firmimg.d6 of cramfs directory
	help		Show help
EOF
}

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

if [ "$1" == "extract" ]; then
	extract
elif [ "$1" == "build" ]; then
	build
elif [ "$1" == "help" ]; then
	help
else
	echo "Unknown command"
fi

