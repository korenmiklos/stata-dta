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
    // For XML format (version 117+), we need to parse XML tags properly
    // Read the entire header as a string first to locate tags
    
    // Read a large chunk to find the header section
    uint64_t start_pos = GetFilePosition() - 1; // -1 because we already read the '<'
    SeekTo(start_pos);
    
    std::string header_xml = ReadString(500); // Read enough to get all header info
    
    // Parse version from <release>VERSION</release>
    size_t release_start = header_xml.find("<release>");
    size_t release_end = header_xml.find("</release>");
    if (release_start == std::string::npos || release_end == std::string::npos) {
        throw IOException("Invalid XML format: could not find release tag");
    }
    std::string version_str = header_xml.substr(release_start + 9, release_end - release_start - 9);
    header_.format_version = std::stoi(version_str);
    
    // Parse byte order from <byteorder>ORDER</byteorder>
    size_t byteorder_start = header_xml.find("<byteorder>");
    size_t byteorder_end = header_xml.find("</byteorder>");
    if (byteorder_start == std::string::npos || byteorder_end == std::string::npos) {
        throw IOException("Invalid XML format: could not find byteorder tag");
    }
    std::string byteorder_str = header_xml.substr(byteorder_start + 11, byteorder_end - byteorder_start - 11);
    header_.is_big_endian = (byteorder_str[0] == 'M'); // MSF = big-endian, LSF = little-endian
    SetByteOrder(header_.is_big_endian);
    
    // Parse number of variables from <K>BINARY_DATA</K>
    size_t k_start = header_xml.find("<K>");
    if (k_start == std::string::npos) {
        throw IOException("Invalid XML format: could not find K tag");
    }
    // Position after the <K> tag to read binary data
    SeekTo(start_pos + k_start + 3);
    header_.nvar = ReadUInt16();
    
    // Parse number of observations from <N>BINARY_DATA</N>
    size_t n_start = header_xml.find("<N>");
    if (n_start == std::string::npos) {
        throw IOException("Invalid XML format: could not find N tag");
    }
    // Position after the <N> tag to read binary data
    SeekTo(start_pos + n_start + 3);
    header_.nobs = ReadObsCount();
    
    // Parse data label from <label>BINARY_DATA</label>
    size_t label_start = header_xml.find("<label>");
    if (label_start != std::string::npos) {
        SeekTo(start_pos + label_start + 7);
        header_.data_label = ReadDataLabel();
    }
    
    // Parse timestamp from <timestamp>TEXT</timestamp>
    size_t timestamp_start = header_xml.find("<timestamp>");
    size_t timestamp_end = header_xml.find("</timestamp>");
    if (timestamp_start != std::string::npos && timestamp_end != std::string::npos) {
        header_.timestamp = header_xml.substr(timestamp_start + 11, timestamp_end - timestamp_start - 11);
    }
    
    // Find the end of the header section to continue parsing
    size_t header_end = header_xml.find("</header>");
    if (header_end != std::string::npos) {
        SeekTo(start_pos + header_end + 9);
    } else {
        throw IOException("Invalid XML format: could not find header end tag");
    }
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
    
    if (header_.format_version >= 117) {
        // XML format: find <variable_types> section
        std::string section_data = FindXMLSection("variable_types");
        
        // Parse binary data within the XML section
        // For version 118+, there seems to be padding bytes, so we skip every other byte
        for (uint16_t i = 0; i < header_.nvar; i++) {
            size_t byte_index = (header_.format_version >= 118) ? i * 2 : i;
            if (byte_index >= section_data.length()) {
                throw IOException("Invalid variable types section: insufficient data");
            }
            uint8_t type_code = static_cast<uint8_t>(section_data[byte_index]);
            
            // For version 118+, handle the new type codes
            if (type_code >= 1 && type_code <= 244) {
                variables_[i].type = static_cast<StataDataType>(type_code);
                variables_[i].str_len = type_code;
            } else if (type_code == 248) {
                // Version 118+: Based on analysis, 248 appears to be LONG/INT32, not DOUBLE
                variables_[i].type = StataDataType::LONG;
            } else if (type_code == 254) {
                variables_[i].type = StataDataType::FLOAT;
            } else if (type_code == 253) {
                variables_[i].type = StataDataType::LONG;
            } else if (type_code == 252) {
                variables_[i].type = StataDataType::INT;
            } else if (type_code == 251) {
                variables_[i].type = StataDataType::BYTE;
            } else {
                variables_[i].type = static_cast<StataDataType>(type_code);
            }
        }
    } else {
        // Binary format
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
}

