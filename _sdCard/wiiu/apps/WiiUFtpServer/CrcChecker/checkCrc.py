#!/usr/bin/env python3

try:
#----------------------------------------------------------------------------------------------------------
# IMPORTS
#----------------------------------------------------------------------------------------------------------

    import argparse
    import sys
    import os
    from os import listdir
    from os.path import isfile, join
    import time
    import pathlib
    import shutil
    import re

    from tkinter import filedialog
    from tkinter import *


    import concurrent.futures
    from concurrent.futures import ThreadPoolExecutor 

    import ftplib

    import numpy as np
    
    import zlib

    import threading
     
except Exception as e :
    print("Python import failed : %s" %e)
    raise Exception("ERROR")

#----------------------------------------------------------------------------------------------------------
# METHODS
#----------------------------------------------------------------------------------------------------------

def computeCrc32(filePath):
    prev = 0
    global mutex
    mutex.acquire()
    for eachLine in open(filePath,"rb"):
        prev = zlib.crc32(eachLine, prev)
    mutex.release()   
    return "%X"%(prev & 0xFFFFFFFF)
    
    
def browseToDir():    

    root = Tk()
    root.withdraw()
    return filedialog.askdirectory()

        
def downloadCrc32Report():    
    global crcReport
    global isWindows
    
    wiiuIp = "127.0.0.1"
    pattern = re.compile("^[0-9]{3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$")
    while True:
        wiiuIp = input("Please enter your Wii-U IP address : ")
        if pattern.match(wiiuIp):
            if isWindows:
                response = os.system("ping " + wiiuIp + "> NULL 2>&1")
            else:
                response = os.system("ping " + wiiuIp + "> /dev/null 2>&1")
                
            #and then check the response...
            if response == 0:
                break
            else:
                print(wiiuIp+" does not respond???")

        else:
            print ("Please enter a valid IP !")
            continue
  
    ftp = ftplib.FTP(wiiuIp)
    ftp.login("USER", "PASSWD")
    ftp.retrbinary("RETR /sd/wiiu/apps/WiiUFtpServer/CrcChecker/WiiUFtpServer_crc32_report.sfv", open(crcReport, 'wb').write)
    ftp.quit()                    

def checkFile(f, relativePath, root):
    global mutex
    global crcData
    global files2fixFolder
    global nbe
    global nbm
    global log
    global errors
    global missings
    
    mutex.acquire()
    print("Checking "+str(relativePath)+"...")
    mutex.release()   
    
    crc32 = computeCrc32(os.path.join(root, f))
    
    # search in report the relative Path
    matchingPathIndexes = np.flatnonzero(np.core.defchararray.find(str(crcData[:,1]),relativePath)!=-1)
    #            print("DEBUG matchingPathIndexes = "+str(matchingPathIndexes))
    if len(matchingPathIndexes) > 0:
        for indp in matchingPathIndexes:
            
            line = crcData[indp]
    #        print("DEBUG line["+str(indp)+"] = "+str(line))
            
            splitList = line[1].split('/')
            fileNameOnServer = splitList[len(splitList)-1]
    #                    print("DEBUG fileNameOnServer = "+str(fileNameOnServer)) 
    #                    print("DEBUG f = "+str(f)) 
            
            if fileNameOnServer == f:
            
                matchingCrcIndexes = np.flatnonzero(np.core.defchararray.find(str(line),crc32)!=-1)
    #                    print("DEBUG matchingCrcIndexes = "+str(matchingCrcIndexes))
                if len(matchingCrcIndexes) > 0:
                    
                    if line[0] == "<":
                        msg = "CRC_OK : "+relativePath+" uploaded sucessfully\n"
                    else:
                        msg = "CRC_OK : "+relativePath+" downloaded sucessfully\n"
                    mutex.acquire()
                    log.write(msg) 
                    mutex.release()   
                        
                else:
                    # error, print the CRC32 value computed VS CRC2 server value
                    crc32Server = line[2].replace("\n","")
                    if line[0] == "<":
                        msg = "CRC_ERROR : failed to upload "+relativePath+", CRC_server ="+str(crc32Server)+" and CRC = "+str(crc32)+"\n"
                    else:
                        msg = "CRC_ERROR : failed to download "+relativePath+", CRC_server ="+str(crc32Server)+" and CRC = "+str(crc32)+"\n"
                    mutex.acquire()
                    nbe += 1;            
                    print(msg)                    
                    errors.write(msg)
                    mutex.release()   
                    
                    folder = files2fixFolder+os.path.sep+"ToTransferAgain"+os.path.sep+(relativePath.replace(f,"")).replace("/",os.path.sep)
                    if not os.path.exists(folder): 
                        os.makedirs(folder)
                    
                    dst = os.path.join(folder,f) 
                    if not os.path.exists(dst): 
                        src = os.path.join(root,f)
                        os.symlink(src, dst)
                    
                
    else:
        # add it to ignored files
        msg = "CRC_INFO : "+relativePath+" not found in WiiUFtpServer_crc32_report.sfv\n"
        mutex.acquire()
        nbm += 1;
        print(msg)
        missings.write(msg)
        mutex.release()   
                            
        folder = files2fixFolder+os.path.sep+"ToTransfer"+os.path.sep+(relativePath.replace(f,"")).replace("/",os.path.sep)
        
        if not os.path.exists(folder): 
            os.makedirs(folder)
            
        dst = os.path.join(folder,f) 
        if not os.path.exists(dst):                 
            src = os.path.join(root,f)
            os.symlink(src, dst)    


                
