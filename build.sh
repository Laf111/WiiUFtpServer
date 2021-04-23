#!/bin/bash
##
VERSION_MAJOR=1
VERSION_MINOR=1
VERSION_PATCH=0
export WiiuFtpServerVersion=$VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH
clear
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
rm -f ./_sdCard/wiiu/apps/WiiUFtpServer/install/WUP-P-WIIUFTPSERVER/code/WiiUFtpServer.rpx > /dev/null 2>&1
rm -rf ./WiiUFtpServer.* > /dev/null 2>&1
make clean

echo "build ..."
echo ""
echo -----------------------------------------------------
make
if [ $? -eq 0 ]; then
    mv -f ./WiiUFtpServer.elf ./build > /dev/null 2>&1

    echo -----------------------------------------------------
    cp -rf ./WiiUFtpServer.rpx ./_sdCard/wiiu/apps/WiiUFtpServer > /dev/null 2>&1
    echo "HBL package in ./_sdCard/wiiu/apps/WiiUFtpServer : "$(ls ./_sdCard/wiiu/apps/WiiUFtpServer)
    echo -----------------------------------------------------
    mv -f ./WiiUFtpServer.rpx ./_sdCard/install/WUP-P-WIIUFTPSERVER/code > /dev/null 2>&1
    echo "WUP package in ./_sdCard/install/WUP-P-WIIUFTPSERVER : "$(ls ./_sdCard/install/WUP-P-WIIUFTPSERVER)
    echo =====================================================
    echo done sucessfully, exit 0
    exit 0
else
    echo ERRORS happened, exit 1
    exit 1
fi