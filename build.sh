#!/bin/bash
##
#/****************************************************************************
#  WiiUFtpServer (fork of FTP everywhere by Laf111@2021)
# ***************************************************************************/
VERSION_MAJOR=6
VERSION_MINOR=3
VERSION_PATCH=0
export WiiuFtpServerVersion=$VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH

buildDate=$(date  +"%Y%m%d%H%M%S")

clear
uname -a
date  +"%Y-%m-%dT%H:%M:%S"
echo " "
echo =======================
echo - WiiUFtpServer $WiiuFtpServerVersion                           -
echo =======================
echo " "
echo checking env ...

check=$(env | grep "DEVKITPRO")
if [ "$check" == "" ]; then
    echo "ERROR: DEVKITPRO not found in the current environement !"
    echo "       Please define DEVKITPRO"
    more ./Makefile | grep "Please set DEVKITPRO"
    exit 100
else
    if [ -d $DEVKITPRO ]; then

        echo ">DEVKITPRO=$DEVKITPRO"
        if [ -f $DEVKITPRO/installed.ini ]; then
            echo ">"$(more $DEVKITPRO/installed.ini | grep -i "Version=")
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

rm -f ./_sdCard/wiiu/apps/WiiUFtpServer/WiiUFtpServer.rpx > /dev/null 2>&1
rm -f ./_loadiine/0005000010050421/code/WiiUFtpServer.rpx > /dev/null 2>&1

echo =====================================================

echo "building..."
echo -----------------------------------------------------
make clean
make
if [ $? -eq 0 ]; then
    # set version in ./_sdCard/wiiu/apps/WiiUFtpServer/meta.xml
    sed -i "s|<version>.*<|<version>$WiiuFtpServerVersion<|g" ./_sdCard/wiiu/apps/WiiUFtpServer/meta.xml
    find ./_sdCard/wiiu/apps/WiiUFtpServer/NandBackup -name dummy.txt -exec rm -f {} \; > /dev/null 2>&1
    sed -i "s|release_date>00000000000000|release_date>$buildDate|g" ./_sdCard/wiiu/apps/WiiUFtpServer/meta.xml

    echo -----------------------------------------------------
    echo ""
    # set version in ./_loadiine/0005000010050421/meta/meta.xml
    withNoDot=$(echo $WiiuFtpServerVersion | sed "s|\.||g")
    sed -i "s|>.*</title_version|>$withNoDot</title_version|g" ./_loadiine/0005000010050421/meta/meta.xml
    # set version in ./_loadiine/0005000010050421/code/app.xml
    sed -i "s|>.*</title_version|>$withNoDot</title_version|g" ./_loadiine/0005000010050421/code/app.xml
    
    cp -rf ./WiiUFtpServer.rpx ./_sdCard/wiiu/apps/WiiUFtpServer > /dev/null 2>&1
    echo "HBL package in ./_sdCard/wiiu/apps/WiiUFtpServer : "$(ls ./_sdCard/wiiu/apps/WiiUFtpServer)
    
    mv -f ./WiiUFtpServer.rpx ./_loadiine/0005000010050421/code > /dev/null 2>&1
    echo "RPX package in ./_loadiine/0005000010050421 : "$(ls ./_loadiine/0005000010050421)
    echo ""
    echo "Use ./toWUP/createChannel.bat in a windows cmd to create the WUP package"
    echo ""
else
    echo ""
    echo ERRORS happened when building RPX file, exit 2
    exit 2
fi

echo =====================================================
echo done sucessfully, exit 0
exit 0
