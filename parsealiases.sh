#!/bin/sh

FNAME=$1
UNAME=$2

echo "user is: $UNAME"
echo "file is: $FNAME"
cat $FNAME | grep -v '^#' | sed -n "
/^$UNAME:/ {
    s/\\\\$// ; h ; t sawslash
    :loop
    n
    s/^\S// ; t exit
    s/\\\\$// ; H ; $ b exit
    t sawslash
    b loop
    :sawslash
    n
    s/\\\\$// ; H ; $ b exit
    t sawslash
    b loop
    :exit
    x
    s/\n//g
    s/$UNAME:\s*//
    s/\s*,\s*/,/g
    s/,$//
    p
}"
