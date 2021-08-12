# MicroWeb DOS web browser
![Screenshot](screenshot.png)

MicroWeb is a web browser for DOS! It is a 16-bit real mode application, designed to run on minimal hardware.

## Minimum requirements
To run you will need:
* Intel 8088 or compatible CPU
* CGA, EGA, VGA or Hercules compatible graphics card
* A network interface (it is possible to use your machine's serial port with the EtherSLIP driver)
* A mouse is desirable but not 100% required
* 640k RAM is desirable. EMS/XMS are not required

## Limitations
* Text only (this may change in a later release)
* HTTP only (no HTTPS support)
* No CSS or Javascript
* Very long pages may be truncated if there is not enough RAM available
* Mouse cursor is currently not visible in Hercules mode

## Keyboard shortcuts
* Escape : Exit
* F2 : Invert screen (useful for LCD displays)
* F6 / Ctrl+L : Select address bar
* Tab / Shift+Tab : Cycle through selectable page elements
* Enter : Follow link / press button
* Backspace : Back in history

The page position can be scrolled with cursor keys, Page up, Page down, Home and End

## Command line options
You can use a URL as an argument to load a specific page on startup. This can also be a path to a local html file. 

MicroWeb will try to automatically choose the most appropriate display mode on startup, but it is possible to manually select a video mode by using a command line switch

Option | Effect
-------|-------
 -c    | Force to run in 640x200 CGA mode
 -h    | Force to run in 720x348 Hercules mode
 -e    | Force to run in 640x350 EGA mode
 -v    | Force to run in 640x480 VGA mode
 -i    | Start with inverted screen colours (useful for some LCD monitors)
 
For example `MICROWEB -c http://68k.news` will start in CGA 640x200 mode and load the 68k.news website

## HTTPS limitations
Unfortunately older machines just don't have the processing power to handle HTTPS but there are a few options available:
* Browse sites that still allow HTTP
* Use a proxy server such as [retro-proxy](https://github.com/DrKylstein/retro-proxy) which converts HTTPS to HTTP. You can configure a proxy server by setting the HTTP_PROXY environment variable before running MicroWeb. e.g. `SET HTTP_PROXY=192.168.0.50:8000`
* Use the [FrogFind!](http://www.frogfind.com) web service to view a stripped down version of a site. If MicroWeb is redirected to an HTTPS site then it will generate a FrogFind link for your convenience.

## Getting started
Check out the [releases page](https://github.com/jhhoward/MicroWeb/releases) which will include a pre-built binary. Also available are FreeDOS boot disk images for 360K and 720K floppies, which are configured to work with a NE2000 network adapter. These boot images can be used in an emulator such as [PCem](https://pcem-emulator.co.uk/).

## Network setup
MicroWeb uses Michael Brutman's [mTCP networking library](http://www.brutman.com/mTCP/) for the network stack. You will need a DOS packet driver relevant to your network interface. You can read more about configuring DOS networking [here](http://www.brutman.com/Dos_Networking/dos_networking.html)

## Build instructions
To build you will need the [OpenWatcom 1.9 C++ compiler](https://sourceforge.net/projects/openwatcom/files/open-watcom-1.9/). 
Use OpenWatcom's wmake to build the makefile in the project/DOS folder

