# Contributing to Stata DTA Extension

Thank you for your interest in contributing to the Stata DTA Extension for DuckDB! This guide will help you get started with development, testing, and submitting contributions.

## Development Setup

### Prerequisites
- C++17 compatible compiler (GCC 8+, Clang 8+, MSVC 2019+)
- CMake 3.5 or later
- Git with submodule support
- Python 3.7+ (for test data generation)
- Optional: ninja, ccache for faster builds

### Getting Started

1. **Fork and clone the repository:**
```bash
git clone --recurse-submodules https://github.com/your-username/stata-dta.git
cd stata-dta
```

2. **Set up development environment:**
```bash
# Install optional build accelerators
brew install ninja ccache  # macOS
# or
sudo apt-get install ninja-build ccache  # Ubuntu

# Generate test data
python3 test/create_test_data.py
```

3. **Build and test:**
```bash
# Build extension
make debug

# Run tests
make test

# Run specific test
./build/debug/test/unittest --test-dir test/sql stata_dta
```

## Project Structure

```
stata-dta/
├── src/
│   ├── include/
│   │   ├── stata_parser.hpp      # Core parser interface
│   │   └── stata_dta_extension.hpp  # Extension interface
│   ├── stata_parser.cpp          # File I/O and type handling
│   ├── stata_reader.cpp          # Version-specific parsing
│   └── stata_dta_extension.cpp   # DuckDB integration
├── test/
│   ├── sql/                      # SQL test files
│   ├── data/                     # Test DTA files
│   └── create_test_data.py       # Test data generator
├── examples/                     # Usage examples
├── docs/                         # Documentation
├── CMakeLists.txt               # Build configuration
└── Makefile                     # Build wrapper
```

## Development Guidelines

### Code Style
- **C++ Standard**: C++17
- **Naming**: snake_case for variables/functions, PascalCase for classes
- **Headers**: Include guards with `#pragma once`
- **Memory**: Prefer smart pointers and RAII patterns
- **Error Handling**: Use DuckDB exception types

### Example Code Style
```cpp
class StataReader : public StataParser {
public:
    explicit StataReader(const std::string& filename);
    
    bool Open();
    std::unique_ptr<DataChunk> ReadChunk(idx_t chunk_size = STANDARD_VECTOR_SIZE);
    
private:
    std::string filename_;
    std::vector<StataVariable> variables_;
    uint64_t rows_read_;
    
    void ReadHeader();
    void ConvertStataValue(const StataVariable& var, const uint8_t* src_data, 
                          Vector& dest_vector, idx_t row_idx);
};
```

### Testing
- **Test Coverage**: All new functionality must include tests
- **Test Types**: Unit tests, functional tests, performance tests
- **Test Files**: SQL-based tests in `test/sql/`
- **Test Data**: Use provided test DTA files or create new ones

### Documentation
- **API Changes**: Update `docs/API.md`
- **Examples**: Add usage examples for new features
- **Comments**: Document complex algorithms and file format details

## Contributing Process

### 1. Issue Creation
Before starting work, create an issue to discuss:
- Bug reports with reproduction steps
- Feature requests with use cases
- Performance improvements with benchmarks

### 2. Development Workflow
```bash
# Create feature branch
git checkout -b feature/your-feature-name

# Make changes with tests
# ... develop and test ...

# Run full test suite
make test

# Commit with descriptive messages
git commit -m "Add support for value labels

- Implement value label parsing for Stata formats 117+
- Add tests for categorical variable handling
- Update API documentation with label usage examples"
```

### 3. Pull Request Submission
- **Title**: Clear, descriptive title
- **Description**: 
  - What changes were made and why
  - How to test the changes
  - Any breaking changes or migration notes
- **Tests**: All tests must pass
- **Documentation**: Update relevant documentation

### 4. Review Process
- Code review by maintainers
- Automated testing on multiple platforms
- Performance regression testing
- Documentation review

## Areas for Contribution

### Priority Areas
1. **Value Labels Implementation**
   - Parse value label mappings from DTA files
   - Expose labels through DuckDB metadata
   - Add SQL functions for label access

2. **Variable Labels Support**
   - Extract variable descriptions
   - Integrate with DuckDB column metadata
   - Add schema introspection features

3. **Date/Time Handling**
   - Convert Stata date formats to DuckDB temporal types
   - Handle various Stata date/time formats
   - Add timezone support where applicable

