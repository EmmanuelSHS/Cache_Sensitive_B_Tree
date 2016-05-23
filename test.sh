#!/bin/bash

output="output.dat"

testloop() {
        echo "" >> $output
        echo "PARAMS $@" >> $output
        ./build "$@" >> $output || true
}

make Makefile all

# Too Few Keys
testloop 1 1 2 3 2

# Too Many Keys
testloop 200 1 3 2 3

# Fanout = 1
testloop 8 2 3 1 3

# Simple test output
testloop 750 100 5 9 17
testloop 83000 1000 17 17 17 17

# Heavy work load (slow output writes and large)
#testloop 360 2000000 9 5 9
#testloop 360 20000000 9 5 9
#testloop 360 200000000 9 5 9
#
#testloop 280 2000000 17 17
#testloop 280 20000000 17 17
#testloop 280 200000000 17 17
#
#testloop 2000 2000000 9 5 5 9
#testloop 2000 20000000 9 5 5 9
#testloop 2000 200000000 9 5 5 9

