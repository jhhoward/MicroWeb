# MicroWeb
![Screenshot](screenshot.png)

MicroWeb is a web browser for DOS! It is a 16-bit real mode application, designed to run on minimal hardware.

## Minimum requirements
To run you will need:
* Intel 8088 or compatible CPU
* CGA compatible graphics card (EGA and VGA are backwards compatible)
* A network interface (it is possible to use your machine's serial port with the EtherSLIP driver)
* A mouse is desirable but not 100% required

## Limitations
* Text only (this may change in a later release)
* HTTP only (no HTTPS support)
* No CSS or Javascript
* Very long pages may be truncated if there is not enough RAM available

## Keyboard shortcuts
Escape : Exit
F2 : Invert screen (useful for LCD displays)
F6 / Ctrl+L : Select address bar
Tab / Shift+Tab : Cycle through selectable page elements
Backspace : Back in history

The page position can be scrolled with cursor keys, Page up, Page down, Home and End

## Network setup
MicroWeb uses Michael Brutman's [mTCP networking library](http://www.brutman.com/mTCP/) for the network stack. You will need a DOS packet driver relevant to your network interface. You can read more about configuring DOS networking [here](http://www.brutman.com/Dos_Networking/dos_networking.html)

## Build instructions
To build you will need the [OpenWatcom 1.9 C++ compiler](https://sourceforge.net/projects/openwatcom/files/open-watcom-1.9/). 
Use OpenWatcom's wmake to build the makefile in the project/DOS folder

