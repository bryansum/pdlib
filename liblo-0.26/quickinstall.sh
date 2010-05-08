#!/bin/sh

# Copyright 2010 Bryan Summersett

CUR_DIR=`pwd`
DEVICE_DIR=$CUR_DIR/iphoneos3.1.2
SIM_DIR=$CUR_DIR/iphonesimulator3.1.2

rm -fr $DEVICE_DIR $SIM_DIR

$CUR_DIR/build_for_iphone -p "$DEVICE_DIR" device
$CUR_DIR/build_for_iphone -p "$SIM_DIR" simulator

lipo -output /usr/local/lib/liblo-iphone.a -create $DEVICE_DIR/lib/liblo.a $SIM_DIR/lib/liblo.a

cp -r $DEVICE_DIR/include/ /usr/local/include
