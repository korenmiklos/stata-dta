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
  - Binary format support (versions 105-116)
  - XML format support (versions 117-119) with proper section parsing
- **Complete Data Types**: All Stata data types with version-specific mappings
  - Strings (1-244 characters)
  - Numeric types: BYTE (int8), INT (int16), LONG (int32), FLOAT, DOUBLE
  - Version 118+ type codes: 248=LONG, 249=INT, 250=BYTE
- **Missing Value Handling**: Proper NULL conversion for all data types
- **Performance Optimization**: Chunked reading, memory efficiency
- **Byte Order Support**: Both big-endian and little-endian files
- **XML Format Parser**: Complete support for mixed XML/binary structure
- **Error Handling**: Comprehensive validation and error messages
- **Test Suite**: Unit, functional, performance, and real-world compatibility tests
- **Production Ready**: Successfully handles large-scale production datasets

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
1. File opening and format detection (binary vs XML)
2. Version-specific header parsing
   - Binary format: Sequential byte reading
   - XML format: Tag-based section parsing with `FindXMLSection()`
3. Variable metadata extraction (types, names, formats)
4. Data section boundary identification
5. Chunked data reading with version-specific type conversion
6. Missing value detection and NULL mapping
7. DuckDB DataChunk population with proper type casting

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
- `test/data/v118_types_test.dta` - Version 118+ type codes testing
- `test/data/expat-local-debug.dta` - Real-world XML format validation

### Test Categories
1. **Unit Tests**: Basic functionality and error conditions
2. **Functional Tests**: Real data processing and SQL integration
3. **Performance Tests**: Large dataset handling and memory usage
4. **Compatibility Tests**: Multiple Stata version support
5. **XML Format Tests**: Version 117+ XML structure validation
6. **Real-World Tests**: Production dataset compatibility
7. **Type Code Tests**: Version-specific type mapping validation

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

## Version-Specific Implementation Details

### Type Code Mappings
The extension handles different type code schemes across Stata versions:

**Traditional Binary Format (versions 105-116):**
- 251 = BYTE (int8)
- 252 = INT (int16) 
- 253 = LONG (int32)
- 254 = FLOAT (float32)
- 255 = DOUBLE (float64)
- 1-244 = STRING (length 1-244)

**Modern XML Format (versions 118+):**
- 248 = LONG (int32) - *Key discovery: not DOUBLE as initially assumed*
- 249 = INT (int16)
- 250 = BYTE (int8)
- 254 = FLOAT (float32)
- 255 = DOUBLE (float64) 
- 1-244 = STRING (length 1-244)

### XML Format Structure
Version 117+ files use a mixed XML/binary format:
```
<stata_dta>
  <header>...</header>
  <map>...</map>
  <variable_types>BINARY_DATA</variable_types>
  <varnames>BINARY_DATA</varnames>
  <sortlist>BINARY_DATA</sortlist>
  <formats>BINARY_DATA</formats>
  <value_label_names>BINARY_DATA</value_label_names>
  <variable_labels>BINARY_DATA</variable_labels>
  <characteristics>BINARY_DATA</characteristics>
  <data>BINARY_DATA</data>
  <value_labels>BINARY_DATA</value_labels>
</stata_dta>
```

### Critical Technical Discoveries
1. **Type Code 248 Mapping**: Initially assumed to be DOUBLE, actually represents INT32 in version 118+
2. **Padding Bytes**: Version 118+ variable types include padding bytes (every other byte is meaningful)
3. **Data Boundaries**: XML format requires parsing `<data>` and `</data>` tags for proper section detection
4. **Real-World Compatibility**: Production files may contain categorical data encoded as integer codes

## Reference Implementation

The `stata.py` file contains the original pandas Stata reader implementation used as reference for the C++ implementation. The extension is now production-ready and provides comprehensive Stata DTA file reading capabilities with full DuckDB integration.