#----------------------------------------------------------------------------------------------------------
# MAIN PROGRAM
#----------------------------------------------------------------------------------------------------------

   
if __name__ == '__main__':

    try:
        sys.getwindowsversion()
    except AttributeError:
        isWindows = False
    else:
        isWindows = True
    if isWindows:
        import win32api,win32process,win32con
        pid = win32api.GetCurrentProcessId()
        handle = win32api.OpenProcess(win32con.PROCESS_ALL_ACCESS, True, pid)
        win32process.SetPriorityClass(handle, win32process.HIGH_PRIORITY_CLASS)
    else:
        os.nice(-10)

    homeFolder = os.path.dirname(os.path.abspath(__file__))

    parser = argparse.ArgumentParser()
    parser.add_argument("-folderToCheck", "--Folder_To_Check", help = "Folder containing the files transfrerred to be checked", type = str, required = False)

    # read input parameters
    args = parser.parse_args()

    crcReport = homeFolder+os.path.sep+"WiiUFtpServer_crc32_report.sfv"

    print(" -================-")
    print("| CrcChecker V2-1  |")
    print(" -================-")
    print("")
    if os.path.exists(crcReport):
        with open(crcReport, "r") as f:
            rows = f.readlines()
            print("A report was found close to this script")
            print(" ")
            print("Use "+str(rows[1].replace("; ", ""))+" (y/n)?")
            pattern = re.compile("^[ynYN]{1}$")
            while True:
                answer = input("Please enter your Wii-U IP address : ")
                if pattern.match(answser):
                    if answer == "y" or answer == "Y":
                        break
                    else:
                        while not os.path.exists(crcReport):
                            downloadCrc32Report()
                            if not os.path.exists(crcReport):
                                print("Sorry, something went wrong retrying...")
                            else:
                                break
                        break
                else:
                    print ("Please answer with y or n !")
                    continue
    else:                
        while not os.path.exists(crcReport):
            downloadCrc32Report()
            if not os.path.exists(crcReport):
                print("Sorry, something went wrong retrying...")

    logFile = homeFolder+os.path.sep+"checkCrc.log"
    errorsFile = homeFolder+os.path.sep+"crcErrors.txt"
    missingsFile = homeFolder+os.path.sep+"missings.txt"

    if os.path.exists(logFile):
        os.remove(logFile)
    if os.path.exists(errorsFile):
        os.remove(errorsFile)
    if os.path.exists(missingsFile):
        os.remove(missingsFile)
    
    with open(crcReport, "r") as f:
        rows = f.readlines()

    print("Using "+str(rows[1].replace("; ", "")))

    crcDataList = []
    
    rowsList = sorted(rows[5:])
    for r in rowsList:
        crcDataList.append(r.split("'"))

    crcData = np.array(crcDataList, dtype=object)
    
    #print("DEBUG crcData = "+str(crcData))

    folderPath = args.Folder_To_Check
    if not folderPath:
        print("Please browse to the folder to be checked...")  
        folderPath = os.getcwd()
        # open a browse folder dialog
        folderPicked = browseToDir()       
        
        if not folderPicked:   
            print("No folder selected, exit with 0")
            exit(0)
        else:
            folderPath = folderPicked
        
    folderPath = str(pathlib.Path(folderPath).resolve())        

    # check if a FILES2FIX folder exist
    files2fixFolder = os.path.join(homeFolder,"FILES2FIX")
    files2fixBackup = os.path.join(homeFolder,"FILES2FIX_backup")
    if not os.path.exists(files2fixFolder): 
        os.makedirs(files2fixFolder)
    else:
        if os.path.exists(files2fixBackup):
            shutil.rmtree(files2fixBackup)
        os.rename(files2fixFolder, files2fixBackup)
            
    log = open(logFile, "w")            
    errors = open(errorsFile, "w")            
    missings = open(missingsFile, "w")

    # define a mutex for working on output files from threads
    mutex = threading.Lock()    
    
    # Pool of os.cpu_count() threads :  
    nbThreads = os.cpu_count()

    executor = ThreadPoolExecutor(nbThreads)
     
    nbf = 0 
    nbe = 0 
    nbm = 0 
    start_time = time.time()

    with concurrent.futures.ThreadPoolExecutor() as executor:
        futures = []

        for root, subdirectories, files in os.walk(folderPath):
            for f in files:
            
                nbf += 1;
                endPath = root.replace(folderPath+os.path.sep,'')
                
                # relative path
                relativePath = endPath.replace(os.path.sep, "/")+"/"+f

                # for debugging purpose : checkFile(f, relativePath, root)
                futures.append(executor.submit(checkFile, f, relativePath, root))

        for future in concurrent.futures.as_completed(futures):
            try:
                msg = future.result()
            except OSError as err:
                print("OS error: {0}".format(err))
            except ValueError:
                print("Could not convert data to an integer.")
            except BaseException as err:
                print("Exception raised: ".format(err))
                raise

                    
    log.close()
    errors.close()
    missings.close()

    returnCode = 0
    timeStr = "{:.2f}".format(time.time() - start_time)
    print("=====================================================================")
    print(str(nbf)+" files treated in "+timeStr+" seconds")
    print("---------------------------------------------------------------------")
    if nbm > 0:
        print(str(nbm)+" files not found in server report, see missings.txt");
        returnCode = 1
    if nbe > 0:
        print(str(nbe)+" CRC errors detected, see crcErrors.txt");
        returnCode = returnCode + 2
    print("=====================================================================")
    
    exit(returnCode)
