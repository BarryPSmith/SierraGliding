ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION
SET count=1

FOR /F "usebackq" %%F IN (`avrdude -p m328p -P %1 -b %2 -c %3 -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h -q -q`) DO (
    SET var!count!=%%F
    SET /a count=!count!+1
)
IF %var1%==0xe2 IF %var2%==0xdc IF %var3%==0x6 (
    ECHO Fuses already set properly
    exit /b
)
ECHO Current fuses: %var1% %var2% %var3%
ECHO Setting fuses
avrdude -p m328p -P %1 -b %2 -c %3 -U lfuse:w:0xe2:m -U hfuse:w:0xdc:m -U efuse:w:0x06:m -u -q
ENDLOCAL