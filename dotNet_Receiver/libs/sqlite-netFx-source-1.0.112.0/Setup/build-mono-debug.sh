#!/bin/bash

scriptdir=`dirname "$BASH_SOURCE"`

if [[ -z "$SQLITE_NET_YEAR" ]]; then
  SQLITE_NET_YEAR=2013
fi

pushd "$scriptdir/.."
xbuild SQLite.NET.$SQLITE_NET_YEAR.MSBuild.sln /property:Configuration=Debug "$@"
popd
