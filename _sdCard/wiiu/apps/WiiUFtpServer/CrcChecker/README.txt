 ----------------------------------
| CRC checker V2-2 (WiiuFtpServer) |
 ----------------------------------

  
HISTORY :
--------

2022/04/19 : V2-2
    fix answer "no" not taken into account when choosing to not use an existing report
    add a try catch for permissions errors
    fix failure when root = folder to check

2022/03/11 : V2-1
    fix "TypeError: string operation on non-string array" that could occurs when
    treating the report

 
REQUIEREMENTS :
--------------

Python 3 + numpy + tk + pywin32

For example miniconda + pip install numpy 

https://docs.conda.io/en/latest/miniconda.html



GOAL : 
-----

Look for WiiuFtpServer CRC 32 report (WiiuFtpServer_CRC32_report.sfv) close to the script.
You can copy it from the SDCard in the same folder of this script otherwise it will be downloaded (do not
close WiiuFtpServer on the Wii-U in this case)

Given the folder containing the files you've just transferred with WiiuFtpServer : 
    - compute CRC32-C
    - search for the file in WiiuFtpServer report
    - compare CRC value with the one computed on server side

The script create a log file (checkCrc.log), a crcErrors.txt (if CRC errors found) and a missings.txt file
that list the files in the input folder not found in the report.

When crc errors and missing files are found, their paths are reported in the dedicated files but this script
will also create a folder "FILES2FIX" that contains onlys symlinks to the files in errors in a subfolder "ToTransferAgain"
and to files missing in the server's report in a subfolder "ToTransfer".

You only have to drag and drop the contain of this subfloder(s) to the target location on the server to complete/fix the faulty transfer.



USE :
----

On WINDOWS, open python with administrator rights to allow the symlinks creation.

This script can be called with the imput folder as argument. If you don't you'll be prompt to browse to it.

If WiiuFtpServer_CRC32_report.sfv is not found in the same folder, it will be downloaded.

The script returns : 
    0 : non errors all files from the input folder were checked successfully
    1 : missing files in the report, other files CRC checked sucessfully
    2 : at least one CRC error was found
    3 : CRC error(s) and missing files found