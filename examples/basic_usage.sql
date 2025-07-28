-- Example usage of the Stata DTA extension for DuckDB
-- This demonstrates how to read Stata .dta files directly into DuckDB

-- Load the Stata DTA extension
LOAD 'stata_dta';

-- Basic usage: Read a simple Stata file
SELECT * FROM read_stata_dta('test/data/simple.dta');

-- Count rows in a dataset
SELECT COUNT(*) as total_rows FROM read_stata_dta('test/data/simple.dta');

-- Work with different data types
SELECT 
    name,
    age,
    salary,
    active
FROM read_stata_dta('test/data/mixed_types.dta')
WHERE age > 25;

-- Handle missing values
SELECT 
    id,
    score,
    CASE 
        WHEN score IS NULL THEN 'Missing'
        ELSE CAST(score as VARCHAR)
    END as score_status
FROM read_stata_dta('test/data/with_missing.dta');

-- Aggregation operations  
SELECT 
    AVG(value) as mean_value,
    STDDEV(value) as std_value,
    MIN(value) as min_value,
    MAX(value) as max_value
FROM read_stata_dta('test/data/simple.dta');

-- Join with other DuckDB tables
CREATE TEMP TABLE lookup AS 
SELECT 1 as id, 'First' as description
UNION ALL SELECT 2, 'Second'
UNION ALL SELECT 3, 'Third'  
UNION ALL SELECT 4, 'Fourth'
UNION ALL SELECT 5, 'Fifth';

SELECT 
    s.id,
    s.value,
    l.description
FROM read_stata_dta('test/data/simple.dta') s
JOIN lookup l ON s.id = l.id;

-- Performance test with large dataset
SELECT 
    category,
    COUNT(*) as count,
    AVG(random_float) as avg_float,
    PERCENTILE_CONT(0.5) WITHIN GROUP (ORDER BY random_int) as median_int
FROM read_stata_dta('test/data/large_dataset.dta')
GROUP BY category
ORDER BY avg_float;

-- Export results to CSV
COPY (
    SELECT * FROM read_stata_dta('test/data/simple.dta')
    WHERE value > 25.0
) TO 'output/filtered_data.csv' (HEADER);

-- Check extension info
SELECT stata_dta_info('version 1.0') as extension_info;

-- Schema inspection
DESCRIBE SELECT * FROM read_stata_dta('test/data/simple.dta');