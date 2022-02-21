@echo off
REM ****************************************************************************
REM  WiiUFtpServer SRC checker
REM  2022-02-20:Laf111:V1-0: Create file
REM ****************************************************************************
setlocal EnableExtensions
REM : ------------------------------------------------------------------
REM : main

    setlocal EnableDelayedExpansion
    color 4F
    title WiiuFtpServer compute CRC

    REM : set current char codeset
    call:setCharSet

    set "THIS_SCRIPT=%~0"
    REM : directory of this script
    set "SCRIPT_FOLDER="%~dp0"" && set "HERE=!SCRIPT_FOLDER:\"="!"

    set "crc32exe="!HERE:"=!\crc32.exe""
    set "crc32_report="WiiuFtpServer_crc32_report.sfv""
    
    set "logFile="!HERE:"=!\checkCrc.log""
    set "crcErrorFiles="!HERE:"=!\crcErrors.txt""
    set "ignoredFiles="!HERE:"=!\ignoredFiles.txt""
    
    pushd !HERE!

    REM : checking arguments
    set /A "nbArgs=0"
    :continue
        if "%~1"=="" goto:end
        set "args[%nbArgs%]="%~1""
        set /A "nbArgs +=1"
        shift
        goto:continue
    :end

    if %nbArgs% NEQ 2 (
        echo ERROR ^: on arguments passed ^!
        echo SYNTAXE ^: "!THIS_SCRIPT!" rootFolder filePath
        echo given {%*}
        timeout /t 5 > NUL 2>&1
        exit /b 99
    )

    REM : args 1
    set "rootFolder=!args[0]!"

    if not exist !rootFolder! (
        echo ERROR ^: root folder !rootFolder! does not exist ^!
        timeout /t 8 > NUL 2>&1
        exit /b 100
    )

    REM : args 2
    set "file=!args[1]!"

    if not exist !file! (
        echo ERROR ^: input file !file! does not exist ^!
        timeout /t 8 > NUL 2>&1
        exit /b 100
    )
            
    for /F "delims=~" %%j in (!file!) do set "fileName=%%~nxj"

    REM : get folder
    for /F "delims=~" %%k in (!file!) do set "dirname="%%~dpk""
    set "str=!dirname:~0,-2!""
    set "strNoQuotes=!str:"=!"
    set "folderPathNoQuotes=!rootFolder:"=!"
    set "relativePath=!strNoQuotes:%folderPathNoQuotes%=!"
    set "relativePath=%relativePath:\=/%"
    
    for /F "delims=~" %%j in (!str!) do set "folder=%%~nxj"
    
    set "pattern=%relativePath%/!fileName!"

    set "line="NOT_FOUND""
    set /A "rc=0"
    
    for /F "delims=~" %%k in ('type !crc32_report! ^| findStr /I /R /C:"!pattern! " 2^>NUL') do (
        set "line="%%k""

        call:computeCrc32File !file! crc32
        set "crc32=!crc32: =!"
    
        echo !line! | find /I "!crc32!" > NUL 2>&1 && (
            echo !line! | find /I "<" > NUL 2>&1 && echo CRC_OK    ^: !file! uploaded sucessfully
            echo !line! | find /I "<" > NUL 2>&1 && echo CRC_OK    ^: !file! uploaded sucessfully >> !logFile!
            echo !line! | find /I ">" > NUL 2>&1 && echo CRC_OK    ^: !file! dowloaded sucessfully
            echo !line! | find /I ">" > NUL 2>&1 && echo CRC_OK    ^: !file! dowloaded sucessfully >> !logFile!
            exit /b 0
        )

        echo !line! | find /I "<" > NUL 2>&1 && echo CRC_ERROR ^: !file! upload failed ^(crc_PC = !crc32!^)
        echo !line! | find /I ">" > NUL 2>&1 && echo CRC_ERROR ^: !file! download failed ^(crc_PC = !crc32!^)
        echo !line! | find /I "<" > NUL 2>&1 && echo CRC_ERROR ^: !file! upload failed ^(crc_PC = !crc32!^) >> !crcErrorFiles!
        echo !line! | find /I ">" > NUL 2>&1 && echo CRC_ERROR ^: !file! download failed ^(crc_PC = !crc32!^) >> !crcErrorFiles!
        exit /b 2
    )
    if [!line!] == ["NOT_FOUND"] (
        echo CRC_INFO  : !file! not found in WiiuFtpServer CRC32 report           
        echo CRC_INFO  : !file! not found in WiiuFtpServer CRC32 report >> !ignoredFiles!          
        exit /b 1
    )        
    
    exit /b !rc!

REM : ------------------------------------------------------------------
REM : functions

    REM : ------------------------------------------------------------------
    :computeCrc32File

        set "fullFilePath="%~1""
        set "%2=0x00000000"

        for /F "delims=(x tokens=2" %%c in ('"!crc32exe! !fullFilePath!"') do (

            set "result="%%c""
            set "crc32=!result:(=!"
            set "crc32=!crc32:)=!"
            set "%2=%%c"
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


