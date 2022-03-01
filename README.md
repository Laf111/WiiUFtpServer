# WiiUFtpServer

**A robust and optimized FTP server for the WiiU !**


<p align="center">
  <img src="WiiUFtpServer.png">
</p>

Based on ftpiiu but with the following issues fixed : 
- **connections failures and crashs**
- file's dates (timestamps)
- **file injection (add rights on files uploaded)**
- remove the one slot limitation on upload and unlock up to **8 simultaneous transfers (download/upload)**
- **much more faster than the original**


https://user-images.githubusercontent.com/47532310/153688829-2e39085a-c96e-43cc-9c7a-caf48838b12e.mp4

Few games such as WWHD check the save files'rights and refuse to import them if permissions rights are not set.

WiiuFtpServer comes also with some **extra features** : 

- **support Wiiu PRO and Wiimote controllers**
- **you can choose to disable or enable the power saving feature**
- **enable / disable VERBOSE mode on server side**
- **mount NAND paths only if you ask for it**
- **a network unbrick feature** 
 
By default, NAND paths are not mounted. 

The very first time you mount them, you'll be asked to create a NAND system files backup (to \_sdCard\wiiu\apps\WiiUFtpServer\NandBackup). 

You can choose to create a partial one (< 3MB) or a full (system files only) NAND backup (500MB are requiered on the SD card)

In any case, **only files contained in the partial backup** will be used and **only** to recover the network in case it doesn't work anymore (leading in a white screen on reboot).

After the restoration process (boot on HBL menu, launch WiiuFtpServer and restore backup process), you will be able to start WiiuFtpServer and unbrick as usual (you can use the full backup files if you don't have a more recent one elsewhere than on the SDcard)

- **a CRC checker tool, to be sure that files were transferred sucessfully (on a CRC error, the file size could be OK)** 


https://user-images.githubusercontent.com/47532310/155005764-72990fa4-b271-4ab4-bb66-08c4c4b301bb.mp4

The CRC checker tool is in the HBL App folder (\_sdCard\wiiu\apps\WiiuFtpServer\CrcChecker).

**NOTES :**

- No user/password requiered and **only one client is allowed**

- The server does not implement the [MTDM](https://support.solarwinds.com/SuccessCenter/s/article/Enable-the-MDTM-command-to-preserve-the-original-time-stamp-of-uploaded-files?language=en_US) function (and so does not preserves files timestamps) but now displays the correct dates : 

<p align="center">
  <img src="timestamps.png">
</p>

- Recommended FTP client settings :
    - 90 sec for timeout which ensure to not timeout on transfer
    - 99 or unlimited retry number
    - 0 sec between retries 
    - Auto : ASCII/bin
    - auto : IPV4 / V6
    - allow retry in active mode (full **active mode is not working**)


#
# BUILD :

(Binairies are available in the [Releases](https://github.com/Laf111/WiiUFtpServer/releases/latest) section)


To build from scratch :

- install devkitPro (in DEVKITPRO_PATH)
- get WUT >= beta12 from https://github.com/devkitPro/wut 
- libIOSUHAX from https://github.com/wiiu-env/libiosuhax.


Launch "msys2\msys2_shell.bat" 

- export DEVKITPRO=$DEVKITPRO_PATH

- Build WUT, create $DEVKITPRO_PATH/wut folder and put lib and include folders in

- Build libIOSUHAX, create $DEVKITPRO_PATH/libiosuhax folder and put lib and include folders in

- cd to WiiUFtpServer folder

- ./build.sh


It creates a HBL App under \_sdCard\wiiu\apps\WiiUFtpServer

To create the channel version (HBC), use "toWUP\createChannel.bat"


Then just copy the \_sdCard folder content to your SD card.
