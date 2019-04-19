#!/bin/bash

# This script will update the `boost/` directory from a sibling directory called `boost_src`, which
# should contain the complete Boost distribution. This shouldn't need to be run often, and
# *definitely* is not a part of typical setup.

# Assumes:
#    - Submodules are inited
#    - `bcp` can be found within $PATH

ROOT=$(dirname $0)

pushd $ROOT > /dev/null

rm -rf boost/
mkdir boost/

SOURCES=(
    ./adobe_source_libraries/adobe/*.hpp
    ./binspector/*.hpp
    ./source/*.cpp
)

bcp --scan --boost=./boost_src ${SOURCES[@]} boost

popd > /dev/null
