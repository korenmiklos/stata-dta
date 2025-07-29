#!/usr/bin/env python3
"""
Generate test Stata DTA files for testing the DuckDB extension
"""

import pandas as pd
import numpy as np
import os
from pathlib import Path

def create_test_files():
    """Create various test .dta files for comprehensive testing"""
    
    # Create test directory
    test_dir = Path(__file__).parent / "data"
    test_dir.mkdir(exist_ok=True)
    
    # Test 1: Simple numeric data
    df_simple = pd.DataFrame({
        'id': [1, 2, 3, 4, 5],
        'value': [10.5, 20.3, 30.1, 40.7, 50.2],
        'count': [100, 200, 300, 400, 500]
    })
    df_simple.to_stata(test_dir / "simple.dta", version=114)
    print("Created simple.dta")
    
    # Test 2: Mixed data types
    df_mixed = pd.DataFrame({
        'id': [1, 2, 3, 4],
        'name': ['Alice', 'Bob', 'Charlie', 'Diana'], 
        'age': [25, 30, 35, 28],
        'salary': [50000.5, 60000.0, 75000.25, 55000.75],
        'active': [True, False, True, True]
    })
    df_mixed.to_stata(test_dir / "mixed_types.dta", version=114)
    print("Created mixed_types.dta")
    
    # Test 3: Data with missing values  
    df_missing = pd.DataFrame({
        'id': [1, 2, 3, 4, 5],
        'score': [85.5, np.nan, 92.3, 78.1, np.nan],
        'grade': ['A', 'B', '', 'C', 'A'],  # Use empty string instead of None
        'count': [10, 0, 30, 0, 50]  # Use 0 instead of boolean with None
    })
    df_missing.to_stata(test_dir / "with_missing.dta", version=114)
    print("Created with_missing.dta")
    
    # Test 4: Large dataset for performance testing
    np.random.seed(42)
    df_large = pd.DataFrame({
        'id': range(10000),
        'random_float': np.random.normal(0, 1, 10000),
        'random_int': np.random.randint(0, 1000, 10000),
        'category': np.random.choice(['A', 'B', 'C', 'D'], 10000),
        'sequence': range(10000, 20000)  # Avoid timestamp issues
    })
    df_large.to_stata(test_dir / "large_dataset.dta", version=114)
    print("Created large_dataset.dta")
    
    # Test 5: Different Stata versions
    df_version_test = pd.DataFrame({
        'x': [1, 2, 3],
        'y': [4.0, 5.0, 6.0],
        'z': ['hello', 'world', 'test']
    })
    
    # Create files in different versions
    for version in [113, 114, 115, 117, 118]:
        try:
            df_version_test.to_stata(test_dir / f"version_{version}.dta", version=version)
            print(f"Created version_{version}.dta")
        except Exception as e:
            print(f"Could not create version {version}: {e}")
    
    # Test 6: Empty dataset
    df_empty = pd.DataFrame({'col1': [], 'col2': []})
    df_empty.to_stata(test_dir / "empty.dta", version=114)
    print("Created empty.dta")
    
    # Test 7: Long strings
    df_strings = pd.DataFrame({
        'short_str': ['a', 'bb', 'ccc'],
        'medium_str': ['medium length string'] * 3,
        'long_str': ['This is a very long string that should test the string handling capabilities of our extension'] * 3
    })
    df_strings.to_stata(test_dir / "string_lengths.dta", version=114)
    print("Created string_lengths.dta")
    
    # Test 8: Special characters and encoding (ASCII only for compatibility)
    df_special = pd.DataFrame({
        'ascii': ['hello', 'world', 'test'],
        'punctuation': ['data!', 'value?', 'end.'],
        'numbers': ['123', '456', '789']
    })
    df_special.to_stata(test_dir / "special_chars.dta", version=114)
    print("Created special_chars.dta")
    
    print(f"\nAll test files created in: {test_dir}")
    print("Files created:")
    for dta_file in sorted(test_dir.glob("*.dta")):
        size = dta_file.stat().st_size
        print(f"  {dta_file.name} ({size} bytes)")

if __name__ == "__main__":
    create_test_files()