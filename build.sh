#!/bin/bash

if ! test -d build; then
	echo "making a new build folder"
	mkdir build
fi

cd build
cmake ..
make -j4
