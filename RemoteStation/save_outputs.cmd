@echo off
for /f "tokens=2 delims==" %%a in ('wmic OS Get localdatetime /value') do set "dt=%%a"
set "YY=%dt:~2,2%" & set "YYYY=%dt:~0,4%" & set "MM=%dt:~4,2%" & set "DD=%dt:~6,2%"
set "HH=%dt:~8,2%" & set "Min=%dt:~10,2%" & set "Sec=%dt:~12,2%"

set "fullstamp=%YYYY%-%MM%-%DD%_%HH%-%Min%-%Sec%"

FOR /F "tokens=*" %%g IN ('git log "--pretty=format:%%h" -n 1') do (SET revid=%%g)

echo F | xcopy %1\All.hex saved\All%2_%fullstamp%_%revid%.hex > nul
echo F | xcopy %1\All.S saved\All%2_%fullstamp%_%revid%.S > nul