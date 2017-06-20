#!/bin/bash

find . -name '*.[c|h]pp' | xargs clang-format -style=file -i
