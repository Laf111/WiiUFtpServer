@echo off
REM ****************************************************************************
REM  WiiUFtpServer SRC checker
REM  2022-02-20:Laf111:V1-0: Create file
REM  2022-02-20:Laf111:V1-1: Fix minor issues and paralellize treatments (vbs scripts)
REM  2022-02-24:Laf111:V1-2: Fix false CRC errors on files with a name containing 2 dots
REM                          Performance enhancement and minor fixes
REM ****************************************************************************
set "VERSION=V1-2"
setlocal EnableExtensions
REM : ------------------------------------------------------------------
REM : main

    setlocal EnableDelayedExpansion
    color 4F
    title WiiuFtpServer CRC checker %VERSION%

    REM : set current char codeset
    call:setCharSet

    set "THIS_SCRIPT=%~0"
    REM : directory of this script
    set "SCRIPT_FOLDER="%~dp0"" && set "HERE=!SCRIPT_FOLDER:\"="!"
    set "browseFolder="!HERE:"=!\resources\vbs\BrowseFolderDialog.vbs""
    set "StartHidden="!HERE:"=!\resources\vbs\StartHidden.vbs""
    set "StartHiddenWait="!HERE:"=!\resources\vbs\StartHiddenWait.vbs""

    set "computeFileCrc="!HERE:"=!\resources\computeFileCrc.bat""
    REM : number of process max the script will try to launch simultaneously
    set /A "nbp=32"

    set "crc32_report="WiiuFtpServer_crc32_report.sfv""

    set "logFile="!HERE:"=!\checkCrc.log""
    set "crcErrorFiles="!HERE:"=!\crcErrors.txt""
    set "ignoredFiles="!HERE:"=!\ignoredFiles.txt""

    pushd !HERE!

    REM : TODO cmd -> call fix bat files ?
    set "convertedToAinsi=0"
    type !THIS_SCRIPT! | find "convertedToAinsi" | find /V "find" > NUL 2>&1 && (
        set "convertedFile=!THIS_SCRIPT:.cmd=.bat!"
        echo GitHub format file to UTF-8 breaking silently windows batch files
        echo Creating an ASCII bat file ^(AINSI^) !convertedFile!
        echo This script will deleted^.
        echo Please now use !convertedFile!
        pause
        del /F !THIS_SCRIPT!
        exit /b 90
    )

    echo  -==============================-
    echo ^| WiiuFtpServer CRC checker %VERSION% ^|
    echo  -==============================-
    echo.
    if not exist !crc32_report! (
        echo !crc32_report! not found
        echo.
        echo If WiiuFtpServer is still running^, it will be downloaded
        pause
        echo.
    )

    echo ^(logging to checkCrc.log^)

    REM : initialize the logFile
    echo  -==============================- > !logFile!
    echo ^| WiiuFtpServer CRC checker %VERSION% ^| >> !logFile!
    echo  -==============================- >> !logFile!

    REM : checking arguments
    set /A "nbArgs=0"
    :continue
        if "%~1"=="" goto:end
        set "args[%nbArgs%]="%~1""
        set /A "nbArgs +=1"
        shift
        goto:continue
    :end

    if %nbArgs% GTR 1 (
        echo ERROR ^: on arguments passed ^!
        echo SYNTAXE ^: "!THIS_SCRIPT!" folderPath
        echo given {%*}
        timeout /t 5 > NUL 2>&1
        exit /b 99
    )

    if %nbArgs% EQU 1 (
        REM : args 1
        set "folderPath=!args[0]!"

        if not exist !folderPath! (
            echo ERROR ^: input folder !folderPath! does not exist ^!
            timeout /t 8 > NUL 2>&1
            exit /b 100
        )

    ) else (
        REM : ask for it
        :askFolder
        for /F %%b in ('cscript /nologo !browseFolder! "Browse to the folder containing the files to check..."') do set "folder=%%b" && set "folderPath=!folder:?= !"
        if [!NEW_GAMES_FOLDER_PATH!] == ["NONE"] (
            choice /C yn /N /M "No item selected, do you wish to cancel (y, n)? : "
            if !ERRORLEVEL! EQU 1 timeout /T 1 > NUL 2>&1 && exit 75
            goto:askFolder
        )
    )

    del /F !crcErrorFiles! > NUL 2>&1
    del /F !ignoredFiles! > NUL 2>&1

    set "backupFolder="!HERE:"=!\FILES2FIX""
    set "oldBackupFolder="!HERE:"=!\FILES2FIX_backup""
    rmdir /Q /S !oldBackupFolder! > NUL 2>&1
    if exist !backupFolder! move /Y !backupFolder! !oldBackupFolder! > NUL 2>&1

    REM : set processes to priority to high
    wmic process where "CommandLine like '%%checkCrc%%'" call setpriority 128 > NUL 2>&1

    REM : search for WiiuFtpServer report
    :getReport
    if not exist !crc32_report! (

        REM : download it
        echo Download CRC32 report from WiiuFtpServer^.^.^.
        echo Download CRC32 report from WiiuFtpServer^.^.^. >> !logFile!

        :getIp
        set /P "wiiuIp=Please enter your Wii-U local IP adress : "
        echo !wiiuIp!| findStr /R /V "[0-9][0-9][0-9]\.[0-9\.]*$" > NUL 2>&1 && goto:getIp

        set "sfvFile="/storage_sdcard/wiiu/apps/WiiuFtpServer/!crc32_report:"=!""

        :downloadReport
        REM : create the ftp commands file
        echo get !sfvFile! > ftp.txt
        echo bye >> ftp.txt
        REM : download the report
        ftp -A -s:ftp.txt !wiiuIp!

        if not exist !crc32_report! (
            echo ERROR ^: fail to download !crc32_report!
            echo please check the IP passed and if WiiuFtpServer is running
            if %nbArgs% EQU 0 pause
            exit /b 50
        )
    )

    echo =================================================================
    echo ================================================================= >> !logFile!
    echo.
    echo input folder = !folderPath!
    echo.

    REM : display the first line of the report, confirm the use

    REM : get the FTP session date from sfv header
    set "line=NOT_FOUND"
    for /F "delims=;" %%k in ('type !crc32_report! ^| find /I "WiiuFtpServer CRC-32 report of FTP session on" 2^>NUL') do set "line=%%k"

    if not ["!line!"] == ["NOT_FOUND"] (
        echo -----------------------------------------------------------------
        echo ----------------------------------------------------------------- >> !logFile!

        echo CRC32 report file details ^:
        echo.
        echo CRC32 report file details ^: >> !logFile!
        echo. >> !logFile!

        echo !line!
        echo !line! >> !logFile!
    )

    echo -----------------------------------------------------------------
    echo ----------------------------------------------------------------- >> !logFile!
    echo Compute CRC32 and compare with server one^.^.^.
    echo Compute CRC32 and compare with server one^.^.^. >> !logFile!
    echo.
    REM : get current date
    for /F "usebackq tokens=1,2 delims=~=" %%i in (`wmic os get LocalDateTime /VALUE 2^>NUL`) do if '.%%i.'=='.LocalDateTime.' set "ldt=%%j"
    set "ldt=%ldt:~0,4%-%ldt:~4,2%-%ldt:~6,2%_%ldt:~8,2%-%ldt:~10,2%-%ldt:~12,6%"
    set "startDate=%ldt%"
    REM : starting DATE

    echo Starting at !startDate! >> !logFile!
    echo Starting at !startDate!
    echo.

    set /A "nbFiles=0"

    call:checkCrcFolder !folderPath!

    REM : get current date
    for /F "usebackq tokens=1,2 delims=~=" %%i in (`wmic os get LocalDateTime /VALUE 2^>NUL`) do if '.%%i.'=='.LocalDateTime.' set "ldt=%%j"
    set "ldt=%ldt:~0,4%-%ldt:~4,2%-%ldt:~6,2%_%ldt:~8,2%-%ldt:~10,2%-%ldt:~12,6%"
    set "date=%ldt%"
    REM : ending DATE

    echo.
    echo Started at !startDate! >> !logFile!
    echo Started at !startDate!
    echo Ending at !date! >> !logFile!
    echo Ending at !date!
    echo.
    echo !nbFiles! files treated
    echo.

    set /A "rc=0"
    if exist !ignoredFiles! (
        echo -----------------------------------------------------------------
        echo ----------------------------------------------------------------- >> !logFile!

        echo !nbIgnored! files were not found in wiiuFtpServer CRC32 report ^:
        echo !nbIgnored! files were not found in wiiuFtpServer CRC32 report ^: >> !logFile!
        echo.
        echo. >> !logFile!
        type !ignoredFiles!
        type !ignoredFiles! >> !logFile!
        set /A "rc=1"
    )

    if exist !crcErrorFiles! (
        echo -----------------------------------------------------------------
        echo ----------------------------------------------------------------- >> !logFile!
        echo !nbErrors! erros found ^:
        echo !nbErrors! found ^: >> !logFile!
        echo.
        echo. >> !logFile!
        type !crcErrorFiles!
        type !crcErrorFiles! >> !logFile!
        echo.
        echo IF YOU UPLOAD THOSE FILES ON SERVER SIDE, PLEASE TRANSFER
        echo THEM AGAIN AND RE-CHECK
        echo.
        echo !nbErrors! erros found ^:
        echo !nbErrors! found ^: >> !logFile!

        set /A "rc=2"
    )
    echo =================================================================
    echo ================================================================= >> !logFile!

    if !rc! EQU 2 echo !nbErrors! errors^, exit !rc!
    if !rc! EQU 1 echo Warning !nbIgnored! files were not found in the server report^, exit !rc!
    if !rc! EQU 0 echo Done with no errors^, exit !rc!
    REM if %nbArgs% EQU 0 pause
    pause
    exit !rc!

