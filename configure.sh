#!/bin/bash

# Run a command, and echo before doing so. Also checks the exit
# status and quits if there was an error.
echo_run () {
    echo "EXEC : $@"
    "$@"
    r=$?
    if test $r -ne 0 ; then
        exit $r
    fi
}

git_dependency() {
    local URL=$1
    local DIR=$2
    local BRANCH=$3

    if [ -z "$BRANCH" ]; then
        BRANCH=$TRAVIS_BRANCH
    fi

    if [ ! -e "$DIR" ]; then
        echo "INFO : Cloning $DIR..."
        echo_run git clone --branch=$BRANCH $URL $DIR
    else
        echo "INFO : Pulling $DIR..."

        echo_run cd $DIR
        echo_run git branch -u origin/$BRANCH
        echo_run git checkout $BRANCH
        echo_run git pull origin $BRANCH
        echo_run cd ..
    fi

    return 0
}

echo_run cd `dirname $0`

BOOST_MAJOR=1
BOOST_MINOR=64
BOOST_DOT=0
BOOST_DT_VER=${BOOST_MAJOR}.${BOOST_MINOR}.${BOOST_DOT}
BOOST_US_VER=${BOOST_MAJOR}_${BOOST_MINOR}_${BOOST_DOT}
BOOST_DIR=boost_${BOOST_US_VER}
BOOST_TAR=$BOOST_DIR.tar.gz

# Make sure the dependencies we grab are in the same branch we are.
if [ -z "$TRAVIS_BRANCH" ]; then
    export TRAVIS_BRANCH=`git rev-parse --abbrev-ref HEAD`
fi

# Make sure we're at the top-level directory when we set up all our siblings.
cd ..

echo_run git_dependency https://github.com/stlab/libraries.git 'libraries'
echo_run git_dependency https://github.com/stlab/adobe_source_libraries 'adobe_source_libraries'
echo_run git_dependency https://github.com/stlab/adobe_platform_libraries 'adobe_platform_libraries' master
echo_run git_dependency https://github.com/stlab/double-conversion.git 'double-conversion' master

exit 0;

# If need be, download Boost and unzip it, moving it to the appropriate location.
if [ ! -e 'boost_libraries' ]; then
    echo "INFO : boost_libraries not found: setting up."

    if [ ! -e $BOOST_TAR ]; then
        echo "INFO : $BOOST_TAR not found: downloading."

        echo_run curl -L "http://sourceforge.net/projects/boost/files/boost/$BOOST_DT_VER/$BOOST_TAR/download" -o $BOOST_TAR;
    else
        echo "INFO : $BOOST_TAR found: skipping download."
    fi

    echo_run rm -rf $BOOST_DIR

    echo_run tar -xf $BOOST_TAR

    echo_run rm -rf boost_libraries

    echo_run mv $BOOST_DIR boost_libraries
else
    echo "INFO : boost_libraries found: skipping setup."
fi

# Create b2 (bjam) via boostrapping, again if need be.
if [ ! -e './boost_libraries/b2' ]; then
    echo "INFO : b2 not found: boostrapping."

    echo_run cd boost_libraries;

    echo_run ./bootstrap.sh --with-toolset=${TOOLSET:-clang}

    echo_run cd ..
else
    echo "INFO : b2 found: skipping boostrap."
fi

# A blurb of diagnostics to make sure everything is set up properly.
ls -alF

echo "INFO : You are ready to go!"
