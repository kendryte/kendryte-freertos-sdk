cmake_minimum_required(VERSION 3.0)

## Required Variable: (if you do not use Kendryte IDE)
# -DTOOLCHAIN=/path/to/rsicv/toolchain/bin
# -DSDK=/path/to/SDK (the folder of this example file)

## Include the SDK library
include("${SDK}/cmake/common.cmake")

## Use SDK library API to add source files
add_source_files(src/hello_world/main.c)

# other cmake instructions

## Use SDK builder to build this project
include("${SDK}/cmake/executable.cmake")

