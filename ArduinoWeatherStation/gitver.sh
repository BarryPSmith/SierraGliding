#!/bin/sh
git log --pretty=format:'#define REV_ID "%h"' -n 1 > "$1revid.h"
retVal=$?
if [ $retVal -ne 0 ]
then
  echo "#define REV_ID \"ERR\"" > "$1revid.h"
  echo -n "Error performing git log: "
  echo $retVal
fi