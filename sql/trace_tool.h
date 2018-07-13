#ifndef SQL_TRACE_TOOL_H_
#define SQL_TRACE_TOOL_H_

#include <vector>

class TraceTool {
public:
	static TraceTool &GetInstance();
	bool ParseNewQuery(const char *query, size_t len);
	int GetCurrentTrxId();
	int GetCurrentTrxType();
	void AddRemainingTimeRecord(long remaining_time);

private:
	struct RemainingTimeRecord {
		int trx_id;
		int trx_type;
		long remaining_time;
		RemainingTimeRecord(int trx_id_in, int trx_type_in, long remaining_time_in) :
			trx_id(trx_id_in), trx_type(trx_type_in), remaining_time(remaining_time_in) {}
		RemainingTimeRecord(RemainingTimeRecord &&) = default;
	};

	TraceTool() {}
	~TraceTool();
	int StringToInt(const char *str, size_t len);

	std::vector<RemainingTimeRecord> records_;
};

#endif
