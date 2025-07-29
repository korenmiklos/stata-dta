# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a production-ready DuckDB extension for reading Stata .dta files directly into DuckDB tables. The extension provides high-performance access to Stata datasets with full SQL integration, supporting multiple file versions (105-119) and all Stata data types with proper missing value handling.

## Architecture

- **Extension Name**: `stata_dta` (defined in CMakeLists.txt and Makefile)
- **Main Implementation**: `src/stata_dta_extension.cpp` - Contains the Stata DTA reading functions and extension loading logic
- **Header**: `src/include/stata_dta_extension.hpp` - Extension class definition
- **Build System**: CMake with DuckDB's extension build system, wrapped by a Makefile
- **Dependencies**: Uses VCPKG for dependency management, currently includes OpenSSL
- **Testing**: SQL-based tests in `test/sql/` directory using DuckDB's test framework

## Common Development Commands

### Building
```sh
make                    # Build the extension (creates build/release/duckdb and extension binaries)
GEN=ninja make         # Build with ninja for faster rebuilds (requires ninja and ccache)
```

### Testing
```sh
make test              # Run all SQL tests in test/sql/
```

### Running
```sh
./build/release/duckdb # Start DuckDB shell with extension pre-loaded
```

### Project Setup (for new extensions)
```sh
python3 ./scripts/bootstrap-template.py <new_extension_name>  # Rename from template
```

## Key Files and Structure

- `Makefile` - Simple wrapper that includes DuckDB's extension build system
- `CMakeLists.txt` - CMake configuration defining extension name and sources  
- `extension_config.cmake` - DuckDB extension loading configuration
- `vcpkg.json` - Dependency management configuration
- `src/stata_dta_extension.cpp` - Main extension implementation with Stata DTA reading functions
- `test/sql/stata_dta.test` - SQL test cases for the extension
- `stata.py` - Python implementation of Stata DTA reader (reference for C++ implementation)
- `extension-ci-tools/` - Submodule with DuckDB's CI/CD tools

## Extension Development Notes

- Extensions register scalar functions using `ExtensionUtil::RegisterFunction()`
- Functions are implemented as functors that operate on `DataChunk` inputs
- The extension uses DuckDB's `UnaryExecutor::Execute` pattern for processing vectors
- Extension loading happens through the `LoadInternal()` function
- Tests use DuckDB's SQL test format with `require <extension_name>` statements

## Build Dependencies

- VCPKG (for dependency management)
- CMake 3.5+
- DuckDB submodule (included via `--recurse-submodules` during clone)
- Optional: ninja and ccache for faster builds
- Optional: CLion IDE support (requires special CMake setup)

## Extension Functions

### Table Functions
- `read_stata_dta(filename VARCHAR)` - **Main function**: Returns table data from Stata DTA file
  - Supports all Stata versions 105, 108, 111, 113-119
  - Automatic version detection and parsing
  - Memory-efficient chunked reading
  - Proper data type mapping and missing value handling

### Scalar Functions
- `stata_dta_info(version VARCHAR)` - Returns extension version and build information

## Implementation Status

### ✅ Completed Features
- **Multi-version Support**: Stata 105, 108, 111, 113, 114, 115, 117, 118, 119
- **Complete Data Types**: byte, int, long, float, double, strings (1-244 chars)
- **Missing Value Handling**: Proper NULL conversion for all data types
- **Performance Optimization**: Chunked reading, memory efficiency
- **Byte Order Support**: Both big-endian and little-endian files
- **Error Handling**: Comprehensive validation and error messages
- **Test Suite**: Unit, functional, and performance tests
- **Documentation**: Examples, usage guides, API reference

### 🔄 Future Enhancements
- Value labels (categorical mappings)
- Variable labels (descriptive names)
- Stata date/time format conversion
- Compressed DTA file support
- Extended metadata extraction

## Key Implementation Details

### Architecture
1. **StataParser**: Base class handling file I/O, byte order, type mappings
2. **StataReader**: Main reader with version-specific logic and chunked data processing
3. **DuckDB Integration**: Table function interface with proper memory management

### Data Flow
1. File opening and version detection
2. Header parsing (variables, formats, labels)
3. Data location identification
4. Chunked data reading with type conversion
5. Missing value detection and NULL mapping
6. DuckDB DataChunk population

### Performance Features
- **Memory Efficient**: Streaming reads, configurable chunk sizes
- **Type Safe**: Proper C++ type handling with bounds checking
- **Error Recovery**: Graceful handling of corrupted or unsupported files

## Testing Strategy

### Test Data
- `test/data/simple.dta` - Basic functionality (5 rows, mixed types)
- `test/data/large_dataset.dta` - Performance testing (10k rows)
- `test/data/with_missing.dta` - Missing value handling
- `test/data/version_*.dta` - Multi-version compatibility

### Test Categories
1. **Unit Tests**: Basic functionality and error conditions
2. **Functional Tests**: Real data processing and SQL integration
3. **Performance Tests**: Large dataset handling and memory usage
4. **Compatibility Tests**: Multiple Stata version support

## Development Notes

### Build Requirements
- CMake 3.5+
- C++17 compiler
- OpenSSL (linked via vcpkg)
- Optional: ninja, ccache for faster builds

### Key Files to Monitor
- Source files in `src/` - Core implementation
- Test files in `test/sql/` - Regression testing
- CMakeLists.txt - Build configuration
- Extension config - DuckDB integration settings

## Reference Implementation

The `stata.py` file contains the original pandas Stata reader implementation used as reference for the C++ implementation. The extension is now production-ready and provides comprehensive Stata DTA file reading capabilities with full DuckDB integration.