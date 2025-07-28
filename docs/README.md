# DuckDB Stata DTA Extension

A DuckDB extension for reading Stata `.dta` files directly into DuckDB tables.

## Overview

This extension allows you to read Stata data files (`.dta` format) directly into DuckDB without needing to convert them first. It supports various Stata file versions and preserves data types, variable labels, and value labels.

## Authors

- Miklós Koren
- Claude (Anthropic AI Assistant)

## Getting started

First step to getting started is to create your own repo from this template by clicking `Use this template`. Then clone your new repository using 
```sh
git clone --recurse-submodules https://github.com/<you>/<your-stata-dta-extension-repo>.git
```
Note that `--recurse-submodules` will ensure DuckDB is pulled which is required to build the extension.

## Building

### Managing dependencies
DuckDB extensions uses VCPKG for dependency management. Enabling VCPKG is very simple: follow the [installation instructions](https://vcpkg.io/en/getting-started) or just run the following:
```shell
cd <your-working-dir-not-the-plugin-repo>
git clone https://github.com/Microsoft/vcpkg.git
sh ./vcpkg/scripts/bootstrap.sh -disableMetrics
export VCPKG_TOOLCHAIN_PATH=`pwd`/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Build steps
Now to build the extension, run:
```sh
make
```
The main binaries that will be built are:
```sh
./build/release/duckdb
./build/release/test/unittest
./build/release/extension/stata_dta/stata_dta.duckdb_extension
```
- `duckdb` is the binary for the duckdb shell with the extension code automatically loaded. 
- `unittest` is the test runner of duckdb. Again, the extension is already linked into the binary.
- `stata_dta.duckdb_extension` is the loadable binary as it would be distributed.

### Tips for speedy builds
DuckDB extensions currently rely on DuckDB's build system to provide easy testing and distributing. This does however come at the downside of requiring the template to build DuckDB and its unittest binary every time you build your extension. To mitigate this, we highly recommend installing [ccache](https://ccache.dev/) and [ninja](https://ninja-build.org/). This will ensure you only need to build core DuckDB once and allows for rapid rebuilds.

To build using ninja and ccache ensure both are installed and run:

```sh
GEN=ninja make
```

## Running the extension

To run the extension code, simply start the shell with `./build/release/duckdb`. This shell will have the extension pre-loaded.  

Now we can use the features from the extension directly in DuckDB:

```sql
-- Read a Stata DTA file into a table
SELECT * FROM read_stata('path/to/file.dta');

-- Get extension information
SELECT stata_dta_info('version');
```

## Planned Functionality

The extension will support:

- Reading various Stata DTA file versions (105, 108, 111, 113, 114, 115, 117, 118, 119)
- Preserving data types (numeric, string, date/time)
- Converting Stata date/time formats to DuckDB equivalents
- Handling missing values appropriately
- Support for variable labels and value labels
- Efficient streaming for large files
- Support for compressed DTA files

## Running the tests

Different tests can be created for DuckDB extensions. The primary way of testing DuckDB extensions should be the SQL tests in `./test/sql`. These SQL tests can be run using:
```sh
make test
```

## Development Status

This extension is currently in development. The Stata DTA reading functionality is being implemented based on the pandas Stata reader code included in `stata.py`.

## Supported Stata Versions

The extension will support reading Stata files from:
- Stata 7SE (version 111)  
- Stata 8/9 (version 113)
- Stata 10/11 (version 114)
- Stata 12 (version 115)
- Stata 13 (version 117)
- Stata 14/15/16 (version 118)
- Stata 15/16 with >32,767 variables (version 119)

## License

[MIT License - see LICENSE file]

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.