void StataReader::ReadVariableNames() {
    if (header_.format_version >= 117) {
        // XML format: find <varnames> section
        std::string section_data = FindXMLSection("varnames");
        
        // Parse null-terminated strings from the section data
        size_t pos = 0;
        size_t name_length = (header_.format_version <= 117) ? 33 : 129;
        
        for (uint16_t i = 0; i < header_.nvar; i++) {
            // Each variable name is fixed-width (129 bytes for version 118+)
            size_t start_pos = i * name_length;
            if (start_pos >= section_data.length()) {
                throw IOException("Invalid variable names section: insufficient data");
            }
            
            // Extract the fixed-width name and find null terminator
            std::string raw_name = section_data.substr(start_pos, 
                std::min(name_length, section_data.length() - start_pos));
            size_t null_pos = raw_name.find('\0');
            
            if (null_pos != std::string::npos) {
                variables_[i].name = raw_name.substr(0, null_pos);
            } else {
                variables_[i].name = raw_name;
            }
        }
    } else {
        // Binary format
        size_t name_length = (header_.format_version <= 117) ? 33 : 129;
        
        for (uint16_t i = 0; i < header_.nvar; i++) {
            variables_[i].name = ReadNullTerminatedString(name_length);
        }
    }
}

void StataReader::ReadSortOrder() {
    if (header_.format_version >= 117) {
        // XML format: find <sortlist> section and skip
        try {
            std::string section_data = FindXMLSection("sortlist");
            // Just skip the sort order data for now
        } catch (const IOException&) {
            // Sort order section might not exist, that's okay
        }
    } else {
        // Binary format: Sort order: 2 bytes per variable + 2 bytes for count
        size_t sort_size = 2 * (header_.nvar + 1);
        SkipBytes(sort_size);
    }
}

void StataReader::ReadFormats() {
    if (header_.format_version >= 117) {
        // XML format: find <formats> section
        std::string section_data = FindXMLSection("formats");
        
        // Parse null-terminated strings from the section data
        size_t pos = 0;
        size_t format_length = (header_.format_version <= 117) ? 49 : 57;
        
        for (uint16_t i = 0; i < header_.nvar; i++) {
            if (pos >= section_data.length()) {
                variables_[i].format = "";
                continue;
            }
            
            // Find the null terminator
            size_t format_end = section_data.find('\0', pos);
            if (format_end == std::string::npos) {
                variables_[i].format = section_data.substr(pos);
                break;
            } else {
                variables_[i].format = section_data.substr(pos, format_end - pos);
                // Move to next format (fixed-width alignment)
                pos = ((pos / format_length) + 1) * format_length;
            }
        }
    } else {
        // Binary format
        size_t format_length = (header_.format_version <= 117) ? 49 : 57;
        
        for (uint16_t i = 0; i < header_.nvar; i++) {
            variables_[i].format = ReadNullTerminatedString(format_length);
        }
    }
}

void StataReader::ReadValueLabelNames() {
    if (header_.format_version >= 117) {
        // XML format: find <value_label_names> section
        try {
            std::string section_data = FindXMLSection("value_label_names");
            
            // Parse null-terminated strings from the section data
            size_t pos = 0;
            size_t label_length = (header_.format_version <= 117) ? 33 : 129;
            
            for (uint16_t i = 0; i < header_.nvar; i++) {
                if (pos >= section_data.length()) {
                    variables_[i].value_label_name = "";
                    continue;
                }
                
                // Find the null terminator
                size_t name_end = section_data.find('\0', pos);
                if (name_end == std::string::npos) {
                    variables_[i].value_label_name = section_data.substr(pos);
                    break;
                } else {
                    variables_[i].value_label_name = section_data.substr(pos, name_end - pos);
                    // Move to next name (fixed-width alignment)
                    pos = ((pos / label_length) + 1) * label_length;
                }
            }
        } catch (const IOException&) {
            // Value label names section might not exist, set empty strings
            for (uint16_t i = 0; i < header_.nvar; i++) {
                variables_[i].value_label_name = "";
            }
        }
    } else {
        // Binary format
        size_t label_length = (header_.format_version <= 117) ? 33 : 129;
        
        for (uint16_t i = 0; i < header_.nvar; i++) {
            variables_[i].value_label_name = ReadNullTerminatedString(label_length);
        }
    }
}

