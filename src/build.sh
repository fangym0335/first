#!/bin/bash

## 1. make
make
if [ $? -eq 0 ] ; then 
	echo "[32;40;1mcompiling successful!" 
else
	echo "[32;40;1mcompiling failure!" 
	exit 1
fi

## 2. copy file to nfsroot
echo -n "copy ifsf_main to  nfsroot... "
if [ -e $NFSROOT/ifsf_main ]; then
	rm -f $NFSROOT/ifsf_main
fi

cp -f ../bin/ifsf_main  $NFSROOT 

if [ $? -eq 0 ] ; then 
	echo -e "done.[0m"
	exit 0
else
	echo -e "failed.[0m"
	exit 1
fi
