#!/bin/bash

# Run a command, and echo before doing so. Also checks the exit
# status and quits if there was an error.
echo_run ()
{
    echo "EXEC : $@"
    "$@"
    r=$?
    if test $r -ne 0 ; then
        exit $r
    fi
}

echo_run cd `dirname $0`

if [ "$BUILDMODE" == "debug" ] ; then
    CURMODE="debug"
elif [ "$BUILDMODE" == "release" ] ; then
    CURMODE="release"
else
    echo "INFO : BUILDMODE unspecfied. Defaulting to debug."

    CURMODE="debug"
fi

BINPATH="../bin/$CURMODE/binspector"
BFFTPATH="./bfft"
SAMPLEPATH="../samples"

./configure.sh
./build.sh

# We have a repository for sample files; grab that in anticipation for the
# validation pass below.
if [ ! -e $SAMPLEPATH ]; then
    echo_run git clone --depth=1 https://github.com/binspector/samples.git $SAMPLEPATH
fi

echo_run $BINPATH -t $BFFTPATH/jpg.bfft -i $SAMPLEPATH/sample.jpg -m validate
echo_run $BINPATH -t $BFFTPATH/png.bfft -i $SAMPLEPATH/sample.png -m validate
echo_run $BINPATH -t $BFFTPATH/bmp.bfft -i $SAMPLEPATH/sample.bmp -m validate
