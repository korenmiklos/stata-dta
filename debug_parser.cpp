#include "src/include/stata_parser.hpp"
#include <iostream>

int main() {
    try {
        duckdb::StataReader reader("test/data/simple.dta");
        if (reader.Open()) {
            const auto& header = reader.GetHeader();
            const auto& variables = reader.GetVariables();
            
            std::cout << "File opened successfully" << std::endl;
            std::cout << "Version: " << (int)header.format_version << std::endl;
            std::cout << "Variables: " << header.nvar << std::endl;
            std::cout << "Observations: " << header.nobs << std::endl;
            std::cout << "Big endian: " << header.is_big_endian << std::endl;
            std::cout << std::endl;
            
            for (size_t i = 0; i < variables.size(); i++) {
                const auto& var = variables[i];
                std::cout << "Var " << i << ": name='" << var.name 
                         << "', type=" << (int)var.type 
                         << ", str_len=" << (int)var.str_len << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}