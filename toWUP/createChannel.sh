#!/bin/bash
##%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
## WiiUFtpServer WUP package builder (java needed)
################################################################################

#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
# LOCAL FUNCTIONS
#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function CheckNusP {
    check=$(more $scriptFolderPath/Key.txt | grep "ENCRYPTING_KEY_32HEXACHARS")
    if [ "$check" != "" ]; then
    
        echo " "
        echo "No key found in Key.txt"
        echo " "
        echo "Replace the last line in Key.txt" 
        echo "with the key you want to echo use for encrypting the data."
        echo " "
        echo "(use the Wii U common key for custom app)"
        exit 101
    fi
    
    KEY=$(more $scriptFolderPath/Key.txt | grep -E "^[A-Fa-F0-9]{32}$")
    
    if [ "$KEY" == "" ]; then
        echo "ERROR : WiiU common key not found in Key.txt !"    
        exit 50
    fi

}

#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
# MAIN PARAMETERS
#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
titleId=0005000010050421

################################################################################
## MAIN PROGRAM
################################################################################

# return code
returnCode=0

# full path to the parent directory this script
scriptFolderPath="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd $scriptFolderPath

echo "========================================================="
echo "Convert Loadiine to WUT package [$titleId]"
echo "========================================================="
echo "(java need to be in the path)"
echo ""
echo "Hit any key to launch the conversion"
echo ""
read pause

wiiUFtpServerRoot=$(dirname $scriptFolderPath)

KEY=""
CheckNusP

INPUTDIRECTORY=$wiiUFtpServerRoot/_loadiine/$titleId
OUTPUTDIRECTORY=$wiiUFtpServerRoot/_sdCard/install/WiiUFtpServer
if [ ! -d $OUTPUTDIRECTORY ]; then
    rm -rf $OUTPUTDIRECTORY > /dev/null 2>&1
fi
mkdir -p $OUTPUTDIRECTORY > /dev/null 2>&1

rm -f "NUSP.log" > /dev/null 2>&1
echo "java -jar NUSPacker.jar -in $INPUTDIRECTORY -out $OUTPUTDIRECTORY -tID $titleId -encryptKeyWith $KEY"
echo "---------------------------------------------------------"
java -jar NUSPacker.jar -in $INPUTDIRECTORY -out $OUTPUTDIRECTORY -tID $titleId -encryptKeyWith $KEY
echo ""
echo "---------------------------------------------------------"
echo "NusPacker returned = $?"

tmpFolder=$scriptFolderPath/tmp
outputFolder=$scriptFolderPath/output
rm -rf $tmpFolder > /dev/null 2>&1
rm -rf $outputFolder > /dev/null 2>&1

echo "========================================================="
echo "done, WUP package created in $OUTPUTDIRECTORY"

echo "#######################################################################" 

if [ $returnCode -lt 50 ]; then
    if [ $returnCode -eq 0 ]; then
        echo "Done successfully"
    else
        echo "Done with warnings"
    fi
else
    echo "Done with errors"
fi
exit $returnCode