4. **Performance Optimization**
   - SIMD optimizations for data conversion
   - Parallel processing for large files
   - Memory mapping for improved I/O

5. **Format Extensions**
   - Compressed DTA file support
   - Stata 20+ compatibility
   - Enhanced metadata extraction

### Good First Issues
- Add support for older Stata versions (104, 105)
- Improve error messages with specific file locations
- Add more comprehensive test cases
- Performance benchmarking utilities
- Documentation improvements

## Testing Guidelines

### Test Types

#### 1. Unit Tests (`test/sql/stata_dta.test`)
```sql
# Test basic functionality
query I
SELECT COUNT(*) FROM read_stata_dta('test/data/simple.dta');
----
5
```

#### 2. Functional Tests (`test/sql/stata_dta_functional.test`)
```sql
# Test data type handling
statement ok
CREATE TEMP TABLE test_data AS SELECT * FROM read_stata_dta('test/data/mixed_types.dta');

query I
SELECT typeof(age) FROM test_data LIMIT 1;
----
SMALLINT
```

#### 3. Performance Tests (`test/sql/stata_dta_performance.test`)
```sql
# Test large dataset handling
query I
SELECT COUNT(*) FROM read_stata_dta('test/data/large_dataset.dta');
----
10000
```

### Creating Test Data
```python
# Add to test/create_test_data.py
def create_new_test_case():
    df = pd.DataFrame({
        'test_column': test_data,
        # ... more columns
    })
    df.to_stata('test/data/new_test.dta', version=117)
```

### Benchmarking
```bash
# Build release version for performance testing
make release

# Run performance tests
./build/release/test/unittest --test-dir test/sql stata_dta_performance

# Custom benchmarking
time ./build/release/duckdb -c "SELECT COUNT(*) FROM read_stata_dta('large_file.dta');"
```

## Debugging

### Debug Builds
```bash
# Build with debug symbols
make debug

# Run with debugger
gdb ./build/debug/duckdb
```

### Logging
```cpp
// Add debug output (remove before committing)
#ifdef DEBUG
std::cout << "Reading header, version: " << static_cast<int>(header_.format_version) << std::endl;
#endif
```

### Test Debugging
```bash
# Run single test with verbose output
./build/debug/test/unittest --test-dir test/sql stata_dta.test -v

# Run DuckDB with extension loaded
./build/debug/duckdb -unsigned
```

## Performance Considerations

### Memory Management
- Use RAII for resource management
- Prefer stack allocation for small objects
- Use smart pointers for dynamic allocation
- Monitor memory usage with large files

### I/O Optimization
- Minimize file seeks
- Use buffered reading for sequential access
- Consider memory mapping for read-only access
- Profile I/O patterns with real-world files

### Benchmarking Guidelines
```cpp
// Example performance measurement
auto start = std::chrono::high_resolution_clock::now();
auto chunk = reader.ReadChunk();
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
```

## Documentation Updates

### When to Update Documentation
- New functions or parameters
- Changed behavior or breaking changes
- Performance improvements
- New supported file formats
- Bug fixes that affect usage

### Documentation Types
- **API Reference** (`docs/API.md`): Function signatures and usage
- **User Guide** (`docs/README.md`): Installation and basic usage
- **Examples** (`examples/`): Real-world usage patterns
- **Developer Guide**: This file (architecture, contributing)

## Release Process

### Version Numbering
- Major: Breaking changes or significant new features
- Minor: New features, backward compatible
- Patch: Bug fixes, performance improvements

### Release Checklist
- [ ] All tests pass on supported platforms
- [ ] Documentation updated
- [ ] Performance benchmarks run
- [ ] CHANGELOG.md updated
- [ ] Version numbers incremented
- [ ] Release notes prepared

## Getting Help

### Communication Channels
- **GitHub Issues**: Bug reports and feature requests
- **GitHub Discussions**: General questions and ideas
- **Pull Request Comments**: Code-specific discussions

### Development Questions
- Architecture decisions: Open GitHub discussion
- Implementation details: Comment on relevant issue
- Testing approach: Ask in pull request

### Reporting Bugs
Include in bug reports:
1. DuckDB and extension versions
2. Operating system and compiler
3. Minimal reproduction case
4. Expected vs actual behavior
5. Sample DTA file (if possible)

Thank you for contributing to making Stata data more accessible in the DuckDB ecosystem!