#!/usr/bin/env bash

make clean
make

rm run/baron30.bin
./fetchresource.sh