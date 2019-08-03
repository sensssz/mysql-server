#include "trace_tool.h"

#include <fstream>

#include <cstring>

namespace {

bool FileExists(const std::string &filename) {
	std::ifstream infile(filename);
	return !infile.fail();
}

void RollAverage(double &average, long &num_values, int new_value) {
	double num_values_new = num_values + 1;
	average = average / num_values_new * num_values + new_value / num_values_new;
	num_values++;
}

}

bool IsLongTrx(THD *thd) {
	return thd != nullptr && thd->trx_type == 5;
}

thread_local int trx_id = -1;
thread_local int trx_type = -1;
thread_local long trx_wait_time = 0;

TraceTool &TraceTool::GetInstance() {
	static TraceTool instance;
	return instance;
}

TraceTool::TraceTool() : average_remaining_time_(0), total_num_remainings_(0), average_latency_(0) {
	std::ifstream remaining_time_file("../remaining_time_variables");
	if (remaining_time_file.fail()) {
		return;
	}
	int num_trx_types;
	remaining_time_file >> num_trx_types;
	remaining_time_variables_.reset(new RemainingTimeVariable[num_trx_types]);
	int num_trx;
	double mean;
	double variance;
	for (int i = 0; i < num_trx_types; i++) {
		remaining_time_file >> num_trx >> mean >> variance;
		remaining_time_variables_.get()[i].mean = mean / 1E6;
		remaining_time_variables_.get()[i].variance = variance / 1E12;
		average_latency_ += mean / 1E6;
	}
	average_latency_ /= num_trx_types;
}

TraceTool::~TraceTool() {
	int i = 1;
	while (::FileExists("../stats/remaining_time." + std::to_string(i) + ".csv")) {
		i++;
	}
	if (remaining_time_records_.size() > 20) {
		std::ofstream remaining_time_file("../stats/remaining_time." + std::to_string(i) + ".csv");
		for (auto &record : remaining_time_records_) {
			remaining_time_file << record.trx_id << ',' << record.trx_type
													<< ',' << record.time << std::endl;
		}
	}
	if (wait_time_records_.size() > 20) {
		std::ofstream wait_time_file("../stats/wait_time." + std::to_string(i) + ".csv");
		for (auto &record : wait_time_records_) {
			wait_time_file << record.trx_id << ',' << record.trx_type
										 << ',' << record.time << std::endl;
		}
	}
	if (dep_size_records_.size() > 20) {
		std::ofstream dep_size_file("../stats/dep_size_time." + std::to_string(i) + ".csv");
		for (auto &record : dep_size_records_) {
			dep_size_file << record.trx_id << ',' << record.trx_type
										<< ',' << record.time << std::endl;
		}
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
	trx_wait_time = 0;
	trx_id = StringToInt(query + trx_id_start, trx_type_start - trx_id_start - 1);
	trx_type = StringToInt(query + trx_type_start, len - trx_type_start - 1);
	return trx_type;
}

void TraceTool::AddRemainingTimeRecord(long remaining_time) {
	if (!ShouldMeasure() || remaining_time <= 0) {
		return;
	}
	::RollAverage(average_remaining_times_[trx_type], num_remainings_[trx_type], remaining_time);
	::RollAverage(average_remaining_time_, total_num_remainings_, remaining_time);
//	remaining_time_records_.push_back(TimeRecord(trx_id, trx_type, remaining_time));
}

void TraceTool::AddWaitTime(long wait_time) {
	trx_wait_time += wait_time;
}

void TraceTool::SubmitWaitTime() {
	if (!ShouldMeasure()) {
		return;
	}
	wait_time_records_.push_back(TimeRecord(trx_id, trx_type, trx_wait_time));
}

void TraceTool::AddDepSizeRecord(THD *thd, long dep_size) {
	if (!ShouldMeasure()) {
		return;
	}
	int trx_type = (thd == nullptr) ? -1 : thd->trx_type;
	dep_size_records_.push_back(TimeRecord(-1, trx_type, dep_size));
}

const RemainingTimeVariable *TraceTool::GetRemainingTimeVariable(THD *thd) {
	if (remaining_time_variables_.get() == nullptr) {
		return nullptr;
	}
	if (thd->trx_type == -1) {
		return remaining_time_variables_.get();
	} else {
		return remaining_time_variables_.get() + thd->trx_type + 1;
	}
}

double TraceTool::GetRemainingTime(THD *thd) {
	if (thd->trx_type == -1) {
		return average_remaining_time_;
	} else {
		return average_remaining_times_[thd->trx_type];
	}
}
