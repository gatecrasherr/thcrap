#!/bin/bash

if ! command -v wine &> /dev/null
then
	echo "FATAL ERROR: wine is not installed, aborting"
	exit 255
fi

if ! command -v winetricks &> /dev/null
then
	echo "Error: winetricks is not installed, aborting"
	exit 1
fi

if command -v wget &> /dev/null
then
	DOWNLOAD_="wget "
elif command -v curl &> /dev/null
then
	DOWNLOAD_="curl -O "
else
	echo "Error: neiter curl nor wget are installed, aborting"
	exit 2
fi

if [ -z "${WINEPREFIX}" ]
then
	read -r -t 5 -p "Warning: environment variable WINEPREFIX is not set. This means that the default wineprefix will be used
When installing .NET, it is recommended to do it in its own wineprefix.
Press any key or wait 5 seconds to continue
"
else
	[ "${WINEPREFIX: -1}" = "/" ] && export WINEPREFIX=${WINEPREFIX::-1}
	[ "${WINEPREFIX}" = "$HOME/.wine" ] && read -r -t 5 -p "Warning: environment variable WINEPREFIX set to default wineprefix location.
When installing .NET, it is recommended to do it in its own wineprefix.
Press any key or wait 5 seconds to continue
"
fi

if [ -z "${WINEARCH}" ]
then
	read -r -t 5 -p "Warning: environment variable WINEARCH is not set.
This makes wine default to running in 64-bit mode, which may cause issues.
Press any key or wait 5 seconds to continue.
"
else
	[ "${WINEARCH}" != "win32" ] && read -r -t 5 -p "Warning: environment variable WINEARCH not set to win32.
This makes wine run in 64-bit mode, which may cause issues.
Press any key or wait 5 seconds to continue.
"
fi

winetricks dotnet40
wineserver -w
wine winecfg -v win7
$DOWNLOAD_ 'https://download.visualstudio.microsoft.com/download/pr/7afca223-55d2-470a-8edc-6a1739ae3252/abd170b4b0ec15ad0222a809b761a036/ndp48-x86-x64-allos-enu.exe'
wine ndp48-x86-x64-allos-enu.exe /passive
wineserver -w
rm ndp48-x86-x64-allos-enu.exe
echo "Successfully installed .NET Framework 4.8.0"
