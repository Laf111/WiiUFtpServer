# WiiUFtpServer
FTP server based on libWUT and libIOSUHAX.

(works with MOCHA and HAXCHI)

<p align="center">
  <img src="WiiUFtpServer.png">
</p>


It **fixes remaining files injection failures** when using ftp-everywhere (such as saves for WWHD*).

The **performance (bandwidth)** of the server is close to be **doubled** compared to ftpiiU based on dynamics libs.

<p align="center">
  <img src="bandwith.png">
</p>

Comes with all necessary files (emebeded libWut and compile sources of libIOSUHAX). 
No dependencies to set.

The build process creates : HBL app + WUP package (to install it on the Wii-U menu as a channel)

\* Few games such as WWHD check the save files'rights and refuse to import them if permissions rights are not set using IOSUHAX_FSA_ChangeMode.

#
# BUILD :

Install devkitPro (in DEVKITPRO_PATH)

Launch "msys2\msys2_shell.bat"

export DEVKITPRO=$DEVKITPRO_PATH

cd WiiUFtpServer

./build.sh

