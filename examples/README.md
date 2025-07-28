# Stata DTA Extension Examples

This directory contains examples demonstrating how to use the Stata DTA extension for DuckDB.

## Prerequisites

1. Build the extension:
   ```bash
   make
   ```

2. Create test data:
   ```bash
   python3 test/create_test_data.py
   ```

## Running Examples

### Basic Usage

The `basic_usage.sql` file demonstrates core functionality:

```bash
./build/release/duckdb < examples/basic_usage.sql
```

### Example Use Cases

1. **Data Import**: Read Stata files directly into DuckDB without conversion
2. **Data Analysis**: Perform SQL operations on Stata data 
3. **Data Integration**: Join Stata data with other data sources
4. **Data Export**: Convert Stata data to other formats

## Sample Queries

### Read a Simple Dataset
```sql
SELECT * FROM read_stata_dta('test/data/simple.dta');
```

### Handle Missing Values
```sql  
SELECT 
    id,
    COALESCE(score, 0) as score_filled
FROM read_stata_dta('test/data/with_missing.dta');
```

### Performance with Large Files
```sql
SELECT COUNT(*) FROM read_stata_dta('test/data/large_dataset.dta');
```

### Data Type Conversion
```sql
SELECT 
    name,
    CAST(age AS BIGINT) as age_int,
    ROUND(salary, 2) as salary_rounded
FROM read_stata_dta('test/data/mixed_types.dta');
```

## Supported Features

- ✅ Multiple Stata versions (114, 117, 118)
- ✅ All numeric data types (byte, int, long, float, double)
- ✅ String data with variable lengths
- ✅ Missing value handling
- ✅ Large file support with chunked reading
- ✅ Integration with DuckDB's SQL engine
- 🔲 Value labels (planned)
- 🔲 Variable labels (planned)
- 🔲 Date/time formats (planned)

## Performance Notes

The extension is optimized for:
- **Memory efficiency**: Streaming reads for large files
- **Type safety**: Proper handling of Stata data types
- **Integration**: Seamless use with DuckDB's query engine

For large files (>1GB), the extension automatically uses chunked reading to manage memory usage.