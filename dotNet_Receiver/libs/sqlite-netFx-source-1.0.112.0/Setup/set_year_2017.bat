@ECHO OFF

::
:: set_year_2017.bat --
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
SET NETFX451ONLY=
SET NETFX452ONLY=
SET NETFX46ONLY=
SET NETFX461ONLY=
SET NETFX462ONLY=
SET NETFX47ONLY=1
SET NETFX471ONLY=
SET NETFX472ONLY=

REM
REM HACK: Evidently, using MSBuild with Visual Studio 2017 requires some
REM       extra magic to make it recognize the "v141" platform toolset.
REM
SET BUILD_ARGS=/property:VisualStudioVersion=15.0

VERIFY > NUL
