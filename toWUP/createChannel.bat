@echo off
setlocal EnableExtensions
REM : ------------------------------------------------------------------
REM : main

    setlocal EnableDelayedExpansion
    color 4F

    set "THIS_SCRIPT=%~0"

    title Create a WUP package from loadiine using the common Key

    REM : checking THIS_SCRIPT path
    call:checkPathForDos "!THIS_SCRIPT!" > NUL 2>&1
    set /A "cr=!ERRORLEVEL!"
    if !cr! NEQ 0 (
        echo ERROR ^: Remove DOS reserved characters from the path "!THIS_SCRIPT!" ^(such as ^&^, %% or ^^!^)^, cr=!cr!
        pause
        exit /b 1
    )

    REM : directory of this script
    set "SCRIPT_FOLDER="%~dp0"" && set "NUSPFolder=!SCRIPT_FOLDER:\"="!"

    for %%a in (!NUSPFolder!) do set "parentFolder="%%~dpa""
    set "WiiuFtpServerRoot=!parentFolder:~0,-2!""

    set "notePad="%windir%\System32\notepad.exe""

    REM : set current char codeset
    call:setCharSet

    REM : search if the script convertWiiuFiles is not already running (nb of search results)
    set /A "nbI=0"

    for /F "delims=~=" %%f in ('wmic process get Commandline 2^>NUL ^| find /I "cmd.exe" ^| find /I "createChannel.bat" ^| find /I /V "find" /C') do set /A "nbI=%%f"
    if %nbI% NEQ 0 (
        if %nbI% GEQ 2 (
            echo "ERROR^: The script createChannel is already running ^!"
            wmic process get Commandline 2>NUL | find /I "cmd.exe" | find /I "convertWiiuFiles.bat" | find /I /V "find"
            pause
            exit /b 100
        )
    )
    

    REM : check NUSP install
    set "KEY=NOT_FOUND"
    call:checkNusP
    if !ERRORLEVEL! GEQ 50 (
        echo ERROR^: in checkNusP function
        exit /b 50
    )
    cls
    echo =========================================================
    echo Convert Loadiine to WUT package
    echo =========================================================
    echo.
    echo Hit any key to launch the conversion
    echo.
    pause

    REM : cd to NUSPFolder
    pushd !NUSPFolder!

    set "INPUTDIRECTORY="!WiiuFtpServerRoot:"=!\_loadiine\0005000010050421""
    set "OUTPUTDIRECTORY="!WiiuFtpServerRoot:"=!\_sdCard\install\WiiuFtpServer""
    if exist !OUTPUTDIRECTORY! rmdir /Q /S !OUTPUTDIRECTORY! > NUL 2>&1
    mkdir !OUTPUTDIRECTORY! > NUL 2>&1
    
    del /F "NUSP.log" > NUL 2>&1
    echo java -jar NUSPacker.jar -in !INPUTDIRECTORY! -out !OUTPUTDIRECTORY! -tID 0005000010050421 -encryptKeyWith !KEY!
    echo ---------------------------------------------------------
    java -jar NUSPacker.jar -in !INPUTDIRECTORY! -out !OUTPUTDIRECTORY! -tID 0005000010050421 -encryptKeyWith !KEY!
    echo.
    echo ---------------------------------------------------------
    echo NusPacker returned = !ERRORLEVEL!
    
    set "tmpFolder="!NUSPFolder:"=!\tmp""
    set "outputFolder="!NUSPFolder:"=!\output""
    rmdir /Q /S !tmpFolder! > NUl 2>&1
    rmdir /Q /S !outputFolder! > NUl 2>&1


    :endMain
    echo =========================================================
    echo done^, WUP package created in !OUTPUTDIRECTORY!
    pause
    exit /b 0

    goto:eof

    REM : ------------------------------------------------------------------


REM : ------------------------------------------------------------------
REM : functions


    REM : check JNUST pre-requisites and installation
    :checkNusP

        REM : exit in case of no NUSPFolder folder exists
        if not exist !NUSPFolder! (
            echo ERROR^: !NUSPFolder! not found
            pause
            exit /b 50
        )
        REM : check if java is installed
        java -version > NUL 2>&1
        if !ERRORLEVEL! NEQ 0 (
            echo ERROR^: java is not installed^, exiting
            pause
            exit /b 51
        )

        set "config="!NUSPFolder:"=!\Key.txt""
        :getKey
        type !config! | find /I "[ENCRYPTING_KEY_32HEXACHARS]" > NUL 2>&1 && (
            echo.
            echo No key found in !config!
            echo.
            echo Replace the last line in !config! 
            echo with the key you want to echo use for encrypting the data.
            echo.
            echo ^(use the ^'Wii U common key^' for custom app^)
            echo.
            echo Close notepad to continue^.
            echo.
            timeout /T 3 > NUL 2>&1
            !notePad! !config!
            goto:getKey
        )
        
        for /F "delims=~" %%i in ('type !config! 2^>NUL') do set "KEY=%%i"      
        if ["!KEY!"] == ["NOT_FOUND"] (
                echo ERROR^: WiiU common key not found in !config! ^!
                pause
                exit /b 54
        )
    goto:eof
    REM : ------------------------------------------------------------------


    REM : function to detect DOS reserved characters in path for variable's expansion : &, %, !
    :checkPathForDos

        set "toCheck=%1"

        REM : if implicit expansion failed (when calling this script)
        if ["!toCheck!"] == [""] (
            echo Remove DOS reserved characters from the path %1 ^(such as ^&^, %% or ^^!^)^, exiting 13
            exit /b 13
        )

        REM : try to resolve
        if not exist !toCheck! (
            echo Remove DOS reserved characters from the path %1 ^(such as ^&^, %% or ^^!^)^, exiting 11
            exit /b 11
        )

        REM : try to list
        dir !toCheck! > NUL 2>&1
        if !ERRORLEVEL! NEQ 0 (
            echo Remove DOS reverved characters from the path %1 ^(such as ^&^, %% or ^^!^)^, exiting 12
            exit /b 12
        )

        exit /b 0
    goto:eof
    REM : ------------------------------------------------------------------


    REM : function to get and set char set code for current host
    :setCharSet

        REM : get charset code for current HOST
        set "CHARSET=NOT_FOUND"
        for /F "tokens=2 delims=~=" %%f in ('wmic os get codeset /value 2^>NUL ^| find "="') do set "CHARSET=%%f"

        if ["%CHARSET%"] == ["NOT_FOUND"] (
            echo Host char codeSet not found in %0 ^?
            pause
            exit /b 9
        )
        REM : set char code set, output to host log file

        chcp %CHARSET% > NUL 2>&1

    goto:eof
    REM : ------------------------------------------------------------------
