#!/bin/bash
set -e
rm -rf output
rm -rf .build
mkdir -p .build
cd .build
cmake ../
make -j32
make install
cd ..
rm -rf .build
