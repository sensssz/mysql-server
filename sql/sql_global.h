#ifndef SQL_GLOBAL_H
#define SQL_GLOBAL_H

#include <map>

class THD;

extern std::map<THD*, int> ldsf_effsize;

#endif
