#!/bin/sh

FNAME=$1
UNAME=$2

echo "user is: $UNAME"
echo "file is: $FNAME"
cat $FNAME | sed -n "
/^$UNAME:.*/ {
    s/^$UNAME:\s*\(.*\)/\1/ 
    h
    :loop
    n
    s/^#.*//
    t loop
    s/^\S.*//
    t exit
    H
    b loop 
    :exit
    x
    s/\n//g
    s/\s*,\s*/\n/g
    p
}"
