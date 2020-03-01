@ECHO OFF

::
:: build_ce_200x.bat --
::
:: WinCE Wrapper Tool for MSBuild
::
:: Written by Joe Mistachkin.
:: Released to the public domain, use at your own risk!
::

SETLOCAL

REM SET __ECHO=ECHO
REM SET __ECHO3=ECHO
IF NOT DEFINED _AECHO (SET _AECHO=REM)
IF NOT DEFINED _CECHO (SET _CECHO=REM)
IF NOT DEFINED _VECHO (SET _VECHO=REM)

%_AECHO% Running %0 %*

SET DUMMY2=%1

IF DEFINED DUMMY2 (
  GOTO usage
)

SET TOOLS=%~dp0
SET TOOLS=%TOOLS:~0,-1%

%_VECHO% Tools = '%TOOLS%'

CALL :fn_ResetErrorLevel

%__ECHO3% CALL "%TOOLS%\vsSp.bat"

IF ERRORLEVEL 1 (
  ECHO Could not detect Visual Studio.
  GOTO errors
)

IF DEFINED BUILD_DEBUG (
  SET BUILD_CONFIGURATIONS=Debug Release
) ELSE (
  SET BUILD_CONFIGURATIONS=Release
)

SET BASE_CONFIGURATIONSUFFIX=Compact
SET PLATFORMS="Pocket PC 2003 (ARMV4)"

REM
REM NOTE: The .NET Compact Framework is only supported by Visual Studio 2005
REM       and 2008, regardless of which versions of Visual Studio are installed
REM       on this machine; therefore, override the YEARS variable limiting it
REM       to 2005 and 2008 only.
REM
CALL :fn_UnsetVariable YEARS

IF NOT DEFINED NOVS2005 (
  IF DEFINED VS2005SP (
    CALL :fn_AppendVariable YEARS " 2005"
  )
)

IF NOT DEFINED NOVS2008 (
  IF DEFINED VS2008SP (
    CALL :fn_AppendVariable YEARS " 2008"
  )
)

%_VECHO% BuildConfigurations = '%BUILD_CONFIGURATIONS%'
%_VECHO% BaseConfigurationSuffix = '%BASE_CONFIGURATIONSUFFIX%'
%_VECHO% Platforms = '%PLATFORMS%'
%_VECHO% Years = '%YEARS%'

CALL :fn_ResetErrorLevel

%__ECHO3% CALL "%TOOLS%\build_all.bat"

IF ERRORLEVEL 1 (
  ECHO Failed to build WinCE binaries.
  GOTO errors
)

GOTO no_errors

:fn_AppendVariable
  SET __ECHO_CMD=ECHO %%%1%%
  IF DEFINED %1 (
    FOR /F "delims=" %%V IN ('%__ECHO_CMD%') DO (
      SET %1=%%V%~2
    )
  ) ELSE (
    SET %1=%~2
  )
  SET __ECHO_CMD=
  CALL :fn_ResetErrorLevel
  GOTO :EOF

:fn_UnsetVariable
  SETLOCAL
  SET VALUE=%1
  IF DEFINED VALUE (
    SET VALUE=
    ENDLOCAL
    SET %VALUE%=
  ) ELSE (
    ENDLOCAL
  )
  CALL :fn_ResetErrorLevel
  GOTO :EOF

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
  ECHO Build failure, errors were encountered.
  GOTO end_of_file

:no_errors
  CALL :fn_ResetErrorLevel
  ENDLOCAL
  ECHO.
  ECHO Build success, no errors were encountered.
  GOTO end_of_file

:end_of_file
%__ECHO% EXIT /B %ERRORLEVEL%
