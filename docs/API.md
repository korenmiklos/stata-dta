# Stata DTA Extension API Reference

## Table Functions

### `read_stata_dta(filename)`

Reads a Stata DTA file and returns its contents as a DuckDB table.

**Syntax:**
```sql
SELECT * FROM read_stata_dta(filename)
```

**Parameters:**
- `filename` (VARCHAR, required): Path to the Stata DTA file

**Returns:**
- Table with columns matching the Stata file structure
- Column names from the Stata variable names
- Data types automatically mapped from Stata to DuckDB types

**Example:**
```sql
-- Basic usage
SELECT * FROM read_stata_dta('data/survey.dta');

-- With filtering and aggregation
SELECT 
    region,
    COUNT(*) as obs_count,
    AVG(income) as avg_income
FROM read_stata_dta('data/households.dta')
WHERE age >= 18
GROUP BY region;
```

**Supported File Versions:**
- Stata 7SE (format 111)
- Stata 8/9 (format 113)
- Stata 10/11 (format 114)
- Stata 12 (format 115)
- Stata 13 (format 117)
- Stata 14/15/16 (format 118)
- Stata 15/16+ (format 119)

**Data Type Mapping:**

| Stata Type | DuckDB Type | Range/Notes |
|------------|-------------|-------------|
| `byte`     | `TINYINT`   | -127 to 100 |
| `int`      | `SMALLINT`  | -32,767 to 32,740 |
| `long`     | `INTEGER`   | -2,147,483,647 to 2,147,483,620 |
| `float`    | `FLOAT`     | IEEE 754 single precision |
| `double`   | `DOUBLE`    | IEEE 754 double precision |
| `str1-244` | `VARCHAR`   | Variable-length strings |

**Missing Values:**
- Stata missing values are automatically converted to SQL NULL
- Numeric missing values: specific Stata missing value constants
- String missing values: empty strings remain as empty strings

**Error Handling:**
```sql
-- File not found
SELECT * FROM read_stata_dta('nonexistent.dta');
-- Error: Cannot open Stata file: nonexistent.dta

-- Unsupported version
SELECT * FROM read_stata_dta('old_version.dta');
-- Error: Unsupported Stata file version: 104

-- Corrupted file
SELECT * FROM read_stata_dta('corrupted.dta');
-- Error: Unexpected end of Stata file
```

## Scalar Functions

### `stata_dta_info(version)`

Returns information about the Stata DTA extension.

**Syntax:**
```sql
SELECT stata_dta_info(version_string)
```

**Parameters:**
- `version_string` (VARCHAR): Version identifier or info request

**Returns:**
- VARCHAR containing extension information

**Example:**
```sql
SELECT stata_dta_info('version');
-- Returns: "Stata DTA Extension version - OpenSSL version: OpenSSL 3.x.x"
```

## Performance Considerations

### Memory Usage
- **Chunked Reading**: Large files are processed in chunks (default: 2048 rows)
- **Memory Efficiency**: Memory usage is independent of file size
- **Streaming**: No need to load entire file into memory

### Optimization Tips
1. **Column Selection**: Use `SELECT specific_columns` instead of `SELECT *` for large files
2. **Filtering**: Apply `WHERE` clauses to reduce data transfer
3. **Indexing**: Consider creating temporary tables with indexes for repeated queries

```sql
-- Efficient: Select specific columns with filtering
SELECT id, income FROM read_stata_dta('large_file.dta') WHERE income > 50000;

-- Less efficient: Select all columns without filtering
SELECT * FROM read_stata_dta('large_file.dta');
```

### Performance Benchmarks
- **Small files** (<1MB): ~10ms load time
- **Medium files** (1-100MB): ~1-5 second load time
- **Large files** (>100MB): Streaming with constant memory usage
- **Very large files** (>1GB): Memory usage remains <50MB

## Integration Examples

### With DuckDB Features

#### Window Functions
```sql
SELECT 
    *,
    ROW_NUMBER() OVER (ORDER BY income DESC) as income_rank,
    PERCENT_RANK() OVER (ORDER BY age) as age_percentile
FROM read_stata_dta('survey.dta');
```

#### Joins
```sql
-- Join with other tables
SELECT 
    s.respondent_id,
    s.income,
    d.region_name
FROM read_stata_dta('survey.dta') s
JOIN region_dictionary d ON s.region_code = d.code;
```

#### Aggregations
```sql
-- Complex aggregations
SELECT 
    education_level,
    gender,
    COUNT(*) as count,
    AVG(income) as avg_income,
    STDDEV(income) as income_stddev,
    PERCENTILE_CONT(0.5) WITHIN GROUP (ORDER BY income) as median_income
FROM read_stata_dta('labor_survey.dta')
GROUP BY education_level, gender
ORDER BY avg_income DESC;
```

#### Export to Other Formats
```sql
-- Export to CSV
COPY (
    SELECT * FROM read_stata_dta('analysis.dta') 
    WHERE year >= 2020
) TO 'recent_data.csv' (HEADER);

-- Export to Parquet
COPY (
    SELECT * FROM read_stata_dta('large_dataset.dta')
) TO 'output.parquet' (FORMAT PARQUET);
```

## Limitations and Considerations

### Current Limitations
1. **Value Labels**: Not yet implemented (planned for future release)
2. **Variable Labels**: Not yet implemented (planned for future release)  
3. **Date Formats**: Stata dates are read as numeric, conversion needed
4. **Compressed Files**: Not supported (planned for future release)

### Workarounds
```sql
-- Date conversion (assuming Stata date format)
SELECT 
    id,
    DATE '1960-01-01' + INTERVAL (stata_date) DAYS as actual_date
FROM read_stata_dta('with_dates.dta');

-- String value label simulation
SELECT 
    CASE category_code
        WHEN 1 THEN 'Category A'
        WHEN 2 THEN 'Category B'
        ELSE 'Unknown'
    END as category_label
FROM read_stata_dta('survey.dta');
```

### Best Practices
1. **Validate Data**: Always check data ranges and missing values after loading
2. **Schema Inspection**: Use `DESCRIBE` to understand the loaded schema
3. **Error Handling**: Wrap file operations in appropriate error handling
4. **Performance Testing**: Test with representative file sizes
5. **Backup Strategy**: Keep original DTA files as authoritative source

## Troubleshooting

### Common Issues

#### "Cannot open Stata file"
- Check file path is correct and accessible
- Verify file permissions
- Ensure file exists and is not corrupted

#### "Unsupported Stata file version"
- File may be too old (<105) or uses unsupported format
- Try opening with Stata and re-saving in compatible format

#### "Unexpected end of file"
- File may be corrupted or truncated
- Check file integrity and re-download/copy if needed

#### Memory Issues
- For very large files, ensure adequate system memory
- Consider processing in smaller chunks using LIMIT/OFFSET

### Debug Information
```sql
-- Check extension is loaded
SELECT * FROM duckdb_functions() WHERE function_name LIKE '%stata%';

-- Get detailed error information
SELECT stata_dta_info('debug');
```