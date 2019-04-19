#!/bin/bash
set -x

if [ "$TRAVIS_OS_NAME" = "linux" ]; then
  sudo update-alternatives \
    --install /usr/bin/gcc gcc /usr/bin/gcc-5 90 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-5 \
    --slave /usr/bin/gcov gcov /usr/bin/gcov-5
  sudo update-alternatives \
    --install /usr/bin/clang clang /usr/bin/clang-3.8 90 \
    --slave /usr/bin/clang++ clang++ /usr/bin/clang++-3.8
  sudo update-alternatives --config gcc
  sudo update-alternatives --config clang
  if [ "$CXX" = "clang++" ]; then
      export PATH=/usr/bin:$PATH
  fi
fi

cd `dirname $0`

echo "INFO : Make sure you run the configure script before this one to ensure dependencies are in place."

if [ "$BUILDTOOL" == "" ] ; then
    echo "INFO : BUILDTOOL unknown. Defaulting to bjam."

    BUILDTOOL="bjam"
fi

if [ "$BUILDTOOL" == "xcode" ] ; then

    if [ "$BUILDMODE" == "debug" ] ; then
        CURMODE="Debug"
    elif [ "$BUILDMODE" == "release" ] ; then
        CURMODE="Release"
    else
        echo "INFO : xcode mode unknown. Defaulting to debug."

        CURMODE="Debug"
    fi

    cd xcode

    xcodebuild -configuration $CURMODE

elif [ "$BUILDTOOL" == "bjam" ] ; then

    if [ "$BUILDMODE" == "debug" ] ; then
        CURMODE="debug"
    elif [ "$BUILDMODE" == "release" ] ; then
        CURMODE="release"
    else
        echo "INFO : bjam mode unknown. Defaulting to debug."

        CURMODE="debug"
    fi

    PROCESSOR_COUNT=`sysctl -n hw.ncpu`

    if [ "$PROCESSOR_COUNT" == "" ] ; then
        PROCESSOR_COUNT=`nproc`
    fi

    if [ "$PROCESSOR_COUNT" == "" ] ; then
        PROCESSOR_COUNT=1
    fi

    echo "INFO : Found $PROCESSOR_COUNT processors."

    ../boost_libraries/b2 --toolset=${TOOLSET:-clang} --without-python --hash -j$PROCESSOR_COUNT $CURMODE

else

    echo "ERRR : BUILDTOOL unknown. Goodbye."

    exit 1;

fi
