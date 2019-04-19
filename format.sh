#!/bin/bash

SOURCES=(
    ./binspector/*.hpp
    ./source/*.cpp
)

find ${SOURCES[@]} -name '*.[c|h]pp' | xargs clang-format -style=file -i
