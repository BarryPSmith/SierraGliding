@ECHO OFF

::
:: build_net_standard_20.bat --
::
:: .NET Standard 2.0 Wrapper Tool for MSBuild
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

SET DUMMY2=%2

IF DEFINED DUMMY2 (
  GOTO usage
)

SET TOOLS=%~dp0
SET TOOLS=%TOOLS:~0,-1%

%_VECHO% Tools = '%TOOLS%'

SET BUILD_CONFIGURATIONS=%1

IF DEFINED BUILD_CONFIGURATIONS (
  CALL :fn_UnquoteVariable BUILD_CONFIGURATIONS
) ELSE (
  %_AECHO% No build configurations specified, using default...
  IF DEFINED BUILD_DEBUG (
    SET BUILD_CONFIGURATIONS=DebugManagedOnly ReleaseManagedOnly
  ) ELSE (
    SET BUILD_CONFIGURATIONS=ReleaseManagedOnly
  )
)

%_VECHO% BuildConfigurations = '%BUILD_CONFIGURATIONS%'

SET YEARS=NetStandard20
SET PLATFORMS="Any CPU"
SET NOUSER=1

REM
REM TODO: This list of properties must be kept synchronized with the common
REM       list in the "SQLite.NET.NetStandard20.Settings.targets" file.
REM
SET MSBUILD_ARGS=/property:ConfigurationSuffix=NetStandard20
SET MSBUILD_ARGS=%MSBUILD_ARGS% /property:InteropCodec=false
SET MSBUILD_ARGS=%MSBUILD_ARGS% /property:InteropLog=false

IF DEFINED MSBUILD_ARGS_NET_STANDARD_20 (
  SET MSBUILD_ARGS=%MSBUILD_ARGS% %MSBUILD_ARGS_NET_STANDARD_20%
)

REM
REM TODO: This list of properties must be kept synchronized with the debug
REM       list in the "SQLite.NET.NetStandard20.Settings.targets" file.
REM
SET MSBUILD_ARGS_DEBUG=/property:CheckState=true
SET MSBUILD_ARGS_DEBUG=%MSBUILD_ARGS_DEBUG% /property:CountHandle=true
SET MSBUILD_ARGS_DEBUG=%MSBUILD_ARGS_DEBUG% /property:TraceConnection=true
SET MSBUILD_ARGS_DEBUG=%MSBUILD_ARGS_DEBUG% /property:TraceDetection=true
SET MSBUILD_ARGS_DEBUG=%MSBUILD_ARGS_DEBUG% /property:TraceHandle=true
SET MSBUILD_ARGS_DEBUG=%MSBUILD_ARGS_DEBUG% /property:TraceStatement=true
SET MSBUILD_ARGS_DEBUG=%MSBUILD_ARGS_DEBUG% /property:TrackMemoryBytes=true

REM
REM NOTE: For use of the .NET Core SDK build system.
REM
SET NETCORE20ONLY=1

REM
REM NOTE: Prevent output files from being wrongly deleted.
REM
SET TARGET=Build

%__ECHO3% CALL "%TOOLS%\build_all.bat"

IF ERRORLEVEL 1 (
  ECHO Failed to build .NET Standard 2.0 binaries.
  GOTO errors
)

GOTO no_errors

:fn_UnquoteVariable
  IF NOT DEFINED %1 GOTO :EOF
  SETLOCAL
  SET __ECHO_CMD=ECHO %%%1%%
  FOR /F "delims=" %%V IN ('%__ECHO_CMD%') DO (
    SET VALUE=%%V
  )
  SET VALUE=%VALUE:"=%
  REM "
  ENDLOCAL && SET %1=%VALUE%
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
  ECHO Usage: %~nx0 [configurations]
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
