# WiiUFtpServer

**A robust and optimized FTP server for the WiiU !**

**NOTE :** WUT developpement is interrupted (too many buggs with no way to debug), so no more channel version for V12 and up.


<p align="center">
  <img src="WiiUFtpServer.png">
</p>

Based on ftpiiu but with the following issues fixed : 
- **connections failures and crashs**
- extend supported clients list 
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

- Under windows, enable long filenames support with setting to 1 the value of *HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled* because some files created by the Wii-U have a name with more than 128 characters (up to 170) and so the limit of 255 characters for the entire path can easily be reached.

- I opened a ticket for FileZilla client that failed to transfer such files (https://trac.filezilla-project.org/ticket/12675#ticket)

#
# BUILD :

(Binairies are available in the [Releases](https://github.com/Laf111/WiiUFtpServer/releases/latest) section)


To build from scratch :

- install [devkitPro](https://github.com/devkitPro/installer/releases/latest) (in DEVKITPRO_PATH)


Launch "msys2\msys2_shell.bat" 

- export DEVKITPRO=$DEVKITPRO_PATH
- cd to WiiUFtpServer folder
- ./build.sh


It creates a HBL App under \_sdCard\wiiu\apps\WiiUFtpServer.

Then just copy the \_sdCard folder content to your SD card.

#
# KNOWN ISSUES :

- symlinks are not displayed in FTP client browser and trying to transfer them will fail (links not resolved)
  When dumping games for CEMU, just ignore those errors (CEMU does not use them).
- libFat is used because of very poor performance on SDCard transfers using only libIOSUHAX 
