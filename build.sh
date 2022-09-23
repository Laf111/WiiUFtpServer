#!/bin/bash
##
#/****************************************************************************
#  WiiUFtpServer (fork of FTP everywhere by Laf111@2021)
# ***************************************************************************/
VERSION_MAJOR=12
VERSION_MINOR=1
VERSION_PATCH=0
export WiiUFtpServerVersion=$VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH

buildDate=$(date  +"%Y%m%d%H%M%S")

clear
uname -a
date  +"%Y-%m-%dT%H:%M:%S"
echo " "
echo ========================
echo - WiiUFtpServer $WiiUFtpServerVersion                           -
echo ========================
echo " "
echo checking env ...
echo -----------------------------------------------------

check=$(env | grep "DEVKITPRO")
if [ "$check" == "" ]; then
    echo "ERROR: DEVKITPRO not found in the current environement !"
    echo "       Please define DEVKITPRO"
    more ./Makefile | grep "Please set DEVKITPRO"
    exit 100
else
    if [ -d $DEVKITPRO ]; then
        if [ -f $DEVKITPRO/installed.ini ]; then
            echo "DEVKITPRO  : ["$(more $DEVKITPRO/installed.ini | grep -i "Version=" | sed "s|Version=||g")"]        in $DEVKITPRO"
            line=$(more $DEVKITPRO/wut/include/wut.h | grep "* wut")
            # wutVersion=${line##*" * wut "}
            wutVersion="1.1.1"
            echo "WUT        : [$wutVersion]         in $DEVKITPRO/wut"
            echo "IOSUHAX    : [crementif]     in $DEVKITPRO/iosuhax"
            echo "Fat        : [crementif]     in $DEVKITPRO/fat"
        fi
    else
        echo "$DEVKITPRO is invalid"
        exit 101
    fi
fi
more makefile | grep -v "#" | grep "CFLAGS" | grep "DLOG2FILE" > /dev/null 2>&1 && echo " " && echo "> log to sd/wiiu/apps/WiiUFtpServer/WiiuFtpServer.log"

check=$(env | grep "DEVKITPPC")
if [ "$check" == "" ]; then
    export DEVKITPPC=$DEVKITPRO/devkitPPC
fi

rm -f ./_sdCard/wiiu/apps/WiiUFtpServer/WiiUFtpServer.rpx > /dev/null 2>&1
rm -f ./_loadiine/0005000010050421/code/WiiUFtpServer.rpx > /dev/null 2>&1

echo =====================================================

echo "building..."
echo -----------------------------------------------------
make clean
make
if [ $? -eq 0 ]; then
    # set version in ./_sdCard/wiiu/apps/WiiUFtpServer/meta.xml
    sed -i "s|<version>.*<|<version>$WiiUFtpServerVersion<|g" ./_sdCard/wiiu/apps/WiiUFtpServer/meta.xml
    find ./_sdCard/wiiu/apps/WiiUFtpServer/NandBackup -name dummy.txt -exec rm -f {} \; > /dev/null 2>&1
    sed -i "s|release_date>[0-9]\{14\}|release_date>$buildDate|g" ./_sdCard/wiiu/apps/WiiUFtpServer/meta.xml

    echo -----------------------------------------------------
    echo ""
    # set version in ./_loadiine/0005000010050421/meta/meta.xml
    withNoDot=$(echo $WiiUFtpServerVersion | sed "s|\.||g")
    sed -i "s|>.*</title_version|>$withNoDot</title_version|g" ./_loadiine/0005000010050421/meta/meta.xml
    # set version in ./_loadiine/0005000010050421/code/app.xml
    sed -i "s|>.*</title_version|>$withNoDot</title_version|g" ./_loadiine/0005000010050421/code/app.xml
    
    cp -rf ./WiiUFtpServer.rpx ./_sdCard/wiiu/apps/WiiUFtpServer > /dev/null 2>&1
    echo "HBL package in ./_sdCard/wiiu/apps/WiiUFtpServer : "$(ls ./_sdCard/wiiu/apps/WiiUFtpServer | grep -v "wuhb")
    
    # create the WUHB files (tiramusi / aroma)
    if [ -f $DEVKITPRO/tools/bin/wuhbtool.exe ]; then
        $DEVKITPRO/tools/bin/wuhbtool.exe "WiiUFtpServer.rpx" ".\_sdCard\wiiu\apps\WiiUFtpServer\WiiUFtpServer.wuhb" --name=WiiUFtpServer  --short-name=WiiUFtpServer --author=Laf111 --icon=".\_loadiine\0005000010050421\meta\iconTex.tga"  --tv-image=".\_loadiine\0005000010050421\meta\bootTvTex.tga"  --drc-image=".\_loadiine\0005000010050421\meta\bootDrcTex.tga" > /dev/null 2>&1
        if [ $? -ne 0 ]; then 
            echo "ERROR when converting RPX to WUHB !"
            exit 3
        else
            echo "WUHB file in   ./_sdCard/wiiu/apps/WiiUFtpServer : "$(ls ./_sdCard/wiiu/apps/WiiUFtpServer | grep "wuhb")
        fi
    fi
    
    echo ""
    mv -f ./WiiUFtpServer.rpx ./_loadiine/0005000010050421/code > /dev/null 2>&1
    java -version > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        ./ToWUP/createChannel.sh
        if [ $? -ne 0 ]; then
            echo ERRORS happened when creating WUP channel
            exit 4
        fi
        echo "Copy _sdCard content to your SD card"        
    else
        echo "Use ./toWUP/createChannel script to create the WUP package (java is requiered)"
        echo "Then copy _sdCard content to your SD card"
    fi
    echo ""

    
else
    echo ERRORS happened when building RPX file
    exit 2
fi

echo =====================================================
echo done sucessfully, exit 0
exit 0
