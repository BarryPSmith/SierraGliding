#!/bin/bash

scriptdir=`dirname "$BASH_SOURCE"`

if [[ "$OSTYPE" == "darwin"* ]]; then
  libname=libSQLite.Interop.dylib
else
  libname=libSQLite.Interop.so
fi

if [[ -z "$SQLITE_NET_YEAR" ]]; then
  SQLITE_NET_YEAR=2013
fi

pushd "$scriptdir/.."

SQLITE_INTEROP_DIR=bin/$SQLITE_NET_YEAR/Release$SQLITE_NET_CONFIGURATION_SUFFIX/bin
SQLITE_INTEROP_FILE=$SQLITE_INTEROP_DIR/$libname

if [[ -f "${SQLITE_INTEROP_FILE}" ]]; then
  cp "$SQLITE_INTEROP_FILE" "$SQLITE_INTEROP_DIR/SQLite.Interop.dll"
  libname=SQLite.Interop.dll
fi

dotnet exec Externals/Eagle/bin/netStandard20/EagleShell.dll -preInitialize "set test_configuration Release; set test_configuration_suffix {$SQLITE_NET_CONFIGURATION_SUFFIX}; set test_native_configuration_suffix {$SQLITE_NET_CONFIGURATION_SUFFIX}; set test_year NetStandard20; set test_native_year {$SQLITE_NET_YEAR}" -file Tests/all.eagle "$@"
popd
