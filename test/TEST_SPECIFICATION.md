# Stata DTA Extension - Test Specification

This document provides a comprehensive test specification for the Stata DTA extension, derived from the pandas Stata reader test suite and adapted for DuckDB SQL testing.

## Overview

The test suite consists of 6 main test files covering different aspects of the extension:

1. **`stata_dta_unit_tests.test`** - Main functionality and integration tests (61 tests)
2. **`stata_dta_missing_values.test`** - Missing value handling (30 tests)  
3. **`stata_dta_version_compatibility.test`** - Stata version support (30 tests)
4. **`stata_dta_error_handling.test`** - Error conditions and recovery (45 tests)
5. **`stata_dta_performance_benchmarks.test`** - Performance and scalability (40 tests)
6. **`stata_dta_comprehensive.test`** - Existing comprehensive tests
7. **`stata_dta_functional.test`** - Existing functional tests
8. **`stata_dta_performance.test`** - Existing performance tests

## Test Data Requirements

The test suite expects the following test data files in `test/data/`:

### Core Test Files
- `simple.dta` - Basic dataset (5 rows, 3 columns: id, name, age)
- `empty.dta` - Empty dataset (0 rows)
- `mixed_types.dta` - All Stata data types (byte, int, long, float, double, string)
- `with_missing.dta` - Dataset with NULL values in various columns
- `large_dataset.dta` - Performance testing (10,000+ rows)
- `string_lengths.dta` - Variable length strings (1-244 characters)
- `special_chars.dta` - International characters and special symbols

### Version-Specific Files
- `version_114.dta` - Stata 10/11 format
- `version_117.dta` - Stata 13 format (Unicode support)
- `version_118.dta` - Stata 14/15/16 format

### Error Testing Files (Optional)
- `corrupted_header.dta` - File with invalid header
- `truncated.dta` - Incomplete file

## Test Categories

### 1. Basic Functionality Tests (61 tests)

**Purpose**: Verify core extension functionality works correctly.

**Key Areas**:
- Extension loading and info function
- File reading and basic queries
- Data type mapping (byte → TINYINT, int → SMALLINT, etc.)
- SQL integration (WHERE, JOIN, GROUP BY, ORDER BY)
- Schema inspection and metadata access

**Critical Tests**:
- Test 4: Simple dataset reading with expected results
- Test 14-16: Value range validation for Stata numeric types
- Test 37-42: SQL integration (JOINs, aggregations, etc.)
- Test 61: Complex query combining all features

### 2. Missing Value Handling Tests (30 tests)

**Purpose**: Ensure Stata's complex missing value system is properly converted to SQL NULLs.

**Key Areas**:
- Basic missing value detection (IS NULL, IS NOT NULL)
- Extended missing values (.a, .b, .c, ..., .z → NULL)
- Empty strings → NULL
- Aggregation functions ignoring NULLs
- NULL behavior in JOINs, ORDER BY, GROUP BY
- Window functions with NULLs

**Critical Tests**:
- Test 1-3: Basic NULL detection and counting
- Test 7: Aggregation functions ignoring NULLs correctly
- Test 14-15: ORDER BY with NULL handling
- Test 21-24: Stata-specific missing value boundaries

### 3. Version Compatibility Tests (30 tests)

**Purpose**: Verify support for multiple Stata file format versions.

**Key Areas**:
- Version 114 (Stata 10/11) support
- Version 117 (Stata 13) Unicode support  
- Version 118 (Stata 14/15/16) large dataset support
- Cross-version data consistency
- Schema consistency across versions
- Performance parity across versions

**Critical Tests**:
- Test 1-3: Basic reading of each version
- Test 4: Cross-version row count consistency
- Test 5-8: Schema consistency verification
- Test 18: Cross-version JOINs work correctly

### 4. Error Handling Tests (45 tests)

**Purpose**: Verify robust error handling and meaningful error messages.

**Key Areas**:
- File access errors (non-existent, permissions, directories)
- Invalid file formats and corrupted files
- Parameter validation (wrong types, NULL, multiple args)
- SQL context errors (invalid usage patterns)
- Resource cleanup after errors
- Error message quality and consistency

**Critical Tests**:
- Test 1-8: File access error scenarios
- Test 15-19: Parameter validation
- Test 38-44: Error recovery and resource cleanup
- Test 41-43: Error message quality verification

### 5. Performance Benchmark Tests (40 tests)

**Purpose**: Verify acceptable performance and scalability characteristics.

**Key Areas**:
- Small, medium, and large file performance
- Memory usage efficiency (chunked reading)
- Concurrent access handling
- Complex query performance (JOINs, aggregations)
- Scalability under load
- Resource cleanup verification

**Critical Tests**:
- Test 3: Large file handling without memory issues
- Test 5: Constant memory usage verification
- Test 6-7: Concurrent access patterns
- Test 30-32: Real-world analytical query patterns
- Test 36-40: Performance consistency

## Implementation Requirements

### Data Type Mapping
```
Stata Type    → DuckDB Type  | Value Range
byte (int8)   → TINYINT      | -127 to 100
int (int16)   → SMALLINT     | -32,767 to 32,740  
long (int32)  → INTEGER      | -2,147,483,647 to 2,147,483,620
float         → FLOAT        | IEEE 754 single precision
double        → DOUBLE       | IEEE 754 double precision
str1-str244   → VARCHAR      | Variable length strings
```

### Missing Value Handling
- All Stata missing values (., .a, .b, ..., .z) → NULL
- Empty strings → NULL
- Proper NULL semantics in all SQL operations
- Aggregation functions ignore NULLs

### Error Messages
- Descriptive error messages including file paths
- Consistent error types (IO Error, Conversion Error, etc.)
- Graceful handling of edge cases

### Performance Requirements
- Constant memory usage for large files (chunked reading)
- Support for files > 1GB without memory exhaustion
- Reasonable performance for concurrent access
- Resource cleanup after errors

## Test-Driven Development Workflow

1. **Run existing tests** to establish baseline
2. **Implement failing tests** one category at a time:
   - Start with basic functionality
   - Add missing value handling
   - Implement version compatibility
   - Add error handling
   - Optimize for performance
3. **Iterate** on implementation until all tests pass
4. **Add regression tests** for any bugs discovered

## Running the Tests

```bash
# Run all tests
make test

# Run specific test file
./build/release/test/unittest --test-dir test/sql stata_dta_unit_tests*

# Run with verbose output
./build/release/test/unittest --test-dir test/sql stata_dta* --verbose
```

## Test Data Generation

Use the provided Python script to generate test data:

```bash
python3 test/create_test_data.py
```

This creates all required `.dta` files in `test/data/` with the expected schemas and data patterns.

## Success Criteria

- All 206 tests pass consistently
- Memory usage remains constant for large files
- Error messages are helpful and actionable
- Performance meets or exceeds pandas Stata reader
- Full SQL integration with DuckDB features
- Support for Stata versions 105, 108, 111, 113-119

## Notes for Implementation

1. **Chunked Reading**: Implement streaming/chunked reading to handle large files
2. **Thread Safety**: Ensure stateless design for concurrent access  
3. **Error Recovery**: Clean up resources properly after errors
4. **Version Detection**: Automatic detection of Stata file version
5. **Byte Order**: Handle both big-endian and little-endian files
6. **String Encoding**: Proper UTF-8 handling for international characters

This test specification provides a comprehensive framework for test-driven development of the Stata DTA extension, ensuring robust, performant, and feature-complete implementation.