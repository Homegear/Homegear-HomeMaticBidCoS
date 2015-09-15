#!/bin/bash
SCRIPTDIR="$( cd "$(dirname $0)" && pwd )"
cd $SCRIPTDIR
CPPFLAGS=-DDEBUG CXXFLAGS="-g -O0" && make && make install
