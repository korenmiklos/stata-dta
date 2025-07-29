#include "stata_parser.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/common/string_util.hpp"
#include <cstring>
#include <algorithm>

namespace duckdb {

StataParser::StataParser() : is_big_endian_(false) {
    // Determine native byte order
    uint16_t test = 1;
    native_is_big_endian_ = (*(uint8_t*)&test) == 0;
    
    InitializeTypeMappings();
    InitializeMissingValues();
}

void StataParser::InitializeTypeMappings() {
    // Old format type mappings (versions <= 115)
    old_type_mapping_[98] = StataDataType::BYTE;    // 'b'
    old_type_mapping_[105] = StataDataType::INT;    // 'i'  
    old_type_mapping_[108] = StataDataType::LONG;   // 'l'
    old_type_mapping_[102] = StataDataType::FLOAT;  // 'f'
    old_type_mapping_[100] = StataDataType::DOUBLE; // 'd'
    
    // Type size mappings
    type_size_mapping_[StataDataType::BYTE] = 1;
    type_size_mapping_[StataDataType::INT] = 2;
    type_size_mapping_[StataDataType::LONG] = 4;
    type_size_mapping_[StataDataType::FLOAT] = 4;
    type_size_mapping_[StataDataType::DOUBLE] = 8;
}

void StataParser::InitializeMissingValues() {
    // Missing value constants from Stata - using the highest reserved values
    missing_int_values_[StataDataType::BYTE] = 101;    // byte missing starts at 101
    missing_int_values_[StataDataType::INT] = 32741;   // int missing starts at 32741  
    missing_int_values_[StataDataType::LONG] = 2147483621;  // long missing starts at 2147483621
    
    // For floats and doubles, Stata uses IEEE 754 NaN values
    // We don't need to store specific values since we check with std::isnan()
    missing_float_values_[StataDataType::FLOAT] = std::numeric_limits<float>::quiet_NaN();
    missing_float_values_[StataDataType::DOUBLE] = std::numeric_limits<double>::quiet_NaN();
}

void StataParser::SetByteOrder(bool is_big_endian) {
    is_big_endian_ = is_big_endian;
}

template<typename T>
T StataParser::SwapBytes(T value) {
    if constexpr (sizeof(T) == 1) {
        return value;
    } else if constexpr (sizeof(T) == 2) {
        return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
    } else if constexpr (sizeof(T) == 4) {
        return ((value & 0xFF) << 24) | 
               (((value >> 8) & 0xFF) << 16) |
               (((value >> 16) & 0xFF) << 8) |
               ((value >> 24) & 0xFF);
    } else if constexpr (sizeof(T) == 8) {
        return ((value & 0xFF) << 56) |
               (((value >> 8) & 0xFF) << 48) |
               (((value >> 16) & 0xFF) << 40) |
               (((value >> 24) & 0xFF) << 32) |
               (((value >> 32) & 0xFF) << 24) |
               (((value >> 40) & 0xFF) << 16) |
               (((value >> 48) & 0xFF) << 8) |
               ((value >> 56) & 0xFF);
    }
    return value;
}

uint8_t StataParser::ReadUInt8() {
    if (!file_stream_ || !file_stream_->good()) {
        throw IOException("Cannot read from Stata file");
    }
    
    uint8_t value;
    file_stream_->read(reinterpret_cast<char*>(&value), sizeof(value));
    if (file_stream_->gcount() != sizeof(value)) {
        throw IOException("Unexpected end of Stata file");
    }
    return value;
}

uint16_t StataParser::ReadUInt16() {
    uint16_t value;
    file_stream_->read(reinterpret_cast<char*>(&value), sizeof(value));
    if (file_stream_->gcount() != sizeof(value)) {
        throw IOException("Unexpected end of Stata file");
    }
    
    if (is_big_endian_ != native_is_big_endian_) {
        value = SwapBytes(value);
    }
    return value;
}

uint32_t StataParser::ReadUInt32() {
    uint32_t value;
    file_stream_->read(reinterpret_cast<char*>(&value), sizeof(value));
    if (file_stream_->gcount() != sizeof(value)) {
        throw IOException("Unexpected end of Stata file");
    }
    
    if (is_big_endian_ != native_is_big_endian_) {
        value = SwapBytes(value);
    }
    return value;
}

uint64_t StataParser::ReadUInt64() {
    uint64_t value;
    file_stream_->read(reinterpret_cast<char*>(&value), sizeof(value));
    if (file_stream_->gcount() != sizeof(value)) {
        throw IOException("Unexpected end of Stata file");
    }
    
    if (is_big_endian_ != native_is_big_endian_) {
        value = SwapBytes(value);
    }
    return value;
}

int8_t StataParser::ReadInt8() {
    return static_cast<int8_t>(ReadUInt8());
}

int16_t StataParser::ReadInt16() {
    return static_cast<int16_t>(ReadUInt16());
}

int32_t StataParser::ReadInt32() {
    return static_cast<int32_t>(ReadUInt32());
}

float StataParser::ReadFloat() {
    uint32_t int_value = ReadUInt32();
    float float_value;
    std::memcpy(&float_value, &int_value, sizeof(float));
    return float_value;
}

double StataParser::ReadDouble() {
    uint64_t int_value = ReadUInt64();
    double double_value;
    std::memcpy(&double_value, &int_value, sizeof(double));
    return double_value;
}

std::string StataParser::ReadString(size_t length) {
    std::vector<char> buffer(length);
    file_stream_->read(buffer.data(), length);
    if (static_cast<size_t>(file_stream_->gcount()) != length) {
        throw IOException("Unexpected end of Stata file while reading string");
    }
    
    return std::string(buffer.data(), length);
}

std::string StataParser::ReadNullTerminatedString(size_t max_length) {
    std::vector<char> buffer(max_length);
    file_stream_->read(buffer.data(), max_length);
    if (static_cast<size_t>(file_stream_->gcount()) != max_length) {
        throw IOException("Unexpected end of Stata file while reading string");
    }
    
    // Find null terminator
    size_t actual_length = 0;
    for (size_t i = 0; i < max_length; i++) {
        if (buffer[i] == '\0') {
            actual_length = i;
            break;
        }
    }
    
    return std::string(buffer.data(), actual_length);
}

LogicalType StataParser::StataTypeToLogicalType(const StataVariable& var) {
    switch (var.type) {
        case StataDataType::BYTE:
            return LogicalType::TINYINT;
        case StataDataType::INT:
            return LogicalType::SMALLINT;
        case StataDataType::LONG:
            return LogicalType::INTEGER;
        case StataDataType::FLOAT:
            return LogicalType::FLOAT;
        case StataDataType::DOUBLE:
            return LogicalType::DOUBLE;
        default:
            // String types (1-244)
            if (static_cast<uint8_t>(var.type) >= 1 && static_cast<uint8_t>(var.type) <= 244) {
                return LogicalType::VARCHAR;
            }
            throw NotImplementedException("Unsupported Stata data type");
    }
}

bool StataParser::IsStringType(StataDataType type) {
    uint8_t type_code = static_cast<uint8_t>(type);
    return type_code >= 1 && type_code <= 244;
}

bool StataParser::IsNumericType(StataDataType type) {
    return !IsStringType(type);
}

bool StataParser::IsMissingValue(const StataVariable& var, const void* data) {
    if (IsStringType(var.type)) {
        return false; // Strings don't have missing values in the same sense
    }
    
    switch (var.type) {
        case StataDataType::BYTE: {
            int8_t value = *static_cast<const int8_t*>(data);
            // Missing values start at 101 and go up to 127 (., .a, .b, ..., .z)
            return value >= missing_int_values_[StataDataType::BYTE];
        }
        case StataDataType::INT: {
            int16_t value = *static_cast<const int16_t*>(data);
            // Missing values start at 32741 and go up to 32767
            return value >= missing_int_values_[StataDataType::INT];
        }
        case StataDataType::LONG: {
            int32_t value = *static_cast<const int32_t*>(data);
            // Missing values start at 2147483621 and go up to 2147483647
            return value >= missing_int_values_[StataDataType::LONG];
        }
        case StataDataType::FLOAT: {
            float value = *static_cast<const float*>(data);
            // Stata uses IEEE 754 NaN values for missing floats
            return std::isnan(value);
        }
        case StataDataType::DOUBLE: {
            double value = *static_cast<const double*>(data);
            // Stata missing values for doubles are very large finite values
            // The base missing value (.) is approximately 8.988e+307
            // All missing values are >= this threshold
            return value >= 8.988e+307;
        }
        default:
            return false;
    }
}

// Explicit template instantiations
template uint16_t StataParser::SwapBytes<uint16_t>(uint16_t);
template uint32_t StataParser::SwapBytes<uint32_t>(uint32_t);
template uint64_t StataParser::SwapBytes<uint64_t>(uint64_t);
template int StataParser::SwapBytes<int>(int);
template short StataParser::SwapBytes<short>(short);

} // namespace duckdb