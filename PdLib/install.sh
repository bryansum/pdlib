#!/bin/bash

# Copyright 2010 Bryan Summersett

if [[ "$1" == "" ]] || [[ ! -d $1 ]]; then
echo "Usage: $0 [prefix-dir]";
exit 1;
fi

xcodebuild -sdk iphoneos3.1.2 -target pd "ARCHS=armv6 armv7" "VALID_ARCHS=armv6 armv7" build
xcodebuild -sdk iphonesimulator3.1.2 -target pd "ARCHS=i386 x86_64" "VALID_ARCHS=i386 x86_64" build

echo "Installing to $1" 

lipo -output build/libpd.a -create build/Release-iphoneos/libpd.a build/Release-iphonesimulator/libpd.a

cp build/libpd.a $1/lib/
cp -r build/Release-iphoneos/usr/local/ $1


