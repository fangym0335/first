#!/bin/sh
newPATH=`echo $PATH|sed s#/usr/local/arm/3.4.1/bin:##`
PATH=/usr/local/arm/arm-linux-gcc-4.8.2/bin:$newPATH
export LD_LIBRARY_PATH=/usr/local/arm/arm-linux-gcc-4.8.2/lib:$LD_LIBRARY_PATH
echo $LD_LIBRARY_PATH
make tools
