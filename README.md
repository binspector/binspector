[![Build status](https://travis-ci.org/binspector/binspector.png?branch=master)](https://travis-ci.org/binspector/binspector)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/13168/badge.svg)](https://scan.coverity.com/projects/binspector-binspector)

# What is Binspector?

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

These instructions assume a command line interface for most of the operations. If you are using a GUI (such as GitHub's client) your mileage may vary.

Building is done with `CMake`. The typical workflow is to start at the top of the repository, and run:

- `mkdir build`
- `cd build`
- `cmake .. -GXcode` (or whatever generator flavor you prefer)
- `xcodebuild ` (or `make`, or...)

The binary will then be found in the `build` folder (or in a subfolder, depending on the generator).
