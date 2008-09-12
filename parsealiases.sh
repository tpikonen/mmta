#!/bin/sh

FNAME=$1
UNAME=$2

echo "user is: $UNAME"
echo "file is: $FNAME"
# 1st sed: grab the correct lines (FIXME: continuing lines starting from the 
# first column are included, this should not be)
# 2nd sed: split tokens to lines
# 3rd sed: trim leading whitespace
cat $FNAME | sed -n "1h;1!H;\${;g;s/.*$UNAME:\s*\([^:]*\)\n\S.*/\1/p;}" \
| sed 's/\(\S\)\s\s*\(\S*\)/\1\n\2/' | sed 's/^\s*//'
