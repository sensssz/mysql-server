#include "trace_tool.h"

#include <fstream>

#include <cstring>

namespace {

bool FileExists(const std::string &filename) {
	std::ifstream infile(filename);
	return !infile.fail();
}

}

thread_local int trx_id = -1;
thread_local int trx_type = -1;

TraceTool &TraceTool::GetInstance() {
	static TraceTool instance;
	return instance;
}

TraceTool::TraceTool() {
	std::ifstream remaining_time_file("../remaining_time_variables");
	if (remaining_time_file.fail()) {
		return;
	}
	int num_trx_types;
	remaining_time_file >> num_trx_types;
	remaining_time_variables_.reset(new RemainingTimeVariable[num_trx_types]);
	double mean;
	double variance;
	for (int i = 0; i < num_trx_types; i++) {
		remaining_time_file >> mean >> variance;
		remaining_time_variables_.get()[i].mean = mean / 1E6;
		remaining_time_variables_.get()[i].variance = variance / 1E12;
	}
}

TraceTool::~TraceTool() {
	int i = 1;
	while (::FileExists("../remaining_time." + std::to_string(i) + ".csv")) {
		i++;
	}
	std::ofstream remaining_time_file("../remaining_time." + std::to_string(i) + ".csv");
	for (auto &record : records_) {
		remaining_time_file << record.trx_id << ','
		<< record.trx_type << ',' << record.remaining_time << std::endl;
	}
}

int TraceTool::StringToInt(const char *str, size_t len) {
	int res = 0;
	size_t i = 0;
	if (str[0] == '-') {
		i = 1;
	}
	for (; i < len; i++) {
		res = res * 10 + (str[i] - '0');
	}
	if (str[0] == '-') {
		res = -res;
	}
	return res;
}

int TraceTool::ParseNewQuery(const char *query, size_t len)
{
	if (strncmp(query, "SET @trx_type", 13) != 0) {
		return -1;
	}
	size_t trx_id_start = 0;
	size_t trx_type_start = 0;
	while (query[trx_id_start] != '\'') {
		trx_id_start++;
	}
	trx_id_start++;
	trx_type_start = trx_id_start + 1;
	while (query[trx_type_start] != '-') {
		trx_type_start++;
	}
	trx_type_start++;
	trx_id = StringToInt(query + trx_id_start, trx_type_start - trx_id_start - 1);
	trx_type = StringToInt(query + trx_type_start, len - trx_type_start - 1);
	return trx_type;
}

void TraceTool::AddRemainingTimeRecord(long remaining_time) {
	if (trx_id == -1 || !ShouldMeasure() || remaining_time <= 0) {
		return;
	}
	records_.push_back(RemainingTimeRecord(trx_id, trx_type, remaining_time));
}

const RemainingTimeVariable *TraceTool::GetRemainingTimeVariable(THD *thd) {
	if (thd->trx_type == -1 || remaining_time_variables_.get() == nullptr) {
		return nullptr;
	}
	return remaining_time_variables_.get() + thd->trx_type;
}
