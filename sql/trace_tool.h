#ifndef SQL_TRACE_TOOL_H_
#define SQL_TRACE_TOOL_H_

#include "sql_class.h"

#include <memory>
#include <vector>

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
	const RemainingTimeVariable *GetRemainingTimeVariable(THD *thd);
	bool ShouldMeasure() { return remaining_time_variables_.get() == nullptr; }

private:
	struct RemainingTimeRecord {
		int trx_id;
		int trx_type;
		long remaining_time;
		RemainingTimeRecord(int trx_id_in, int trx_type_in, long remaining_time_in) :
		trx_id(trx_id_in), trx_type(trx_type_in), remaining_time(remaining_time_in) {}
		RemainingTimeRecord(RemainingTimeRecord &&) = default;
	};

	TraceTool();
	~TraceTool();
	int StringToInt(const char *str, size_t len);

	std::unique_ptr<RemainingTimeVariable[]> remaining_time_variables_;
	std::vector<RemainingTimeRecord> records_;
};

#endif
