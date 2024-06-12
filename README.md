# lysys

[![build](https://github.com/evrhel/lysys/actions/workflows/build.yaml/badge.svg)](https://github.com/evrhel/lysys/actions/workflows/build.yaml)

lysys is a cross-platform C API to interact with functionality of the underlying operating system not provided by the C standard library. It is not finished yet, but some features are implemented. The following platforms are supported or planned to be supported:

- Windows (high support)
- Darwin (okay support)
- Linux (okay support)

Note most functionality is largely untested.

## Building

lysys uses CMake as the build system. Follow the following steps to build the project:

```sh
mkdir build
cd build
cmake ..
```
 
This will generate a build system of which you can use to build the library. You may select the build system of your choice with the `-G` option, run `cmake --help` to see a list of available generators.

## Usage

To access the API, link against the built library, add `include` to your include path, and include `lysys/lysys.h` somewhere in your project.

**Note**: If you are using C++, you must instead include `lysys/lysys.hpp` to avoid name mangling.

Before making any calls to the API must be initialized with a call to `ls_init`. There must be a single call corresponding call to `ls_shutdown` or `ls_exit`. The latter will terminate the program.

### Example Program

This program reads from the file `file.txt` and prints its contents to the terminal. It does not use proper error checking; you should not do this.

```c
#include <lysys/lysys.h>

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	char buf[512];
	size_t bytes_read;
	ls_handle file;

	// Open file `file.txt` for reading
	file = ls_open("file.txt", LS_FILE_READ, LS_SHARE_READ, LS_OPEN_EXISTING);

	// Read in file contents
	bytes_read = ls_read(file, buf, sizeof(buf) - 1);
	buf[bytes_read] = 0;

	// Close the file
	ls_close(file);

	// Write to terminal
	puts(buf);	

	return 0;
}
```

