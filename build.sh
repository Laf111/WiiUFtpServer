#!/bin/bash
##
#/****************************************************************************
#  WiiUFtpServer (fork of FTP everywhere by Laf111@2021)
# ***************************************************************************/
VERSION_MAJOR=12
VERSION_MINOR=2
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
        fi
    else
        echo "$DEVKITPRO is invalid"
        exit 101
    fi
fi

check=$(env | grep "DEVKITPPC")
if [ "$check" == "" ]; then
    export DEVKITPPC=$DEVKITPRO/devkitPPC
fi

rm -f ./_sdCard/wiiu/apps/WiiUFtpServer/WiiUFtpServer.elf > /dev/null 2>&1

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
    
    cp -rf ./WiiUFtpServer.elf ./_sdCard/wiiu/apps/WiiUFtpServer > /dev/null 2>&1
    echo "HBL package in ./_sdCard/wiiu/apps/WiiUFtpServer : "$(ls ./_sdCard/wiiu/apps/WiiUFtpServer)
    
else
    echo ERRORS happened when building RPX file, exit 2
    exit 2
fi

echo =====================================================
echo done sucessfully, exit 0
exit 0
