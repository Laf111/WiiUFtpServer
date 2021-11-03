# WiiUFtpServer
**A new fork of ftpiiu that fixes files injection, display files timestamps correctly and much more faster than the original software.**

<p align="center">
  <img src="WiiUFtpServer.png">
</p>


It **fixes remaining files injection failures** when using ftp-everywhere.

Few games such as WWHD check the save files'rights and refuse to import them if permissions rights are not set using IOSUHAX_FSA_ChangeMode.

It comes with some **extra features** : 

- **support Wiiu PRO and Wiimote controllers**
- **you can choose to disable or enable the power saving feature**
- **enable / disable VERBOSE mode on server side**
- **mount NAND paths only if you ask for it**
- **a network unbrick feature** 
 
By default, NAND paths are not mounted. 

The very first time you mount them, you'll be asked to create a NAND system files backup (to \_sdCard\wiiu\apps\WiiUFtpServer\NandBackup). 

You can choose to create a partial one (< 3MB) or a full (system files only) NAND backup (500MB are requiered on the SD card)

In any case, **only files contained in the partial backup** will be used and **only** to recover the network in case it don't work anymore.

After the restoration process, you will be able to start WiiuFtpServer and unbrick as usual (you can use the full backup files if you don't have a more recent one)


**Speeds compared to FTP-everywhere** : (Wii-U with Ethernet adapter - NAS - Laptop using Wifi)

<p align="center">
  <img src="bandwith.png">
</p>


**NOTES :**


- The server does not implement the [MTDM](https://support.solarwinds.com/SuccessCenter/s/article/Enable-the-MDTM-command-to-preserve-the-original-time-stamp-of-uploaded-files?language=en_US) function (and so does not preserves files timestamps) but now displays the correct dates : 

<p align="center">
  <img src="timestamps.png">
</p>

- The FTP method used is I/O multiplexing (blocking I/O). 

- This **FTP server is limited to 1 unique client** (more safer) and one unique transfer slot for up/download (fix deconnexion issues) but you can **set the number of simultaneous transfers to the max on your FTP client : the connections will not timed out and this will speed the overhall transfer time**.

**When using Ethernet** : (Wii-U with Ethernet adapter - NAS - Laptop Ethernet 5)

<p align="center">
  <img src="Ethernet.png">
</p>


#
# BUILD :

Binairies are available in the [Releases](https://github.com/Laf111/WiiUFtpServer/releases/latest) section.


Install devkitPro (in DEVKITPRO_PATH) and download the libIOSUHAX from YaWut repository.

Launch "msys2\msys2_shell.bat" 
Get WUT with command : pacman -Syu devkitPPC wut-tools wut

Build libIOSUHAX, create $DEVKITPRO_PATH/libiosuhax folder and put lib and include folders in.

- export DEVKITPRO=$DEVKITPRO_PATH

- cd WiiUFtpServer

- ./build.sh

It creates a HBL App under \_sdCard\wiiu\apps\WiiUFtpServer

To create the channel version (HBC), use "toWUP\createChannel.bat"

Then copy the \_sdCard folder content to your SD card.
