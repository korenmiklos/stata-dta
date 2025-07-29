#include "stata_parser.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/vector.hpp"

namespace duckdb {

StataReader::StataReader(const std::string& filename) 
    : filename_(filename), data_location_(0), rows_read_(0) {
}

StataReader::~StataReader() {
    Close();
}

bool StataReader::Open() {
    try {
        file_stream_ = make_uniq<std::ifstream>(filename_, std::ios::binary);
        if (!file_stream_->is_open()) {
            throw IOException("Cannot open Stata file: " + filename_);
        }
        
        ReadHeader();
        ReadVariableTypes();
        ReadVariableNames();
        ReadSortOrder();
        ReadFormats();
        ReadValueLabelNames();
        ReadVariableLabels();
        ReadCharacteristics();
        ReadValueLabels();
        PrepareDataReading();
        
        return true;
    } catch (const IOException& e) {
        Close();
        throw;  // Re-throw IOException as-is
    } catch (const std::exception& e) {
        Close();
        throw IOException(std::string(e.what()));
    }
}

void StataReader::Close() {
    if (file_stream_) {
        file_stream_->close();
        file_stream_.reset();
    }
}

void StataReader::ReadHeader() {
    uint8_t first_char = ReadUInt8();
    
    if (first_char == '<') {
        ReadNewHeader();
    } else {
        ReadOldHeader(first_char);
    }
    
    // Validate format version
    if (header_.format_version < 105 || header_.format_version > 119) {
        throw InvalidInputException(
            "Unsupported Stata file version: %d. Supported versions: 105, 108, 111, 113-119",
            header_.format_version
        );
    }
}

void StataReader::ReadOldHeader(uint8_t first_char) {
    header_.format_version = first_char;
    
    // Read byte order
    uint8_t byteorder = ReadUInt8();
    // Fix byte order detection: 0x2 = little-endian (LSF), 0x1 = big-endian (MSF)
    header_.is_big_endian = (byteorder == 0x1);
    SetByteOrder(header_.is_big_endian);
    
    // Read file type
    header_.filetype = ReadUInt8();
    
    // Skip unused byte
    SkipBytes(1);
    
    // Read number of variables
    header_.nvar = ReadUInt16();
    
    // Read number of observations
    header_.nobs = ReadObsCount();
    
    // Read data label and timestamp
    header_.data_label = ReadDataLabel();
    header_.timestamp = ReadTimestamp();
}

void StataReader::ReadNewHeader() {
    // Skip the opening tag: "stata_dta><header><release>"
    SkipBytes(27);
    
    // Read format version (3 bytes)
    std::string version_str = ReadString(3);
    header_.format_version = std::stoi(version_str);
    
    // Skip to byte order
    SkipBytes(21); // ></release><byteorder>
    std::string byteorder_str = ReadString(3); // "LSF" or "MSF"  
    header_.is_big_endian = (byteorder_str[0] == 'M'); // MSF = big-endian, LSF = little-endian
    SetByteOrder(header_.is_big_endian);
    
    // Skip to number of variables  
    SkipBytes(14); // </byteorder><K>
    header_.nvar = ReadUInt16();
    
    // Skip to number of observations
    SkipBytes(7); // </K><N>
    header_.nobs = ReadObsCount();
    
    // Skip to data label
    SkipBytes(18); // </N><label><data_label>
    header_.data_label = ReadDataLabel();
    
    // Skip to timestamp
    SkipBytes(30); // </data_label></label><timestamp>
    header_.timestamp = ReadTimestamp();
    
    // Skip closing tags
    SkipBytes(19); // </timestamp></header>
}

uint64_t StataReader::ReadObsCount() {
    if (header_.format_version >= 118) {
        return ReadUInt64();
    } else {
        return ReadUInt32();
    }
}

std::string StataReader::ReadDataLabel() {
    if (header_.format_version >= 118) {
        uint16_t length = ReadUInt16();
        return ReadString(length);
    } else if (header_.format_version == 117) {
        return ReadNullTerminatedString(81);
    } else {
        return ReadNullTerminatedString(81);
    }
}

std::string StataReader::ReadTimestamp() {
    if (header_.format_version >= 117) {
        return ReadNullTerminatedString(18);
    } else {
        return ReadNullTerminatedString(18);
    }
}

void StataReader::ReadVariableTypes() {
    variables_.resize(header_.nvar);
    
    for (uint16_t i = 0; i < header_.nvar; i++) {
        uint8_t type_code = ReadUInt8();
        
        if (header_.format_version <= 115 && old_type_mapping_.count(type_code)) {
            variables_[i].type = old_type_mapping_[type_code];
        } else if (type_code >= 1 && type_code <= 244) {
            variables_[i].type = static_cast<StataDataType>(type_code);
            variables_[i].str_len = type_code;
        } else {
            variables_[i].type = static_cast<StataDataType>(type_code);
        }
        
    }
}

void StataReader::ReadVariableNames() {
    size_t name_length = (header_.format_version <= 117) ? 33 : 129;
    
    for (uint16_t i = 0; i < header_.nvar; i++) {
        variables_[i].name = ReadNullTerminatedString(name_length);
    }
}

void StataReader::ReadSortOrder() {
    // Sort order: 2 bytes per variable + 2 bytes for count
    size_t sort_size = 2 * (header_.nvar + 1);
    SkipBytes(sort_size);
}

void StataReader::ReadFormats() {
    size_t format_length = (header_.format_version <= 117) ? 49 : 57;
    
    for (uint16_t i = 0; i < header_.nvar; i++) {
        variables_[i].format = ReadNullTerminatedString(format_length);
    }
}

void StataReader::ReadValueLabelNames() {
    size_t label_length = (header_.format_version <= 117) ? 33 : 129;
    
    for (uint16_t i = 0; i < header_.nvar; i++) {
        variables_[i].value_label_name = ReadNullTerminatedString(label_length);
    }
}

void StataReader::ReadVariableLabels() {
    size_t label_length = (header_.format_version <= 117) ? 81 : 321;
    
    for (uint16_t i = 0; i < header_.nvar; i++) {
        variables_[i].label = ReadNullTerminatedString(label_length);
    }
}

void StataReader::ReadCharacteristics() {
    // Skip characteristics for now - they're optional metadata
    // In newer formats, this section has a length prefix
    if (header_.format_version >= 117) {
        // Skip <characteristics> tag and content
        while (true) {
            std::string tag = ReadString(1);
            if (tag == "<") {
                // Look for </characteristics>
                std::string possible_end = ReadString(15);
                if (possible_end == "/characteristic") {
                    SkipBytes(2); // s>
                    break;
                }
            }
        }
    }
}

void StataReader::ReadValueLabels() {
    // Value labels are complex - skip for initial implementation
    // TODO: Implement value label reading
    if (header_.format_version >= 117) {
        // Skip to data section
        while (true) {
            std::string tag = ReadString(1);
            if (tag == "<") {
                std::string possible_data = ReadString(4);
                if (possible_data == "data") {
                    SkipBytes(1); // >
                    break;
                }
            }
        }
    }
}

void StataReader::PrepareDataReading() {
    data_location_ = GetFilePosition();
    
    // FIXME: There's a 5-byte offset issue with version 114 files generated by pandas
    // This needs to be investigated and fixed properly by analyzing the file format
    if (header_.format_version == 114) {
        data_location_ += 5;
    }
    
    // Prepare column types for DuckDB
    column_types_.reserve(header_.nvar);
    for (const auto& var : variables_) {
        column_types_.push_back(StataTypeToLogicalType(var));
    }
}

unique_ptr<DataChunk> StataReader::ReadChunk(idx_t chunk_size) {
    if (!HasMoreData()) {
        return nullptr;
    }
    
    auto chunk = make_uniq<DataChunk>();
    chunk->Initialize(Allocator::DefaultAllocator(), column_types_, chunk_size);
    
    ReadDataChunk(*chunk, chunk_size);
    
    return chunk;
}

void StataReader::ReadDataChunk(DataChunk& chunk, idx_t chunk_size) {
    // Calculate actual rows to read
    idx_t rows_to_read = std::min(chunk_size, static_cast<idx_t>(header_.nobs - rows_read_));
    
    if (rows_to_read == 0) {
        chunk.SetCardinality(0);
        return;
    }
    
    // Seek to correct position
    uint64_t row_size = 0;
    for (const auto& var : variables_) {
        if (IsStringType(var.type)) {
            row_size += var.str_len;
        } else {
            row_size += type_size_mapping_[var.type];
        }
    }
    
    uint64_t data_offset = data_location_ + (rows_read_ * row_size);
    
    
    SeekTo(data_offset);
    
    // Read data for each row
    for (idx_t row = 0; row < rows_to_read; row++) {
        for (size_t col = 0; col < variables_.size(); col++) {
            const auto& var = variables_[col];
            auto& vector = chunk.data[col];
            
            if (IsStringType(var.type)) {
                std::string str_data = ReadString(var.str_len);
                // Remove null terminator and trailing nulls
                size_t actual_len = str_data.find('\0');
                if (actual_len != std::string::npos) {
                    str_data = str_data.substr(0, actual_len);
                }
                FlatVector::GetData<string_t>(vector)[row] = StringVector::AddString(vector, str_data);
            } else {
                std::vector<uint8_t> raw_data(type_size_mapping_[var.type]);
                file_stream_->read(reinterpret_cast<char*>(raw_data.data()), raw_data.size());
                
                ConvertStataValue(var, raw_data.data(), vector, row);
            }
        }
    }
    
    chunk.SetCardinality(rows_to_read);
    rows_read_ += rows_to_read;
}

void StataReader::ConvertStataValue(const StataVariable& var, const uint8_t* src_data, Vector& dest_vector, idx_t row_idx) {
    if (IsMissingValue(var, src_data)) {
        FlatVector::SetNull(dest_vector, row_idx, true);
        return;
    }
    
    switch (var.type) {
        case StataDataType::BYTE: {
            int8_t value = *reinterpret_cast<const int8_t*>(src_data);
            FlatVector::GetData<int8_t>(dest_vector)[row_idx] = value;
            break;
        }
        case StataDataType::INT: {
            int16_t value = *reinterpret_cast<const int16_t*>(src_data);
            if (is_big_endian_ != native_is_big_endian_) {
                value = SwapBytes(value);
            }
            FlatVector::GetData<int16_t>(dest_vector)[row_idx] = value;
            break;
        }
        case StataDataType::LONG: {
            int32_t value = *reinterpret_cast<const int32_t*>(src_data);
            if (is_big_endian_ != native_is_big_endian_) {
                value = SwapBytes(value);
            }
            FlatVector::GetData<int32_t>(dest_vector)[row_idx] = value;
            break;
        }
        case StataDataType::FLOAT: {
            uint32_t int_value = *reinterpret_cast<const uint32_t*>(src_data);
            if (is_big_endian_ != native_is_big_endian_) {
                int_value = SwapBytes(int_value);
            }
            float value;
            std::memcpy(&value, &int_value, sizeof(float));
            FlatVector::GetData<float>(dest_vector)[row_idx] = value;
            break;
        }
        case StataDataType::DOUBLE: {
            uint64_t int_value = *reinterpret_cast<const uint64_t*>(src_data);
            if (is_big_endian_ != native_is_big_endian_) {
                int_value = SwapBytes(int_value);
            }
            double value;
            std::memcpy(&value, &int_value, sizeof(double));
            FlatVector::GetData<double>(dest_vector)[row_idx] = value;
            break;
        }
        default:
            throw NotImplementedException("Unsupported Stata data type in conversion");
    }
}

std::string StataReader::DecodeString(const uint8_t* data, size_t length) {
    // Basic ASCII decoding - could be enhanced for other encodings
    std::string result(reinterpret_cast<const char*>(data), length);
    
    // Remove null terminators
    size_t null_pos = result.find('\0');
    if (null_pos != std::string::npos) {
        result = result.substr(0, null_pos);
    }
    
    return result;
}

void StataReader::SkipBytes(size_t count) {
    file_stream_->seekg(count, std::ios::cur);
}

uint64_t StataReader::GetFilePosition() {
    return file_stream_->tellg();
}

void StataReader::SeekTo(uint64_t position) {
    file_stream_->seekg(position);
}

} // namespace duckdb