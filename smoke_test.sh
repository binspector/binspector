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

cd ..

if [ "$BUILDMODE" == "debug" ] ; then
    CURMODE="debug"
elif [ "$BUILDMODE" == "release" ] ; then
    CURMODE="release"
else
    echo "INFO : BUILDMODE unspecfied. Defaulting to debug."

    CURMODE="debug"
fi

BINPATH="./bin/$CURMODE/binspector"

if [ ! -e $BINPATH ]; then
    echo "INFO : $BINPATH not found: setting up."

    ./binspector/configure.sh
else
    echo "INFO : $BINPATH found: skipping setup."
fi

# Always run build in case the sources have been touched.
./binspector/build.sh

if [ "$BUILDTOOL" == "xcode" ]; then
    echo "INFO : xcode build; skipping execution phase"
    exit 0;
fi

if [ ! -e 'samples' ]; then
    mkdir 'samples'
fi

JPEGPATH='samples/sample.jpg'

if [ ! -e $JPEGPATH ]; then
    echo "INFO : $JPEGPATH not found; downloading."

    # Image use license: http://creativecommons.org/licenses/by-sa/3.0/
    SAMPLE_JPEG='http://upload.wikimedia.org/wikipedia/commons/b/b4/JPEG_example_JPG_RIP_100.jpg'

    echo_run curl -L "$SAMPLE_JPEG" -o $JPEGPATH
else
    echo "INFO : $JPEGPATH found: skipping download."
fi

PNGPATH='samples/sample.png'

if [ ! -e $PNGPATH ]; then
    echo "INFO : $PNGPATH not found; downloading."

    # Image use license: http://creativecommons.org/licenses/by-sa/3.0/
    SAMPLE_PNG='http://upload.wikimedia.org/wikipedia/commons/4/47/PNG_transparency_demonstration_1.png'

    echo_run curl -L "$SAMPLE_PNG" -o $PNGPATH
else
    echo "INFO : $PNGPATH found: skipping download."
fi

echo_run $BINPATH -t ./binspector/test/issue1.bfft -i ./binspector/test/empty.bin -m validate
echo_run $BINPATH -t ./binspector/test/issue11.bfft -i $JPEGPATH -m validate
echo_run $BINPATH -t ./binspector/test/issue19.bfft -i ./binspector/test/empty.bin -m validate
echo_run $BINPATH -t ./binspector/bfft/png.bfft -i $PNGPATH -m validate
echo_run $BINPATH -t ./binspector/bfft/jpg.bfft -i $JPEGPATH -m validate
echo_run $BINPATH -t ./binspector/bfft/png.bfft -i $PNGPATH -m fuzz --fuzz-recurse
echo_run $BINPATH -t ./binspector/bfft/png.bfft -i $PNGPATH -m fuzz
echo_run $BINPATH -t ./binspector/bfft/jpg.bfft -i $JPEGPATH -m fuzz --fuzz-recurse
echo_run $BINPATH -t ./binspector/bfft/jpg.bfft -i $JPEGPATH -m fuzz
