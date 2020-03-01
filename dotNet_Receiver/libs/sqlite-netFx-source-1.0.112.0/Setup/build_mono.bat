@ECHO OFF

::
:: build_mono.bat --
::
:: Mono Wrapper Tool for MSBuild
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

CALL :fn_ResetErrorLevel

%__ECHO3% CALL "%TOOLS%\vsSp.bat"

IF ERRORLEVEL 1 (
  ECHO Could not detect Visual Studio.
  GOTO errors
)

CALL :fn_UnsetVariable YEARS

IF NOT DEFINED NOVS2017 (
  IF DEFINED VS2017SP (
    SET YEARS=2017
  ) ELSE (
    ECHO Could not detect Visual Studio 2017.
  )
) ELSE (
  ECHO Use of Visual Studio 2017 is disallowed.
)

IF NOT DEFINED NOVS2015 (
  IF DEFINED VS2015SP (
    SET YEARS=2015
  ) ELSE (
    ECHO Could not detect Visual Studio 2015.
  )
) ELSE (
  ECHO Use of Visual Studio 2015 is disallowed.
)

IF NOT DEFINED NOVS2013 (
  IF DEFINED VS2013SP (
    SET YEARS=2013
  ) ELSE (
    ECHO Could not detect Visual Studio 2013.
  )
) ELSE (
  ECHO Use of Visual Studio 2013 is disallowed.
)

IF NOT DEFINED YEARS (
  IF NOT DEFINED NOVS2012 (
    IF DEFINED VS2012SP (
      SET YEARS=2012
    ) ELSE (
      ECHO Could not detect Visual Studio 2012.
    )
  ) ELSE (
    ECHO Use of Visual Studio 2012 is disallowed.
  )
)

IF NOT DEFINED YEARS (
  ECHO No supported version of Visual Studio was detected and allowed.
  goto errors
)

SET PLATFORMS="Any CPU"
SET NOUSER=1

REM
REM TODO: This list of properties must be kept synchronized with the common
REM       list in the "SQLite.NET.Mono.Settings.targets" file.
REM
SET MSBUILD_ARGS=/property:ConfigurationSuffix=MonoOnPosix
SET MSBUILD_ARGS=%MSBUILD_ARGS% /property:InteropCodec=false
SET MSBUILD_ARGS=%MSBUILD_ARGS% /property:InteropLog=false

IF DEFINED MSBUILD_ARGS_MONO (
  SET MSBUILD_ARGS=%MSBUILD_ARGS% %MSBUILD_ARGS_MONO%
)

REM
REM TODO: This list of properties must be kept synchronized with the debug
REM       list in the "SQLite.NET.Mono.Settings.targets" file.
REM
SET MSBUILD_ARGS_DEBUG=/property:CheckState=true
SET MSBUILD_ARGS_DEBUG=%MSBUILD_ARGS_DEBUG% /property:CountHandle=true
SET MSBUILD_ARGS_DEBUG=%MSBUILD_ARGS_DEBUG% /property:TraceConnection=true
SET MSBUILD_ARGS_DEBUG=%MSBUILD_ARGS_DEBUG% /property:TraceDetection=true
SET MSBUILD_ARGS_DEBUG=%MSBUILD_ARGS_DEBUG% /property:TraceHandle=true
SET MSBUILD_ARGS_DEBUG=%MSBUILD_ARGS_DEBUG% /property:TraceStatement=true
SET MSBUILD_ARGS_DEBUG=%MSBUILD_ARGS_DEBUG% /property:TrackMemoryBytes=true

%__ECHO3% CALL "%TOOLS%\build_all.bat"

IF ERRORLEVEL 1 (
  ECHO Failed to build Mono binaries.
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
