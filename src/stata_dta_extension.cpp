#define DUCKDB_EXTENSION_MAIN

#include "stata_dta_extension.hpp"
#include "stata_parser.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include <duckdb/parser/parsed_data/create_table_function_info.hpp>

// OpenSSL linked through vcpkg
#include <openssl/opensslv.h>

namespace duckdb {

// Stata DTA table function data
struct StataDtaBindData : public TableFunctionData {
	std::unique_ptr<StataReader> reader;
	std::string filename;
	vector<LogicalType> types;
	vector<string> names;
};

// Stata DTA table function
static unique_ptr<FunctionData> StataDtaBind(ClientContext &context, TableFunctionBindInput &input, vector<LogicalType> &return_types, vector<string> &names) {
	auto result = make_uniq<StataDtaBindData>();
	
	// Get filename from arguments
	if (input.inputs.empty() || input.inputs[0].IsNull()) {
		throw InvalidInputException("read_stata_dta requires a filename argument");
	}
	
	result->filename = StringValue::Get(input.inputs[0]);
	
	// Create and open reader
	result->reader = std::make_unique<StataReader>(result->filename);
	if (!result->reader->Open()) {
		throw IOException("Cannot open Stata file: " + result->filename);
	}
	
	// Get metadata from file
	const auto& variables = result->reader->GetVariables();
	const auto& header = result->reader->GetHeader();
	
	// Set up return types and names
	for (const auto& var : variables) {
		return_types.push_back(result->reader->StataTypeToLogicalType(var));
		names.push_back(var.name);
	}
	
	result->types = return_types;
	result->names = names;
	
	return std::move(result);
}

static void StataDtaFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.bind_data->Cast<StataDtaBindData>();
	
	if (!data.reader->HasMoreData()) {
		return; // No more data
	}
	
	auto chunk = data.reader->ReadChunk(STANDARD_VECTOR_SIZE);
	if (!chunk || chunk->size() == 0) {
		return;
	}
	
	// Copy data to output
	output.SetCardinality(chunk->size());
	for (idx_t col_idx = 0; col_idx < chunk->ColumnCount(); col_idx++) {
		output.data[col_idx].Reference(chunk->data[col_idx]);
	}
}

// Placeholder function - will show extension info
inline void StataDtaInfoFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(name_vector, result, args.size(), [&](string_t name) {
		return StringVector::AddString(result, "Stata DTA Extension " + name.GetString() + " - OpenSSL version: " +
		                                           OPENSSL_VERSION_TEXT);
	});
}

static void LoadInternal(DatabaseInstance &instance) {
	// Register Stata DTA table reading function
	TableFunction stata_read_function("read_stata_dta", {LogicalType::VARCHAR}, StataDtaFunction, StataDtaBind);
	stata_read_function.named_parameters["columns"] = LogicalType::LIST(LogicalType::VARCHAR);
	ExtensionUtil::RegisterFunction(instance, stata_read_function);

	// Register extension info function
	auto stata_info_function = ScalarFunction("stata_dta_info", {LogicalType::VARCHAR},
	                                                            LogicalType::VARCHAR, StataDtaInfoFun);
	ExtensionUtil::RegisterFunction(instance, stata_info_function);
}

void StataDtaExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
}
std::string StataDtaExtension::Name() {
	return "stata_dta";
}

std::string StataDtaExtension::Version() const {
#ifdef EXT_VERSION_STATA_DTA
	return EXT_VERSION_STATA_DTA;
#else
	return "1.0.0";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void stata_dta_init(duckdb::DatabaseInstance &db) {
	duckdb::DuckDB db_wrapper(db);
	db_wrapper.LoadExtension<duckdb::StataDtaExtension>();
}

DUCKDB_EXTENSION_API const char *stata_dta_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