void StataReader::ReadVariableLabels() {
    if (header_.format_version >= 117) {
        // XML format: find <variable_labels> section
        try {
            std::string section_data = FindXMLSection("variable_labels");
            
            // Parse null-terminated strings from the section data
            size_t pos = 0;
            size_t label_length = (header_.format_version <= 117) ? 81 : 321;
            
            for (uint16_t i = 0; i < header_.nvar; i++) {
                if (pos >= section_data.length()) {
                    variables_[i].label = "";
                    continue;
                }
                
                // Find the null terminator
                size_t label_end = section_data.find('\0', pos);
                if (label_end == std::string::npos) {
                    variables_[i].label = section_data.substr(pos);
                    break;
                } else {
                    variables_[i].label = section_data.substr(pos, label_end - pos);
                    // Move to next label (fixed-width alignment)
                    pos = ((pos / label_length) + 1) * label_length;
                }
            }
        } catch (const IOException&) {
            // Variable labels section might not exist, set empty strings
            for (uint16_t i = 0; i < header_.nvar; i++) {
                variables_[i].label = "";
            }
        }
    } else {
        // Binary format
        size_t label_length = (header_.format_version <= 117) ? 81 : 321;
        
        for (uint16_t i = 0; i < header_.nvar; i++) {
            variables_[i].label = ReadNullTerminatedString(label_length);
        }
    }
}

void StataReader::ReadCharacteristics() {
    // Skip characteristics for now - they're optional metadata
    if (header_.format_version >= 117) {
        // XML format: find <characteristics> section and skip
        try {
            std::string section_data = FindXMLSection("characteristics");
            // Just skip the characteristics data for now
        } catch (const IOException&) {
            // Characteristics section might not exist, that's okay
        }
    }
    // For older versions, characteristics are typically not present or minimal
}

void StataReader::ReadValueLabels() {
    // Value labels are complex - skip for initial implementation
    // TODO: Implement value label reading
    if (header_.format_version >= 117) {
        // XML format: find <value_labels> section and skip
        try {
            std::string section_data = FindXMLSection("value_labels");
            // Just skip the value labels data for now
        } catch (const IOException&) {
            // Value labels section might not exist, that's okay
        }
    }
    // For older versions, value labels are typically at the end and can be skipped
}

void StataReader::PrepareDataReading() {
    if (header_.format_version >= 117) {
        // XML format: Find the <data> section
        SeekTo(0);
        file_stream_->seekg(0, std::ios::end);
        uint64_t file_size = file_stream_->tellg();
        SeekTo(0);
        
        std::string file_content(file_size, '\0');
        file_stream_->read(&file_content[0], file_size);
        
        // Look for the <data> tag
        size_t data_start_tag = file_content.find("<data>");
        size_t data_end_tag = file_content.find("</data>");
        
        if (data_start_tag != std::string::npos && data_end_tag != std::string::npos) {
            data_location_ = data_start_tag + 6; // After "<data>"
            
            // Calculate actual data size from XML boundaries
            uint64_t xml_data_size = data_end_tag - (data_start_tag + 6);
            
            // Calculate expected row size
            uint64_t expected_row_size = 0;
            for (const auto& var : variables_) {
                if (IsStringType(var.type)) {
                    expected_row_size += var.str_len;
                } else {
                    expected_row_size += type_size_mapping_[var.type];
                }
            }
            
            // Adjust number of observations if the data section is smaller
            if (expected_row_size > 0) {
                uint64_t max_possible_rows = xml_data_size / expected_row_size;
                if (max_possible_rows < header_.nobs) {
                    header_.nobs = max_possible_rows;
                }
            }
        } else {
            throw IOException("Could not find <data> section in XML format file");
        }
    } else {
        data_location_ = GetFilePosition();
        
        // FIXME: There's a 5-byte offset issue with version 114 files generated by pandas
        // This needs to be investigated and fixed properly by analyzing the file format
        if (header_.format_version == 114) {
            data_location_ += 5;
        }
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

std::string StataReader::FindXMLSection(const std::string& section_name) {
    // Save current position
    uint64_t original_pos = GetFilePosition();
    
    // Go back to start of file to search for the section
    SeekTo(0);
    
    // Read the entire file (or a very large portion)
    file_stream_->seekg(0, std::ios::end);
    uint64_t file_size = file_stream_->tellg();
    SeekTo(0);
    
    // Read all the file content
    std::string file_content(file_size, '\0');
    file_stream_->read(&file_content[0], file_size);
    
    // Find the section tags
    std::string start_tag = "<" + section_name + ">";
    std::string end_tag = "</" + section_name + ">";
    
    size_t start_pos = file_content.find(start_tag);
    size_t end_pos = file_content.find(end_tag);
    
    if (start_pos == std::string::npos || end_pos == std::string::npos) {
        // Restore position and throw error
        SeekTo(original_pos);
        throw IOException("Could not find XML section: " + section_name);
    }
    
    size_t content_start = start_pos + start_tag.length();
    std::string section_content = file_content.substr(content_start, end_pos - content_start);
    
    // Restore original position
    SeekTo(original_pos);
    
    return section_content;
}

} // namespace duckdb