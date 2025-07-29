#pragma once

#include "duckdb.hpp"
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace duckdb {

enum class StataDataType : uint8_t {
    STR1_244 = 1,   // String types 1-244
    BYTE = 251,     // int8
    INT = 252,      // int16 
    LONG = 253,     // int32
    FLOAT = 254,    // float32
    DOUBLE = 255    // float64
};

struct StataVariable {
    std::string name;
    StataDataType type;
    uint8_t str_len;     // For string types
    std::string format;
    std::string label;
    std::string value_label_name;
};

struct StataHeader {
    uint8_t format_version;
    bool is_big_endian;
    uint8_t filetype;
    uint16_t nvar;
    uint64_t nobs;
    std::string data_label;
    std::string timestamp;
};

class StataParser {
public:
    StataParser();
    virtual ~StataParser() = default;
    
    // File reading utilities
    uint8_t ReadUInt8();
    uint16_t ReadUInt16();
    uint32_t ReadUInt32(); 
    uint64_t ReadUInt64();
    int8_t ReadInt8();
    int16_t ReadInt16();
    int32_t ReadInt32();
    float ReadFloat();
    double ReadDouble();
    std::string ReadString(size_t length);
    std::string ReadNullTerminatedString(size_t max_length);
    
    // Byte order handling
    void SetByteOrder(bool is_big_endian);
    template<typename T> T SwapBytes(T value);
    
    // Data type utilities
    LogicalType StataTypeToLogicalType(const StataVariable& var);
    bool IsStringType(StataDataType type);
    bool IsNumericType(StataDataType type);
    
    // Missing value detection
    bool IsMissingValue(const StataVariable& var, const void* data);
    
protected:
    unique_ptr<std::ifstream> file_stream_;
    bool is_big_endian_;
    bool native_is_big_endian_;
    
    // Stata type mappings
    std::map<uint8_t, StataDataType> old_type_mapping_;
    std::map<StataDataType, size_t> type_size_mapping_;
    
    // Missing value constants
    std::map<StataDataType, int64_t> missing_int_values_;
    std::map<StataDataType, double> missing_float_values_;
    
    void InitializeTypeMappings();
    void InitializeMissingValues();
};

class StataReader : public StataParser {
public:
    StataReader(const std::string& filename);
    ~StataReader();
    
    // Main interface
    bool Open();
    void Close();
    unique_ptr<DataChunk> ReadChunk(idx_t chunk_size = STANDARD_VECTOR_SIZE);
    
    // Metadata access
    const StataHeader& GetHeader() const { return header_; }
    const std::vector<StataVariable>& GetVariables() const { return variables_; }
    bool HasMoreData() const { return rows_read_ < header_.nobs; }
    
private:
    std::string filename_;
    StataHeader header_;
    std::vector<StataVariable> variables_;
    vector<LogicalType> column_types_;
    std::map<std::string, std::map<int32_t, std::string>> value_labels_;
    
    uint64_t data_location_;
    uint64_t rows_read_;
    
    // Header reading
    void ReadHeader();
    void ReadOldHeader(uint8_t first_char);
    void ReadNewHeader();
    
    // Variable info reading
    void ReadVariableTypes();
    void ReadVariableNames();
    void ReadSortOrder();
    void ReadFormats();
    void ReadValueLabelNames();
    void ReadVariableLabels();
    void ReadCharacteristics();
    void ReadValueLabels();
    
    // Data reading
    void PrepareDataReading();
    void ReadDataChunk(DataChunk& chunk, idx_t chunk_size);
    void ConvertStataValue(const StataVariable& var, const uint8_t* src_data, Vector& dest_vector, idx_t row_idx);
    
    // Utility functions
    std::string DecodeString(const uint8_t* data, size_t length);
    void SkipBytes(size_t count);
    uint64_t GetFilePosition();
    void SeekTo(uint64_t position);
    
    // Version-specific reading
    uint64_t ReadObsCount();
    std::string ReadDataLabel();
    std::string ReadTimestamp();
    
    // XML format helpers (version 117+)
    std::string FindXMLSection(const std::string& section_name);
};

} // namespace duckdb