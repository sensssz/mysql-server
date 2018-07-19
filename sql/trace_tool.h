#ifndef SQL_TRACE_TOOL_H_
#define SQL_TRACE_TOOL_H_

#include "sql_class.h"

#include <memory>
#include <vector>

bool IsLongTrx(THD *thd);

struct RemainingTimeVariable {
	double mean;
	double variance;
	RemainingTimeVariable() : mean(0), variance(0) {}
	RemainingTimeVariable(double mean_in, double variance_in) :
	mean(mean_in), variance(variance_in) {}
};

class TraceTool {
public:
	static TraceTool &GetInstance();
	int ParseNewQuery(const char *query, size_t len);
	void AddRemainingTimeRecord(long remaining_time);
	void AddWaitTime(long wait_time);
	void SubmitWaitTime();
	const RemainingTimeVariable *GetRemainingTimeVariable(THD *thd);
	double AverageLatency() { return average_latency_; }
//	bool ShouldMeasure() { return remaining_time_variables_.get() == nullptr; }
	bool ShouldMeasure() { return true; }

private:
	struct TimeRecord {
		int trx_id;
		int trx_type;
		long time;
		TimeRecord(int trx_id_in, int trx_type_in, long time_in) :
		trx_id(trx_id_in), trx_type(trx_type_in), time(time_in) {}
		TimeRecord(TimeRecord &&) = default;
	};

	TraceTool();
	~TraceTool();
	int StringToInt(const char *str, size_t len);

	std::unique_ptr<RemainingTimeVariable[]> remaining_time_variables_;
	std::vector<TimeRecord> remaining_time_records_;
	std::vector<TimeRecord> wait_time_records_;
	double average_latency_;
};

#endif
