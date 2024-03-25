# lysys

lysys is a cross-platform C API to interact with functionality of the underlying operating system not provided by the C standard library. It is not finished yet, but some features are implemented. The following platforms are supported or planned to be supported:

- Windows (high support)
- Darwin (minimal support)
- Linux (minimal support)

## Building

lysys uses CMake as the build system. Follow the following steps to build the project:

```sh
mkdir build
cd build
cmake ..
```
 
This will generate a build system of which you can use to build the library. You may select the build system of your choice with the `-G` option, run `cmake --help` to see a list of available generators.
