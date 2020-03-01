@ECHO OFF

::
:: archive.bat --
::
:: Source Archiving Tool
::
:: Written by Joe Mistachkin.
:: Released to the public domain, use at your own risk!
::

SETLOCAL

REM SET __ECHO=ECHO
REM SET __ECHO2=ECHO
IF NOT DEFINED _AECHO (SET _AECHO=REM)
IF NOT DEFINED _CECHO (SET _CECHO=REM)
IF NOT DEFINED _VECHO (SET _VECHO=REM)

%_AECHO% Running %0 %*

SET DUMMY2=%1

IF DEFINED DUMMY2 (
  GOTO usage
)

SET ROOT=%~dp0\..
SET ROOT=%ROOT:\\=\%

SET TOOLS=%~dp0
SET TOOLS=%TOOLS:~0,-1%

%_VECHO% Root = '%ROOT%'
%_VECHO% Tools = '%TOOLS%'

CALL :fn_ResetErrorLevel

%__ECHO2% PUSHD "%ROOT%"

IF ERRORLEVEL 1 (
  ECHO Could not change directory to "%ROOT%".
  GOTO errors
)

FOR /F "delims=" %%V IN ('TYPE System.Data.SQLite\AssemblyInfo.cs ^| find.exe "AssemblyVersion"') DO (
  SET VERSION=%%V
)

IF NOT DEFINED VERSION (
  SET VERSION=1.0.0.0
  GOTO skip_mungeVersion
)

REM
REM NOTE: Strip off all the extra stuff from the AssemblyVersion line we found
REM       in the AssemblyInfo.cs file that we do not need (i.e. everything
REM       except the raw version number itself).
REM
SET VERSION=%VERSION:(=%
SET VERSION=%VERSION:)=%
SET VERSION=%VERSION:[=%
SET VERSION=%VERSION:]=%
SET VERSION=%VERSION: =%
SET VERSION=%VERSION:assembly:=%
SET VERSION=%VERSION:AssemblyVersion=%
SET VERSION=%VERSION:"=%
REM "

:skip_mungeVersion

%_VECHO% Version = '%VERSION%'

CALL :fn_ResetErrorLevel

IF NOT EXIST Setup\Output (
  %__ECHO% MKDIR Setup\Output

  IF ERRORLEVEL 1 (
    ECHO Could not create directory "Setup\Output".
    GOTO errors
  )
)

IF DEFINED ARCHIVE_FULL (
  SET EXCLUDE_SRC=@data\exclude_full_src.txt
  SET PREFIX=sqlite-netFx-full-source
) ELSE (
  SET EXCLUDE_SRC=@data\exclude_src.txt
  SET PREFIX=sqlite-netFx-source
)

%_VECHO% ExcludeSrc = '%EXCLUDE_SRC%'
%_VECHO% Prefix = '%PREFIX%'

%_CECHO% zip.exe -v -r "Setup\Output\%PREFIX%-%VERSION%.zip" * -x "%EXCLUDE_SRC%"
%__ECHO% zip.exe -v -r "Setup\Output\%PREFIX%-%VERSION%.zip" * -x "%EXCLUDE_SRC%"

IF ERRORLEVEL 1 (
  ECHO Failed to archive source files.
  GOTO errors
)

%__ECHO2% POPD

IF ERRORLEVEL 1 (
  ECHO Could not restore directory.
  GOTO errors
)

GOTO no_errors

:fn_ResetErrorLevel
  VERIFY > NUL
  GOTO :EOF

:fn_SetErrorLevel
  VERIFY MAYBE 2> NUL
  GOTO :EOF

:usage
  ECHO.
  ECHO Usage: %~nx0
  ECHO.
  GOTO errors

:errors
  CALL :fn_SetErrorLevel
  ENDLOCAL
  ECHO.
  ECHO Archive failure, errors were encountered.
  GOTO end_of_file

:no_errors
  CALL :fn_ResetErrorLevel
  ENDLOCAL
  ECHO.
  ECHO Archive success, no errors were encountered.
  GOTO end_of_file

:end_of_file
%__ECHO% EXIT /B %ERRORLEVEL%