REM : ------------------------------------------------------------------
REM : functions

    REM : ------------------------------------------------------------------
    :checkCrcFolder

        set "fullFolderPath="%~1""

        set /A "nbL=0"
        for /F "delims=~" %%i in ('dir /a-d /s /b !fullFolderPath! 2^>NUL') do (
            set "file="%%i""

            set /A "nbL+=1" && if !nbL! LSS !nbp! (
                    wscript /nologo !StartHidden! !computeFileCrc! !fullFolderPath! !file! & set /A "nbFiles+=1"
                ) else (
                    wscript /nologo !StartHiddenWait! !computeFileCrc! !fullFolderPath! !file! & set /A "nbFiles+=1"
                    set /A "nbL=0"
                )
        )

    goto:eof


    REM : ------------------------------------------------------------------
    REM : function to get and set char set code for current host
    :setCharSet

        REM : get charset code for current HOST
        set "CHARSET=NOT_FOUND"
        for /F "tokens=2 delims=~=" %%f in ('wmic os get codeset /value 2^>NUL ^| find "="') do set "CHARSET=%%f"

        if ["%CHARSET%"] == ["NOT_FOUND"] (
            echo Host char codeSet not found ^?^, exiting 1
            exit /b 9
        )
        REM : set char code set, output to host log file

        chcp %CHARSET% > NUL 2>&1

    goto:eof
    REM : ------------------------------------------------------------------


