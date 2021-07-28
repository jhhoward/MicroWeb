@ECHO OFF

REM NOTE: If you remove this file from your boot disks, 
REM   the FreeDOS boot will automaticall prompt user for date/time
REM   That issue can be fixed by adding the "/D" flag in CONFIG.SYS
REM   to the COMMAND.COM line , but /D will also make FreeDOS skip
REM   execution of AUTOEXEC.BAT.
REM   More info is here:
REM     https://sourceforge.net/p/freedos/mailman/message/5156181/

REM Change packet driver settings as required
if %config%.==. NE2000 0x60 3 0x300
if %config%.==0. NE2000 0x60 3 0x300
if %config%.==1. NE2000XT 0x60 3 0x300
if %config%.==2. 8029PKT 0x60
if %config%.==3. 8139PKT 0x60
if %config%.==4. pcntpk INT=0x60
if %config%.==5. 3C503 0x60 3 0x300
if %config%.==6. SMC_WD 0x60 3 0x300

SET MTCPCFG=MTCP.CFG

REM Install mouse driver
ECHO.
ECHO Choose mouse type:
ECHO 0 = Autodetect
ECHO 1 = Serial
ECHO 2 = PS/2
ECHO 3 = None
SET /p mousetype=:

if "%mousetype%"=="1" goto serial
if "%mousetype%"=="2" goto ps2
if "%mousetype%"=="3" goto start

REM autodetect mouse type
CTMOUSE
goto start

:serial
CTMOUSE /S
goto start

:ps2
CTMOUSE /P
goto start

:start
REM Try to get an IP address automatically via DHCP
DDHCP.COM /M

ECHO.
ECHO Welcome to the MicroWeb FreeDOS boot disk!
ECHO To run MicroWeb: type MICROWEB
ECHO Available command line options:
ECHO  -c Run in 640x200 CGA mode
ECHO  -h Run in 720x348 Hercules mode
ECHO  -e Run in 640x350 EGA mode
ECHO  -v Run in 640x480 VGA mode
ECHO.