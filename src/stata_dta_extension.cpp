#define DUCKDB_EXTENSION_MAIN

#include "stata_dta_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

// OpenSSL linked through vcpkg
#include <openssl/opensslv.h>

namespace duckdb {

// Placeholder function - will be replaced with Stata DTA reading functionality
inline void StataDtaReadFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &filename_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(filename_vector, result, args.size(), [&](string_t filename) {
		return StringVector::AddString(result, "Reading Stata DTA file: " + filename.GetString());
	});
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
	// Register Stata DTA reading function (placeholder)
	auto stata_read_function = ScalarFunction("read_stata_dta", {LogicalType::VARCHAR}, LogicalType::VARCHAR, StataDtaReadFun);
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
