#!/bin/bash
##%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
## WiiUFtpServer WUP package builder (java needed)
################################################################################


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
echo "done, WUP package created in $OUTPUTDIRECTORY




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