@ECHO OFF

::
:: set_year_2013.bat --
::
:: Written by Joe Mistachkin.
:: Released to the public domain, use at your own risk!
::

SET NETCORE20ONLY=
SET NETCORE30ONLY=
SET NETFX20ONLY=
SET NETFX35ONLY=
SET NETFX40ONLY=
SET NETFX45ONLY=
SET NETFX451ONLY=1
SET NETFX452ONLY=
SET NETFX46ONLY=
SET NETFX461ONLY=
SET NETFX462ONLY=
SET NETFX47ONLY=
SET NETFX471ONLY=
SET NETFX472ONLY=

REM
REM HACK: Evidently, using MSBuild with Visual Studio 2013 requires some
REM       extra magic to make it recognize the "v120" platform toolset.
REM
SET BUILD_ARGS=/property:VisualStudioVersion=12.0

VERIFY > NUL
