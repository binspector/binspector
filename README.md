[![Build status](https://travis-ci.org/binspector/binspector.png?branch=master)](https://travis-ci.org/binspector/binspector)

# What is Binspector?

[![Join the chat at https://gitter.im/binspector/binspector](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/binspector/binspector?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

Binspector and its related language are built around the idea of exposing the guts of binary formats and files so plain old humans can grok them better.

Specifically there are two pieces that accomplish this:

 - A language used to construct binary format descriptions
 - The Binspector tool

The language allows individuals to create formalized descriptions of binary formats. These descriptions are known as BFFTs (<strong>b</strong>inary <strong>f</strong>ile <strong>f</strong>ormat <strong>t</strong>emplate). From there any given binary file can be tested against a BFFT in order to:

- Verify the binary file meets the requirements of the binary format
  - e.g., "Is my_photo.jpg a well-formed jpeg?"
- Analyze the contents of the binary file and interpret the raw data within
  - e.g., "What Exif metadata is contained within my_photo.jpg, and where?"
- Inspect particular binary values in a file with proper context and interpretation
  - e.g., "What is the value of Exif tag 37377 in my_photo.jpg?"
- Intelligently fuzz the binary at potential "weak points" that may exist in file import code, and auto-generate files containing these attack vectors.
  - e.g., "I need corrupt sample files to harden my Exif import code."

Binspector is a command-line tool with various output methods (including its own pseudo command-line interface) intended to facilitate the above processes.

## Building From Source

### Overview

The sources for binspector have been moved to GitHub. These instructions assume a command line interface for most of the operations. If you are using a GUI (such as GitHub's client) your mileage may vary.

### Pre-Development Setup

To set up building binspector, first run `configure.sh` at the top of the repository. This will download and install the necessary dependencies such as Boost and the Adobe Source Libraries. You should need to do this step infrequently.

### Option 1: Build with bjam

(`bjam` has been renamed `b2` in the most recent versions of Boost. Throughout the documentation where `bjam` is referenced, `b2` is implied. My muscle memory is still making the transition.)

To build binspector run the `build.sh` script at the top of the repository, or just run `b2`. In the latter case, to make the build go faster pass `-jN` to bjam, where `N` is the number of concurrent threads you want compiling the project. (`build.sh` should handle this for you.)

After building, the binspector binary will be inside the `../bin` directory.

### Option 2: Build with Xcode

Binspector now has an Xcode project contained within the `xcode` directory of the repository. Building from within Xcode should be straightforward. All intermediate files and binaries produced will go into the same `./built_artifacts/` directory (though in a different location).
