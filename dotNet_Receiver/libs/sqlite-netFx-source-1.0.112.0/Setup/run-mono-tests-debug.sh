#!/bin/bash

scriptdir=`dirname "$BASH_SOURCE"`

if [[ -z "$SQLITE_NET_YEAR" ]]; then
  SQLITE_NET_YEAR=2013
fi

pushd "$scriptdir/.."
mono Externals/Eagle/bin/netFramework40/EagleShell.exe -preInitialize "set root_path {$scriptdir/..}; set test_configuration Debug; set test_year {$SQLITE_NET_YEAR}; set build_directory {bin/$SQLITE_NET_YEAR/Debug$SQLITE_NET_CONFIGURATION_SUFFIX/bin}" -file Tests/all.eagle "$@"
popd
