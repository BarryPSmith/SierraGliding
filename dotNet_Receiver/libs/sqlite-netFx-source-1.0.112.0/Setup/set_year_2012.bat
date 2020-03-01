@ECHO OFF

::
:: set_year_2012.bat --
::
:: Written by Joe Mistachkin.
:: Released to the public domain, use at your own risk!
::

SET NETCORE20ONLY=
SET NETCORE30ONLY=
SET NETFX20ONLY=
SET NETFX35ONLY=
SET NETFX40ONLY=
SET NETFX45ONLY=1
SET NETFX451ONLY=
SET NETFX452ONLY=
SET NETFX46ONLY=
SET NETFX461ONLY=
SET NETFX462ONLY=
SET NETFX47ONLY=
SET NETFX471ONLY=
SET NETFX472ONLY=

REM
REM HACK: Evidently, installing Visual Studio 2013 breaks using MSBuild to
REM       build native projects that specify a platform toolset of "v110".
REM
SET BUILD_ARGS=/property:VisualStudioVersion=11.0

VERIFY > NUL
