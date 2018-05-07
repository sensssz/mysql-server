#ifndef LOCK_MEM_RECORDER_H_
#define LOCK_MEM_RECORDER_H_

#include <atomic>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include <cstdint>

static thread_local bool should_log = false;
static thread_local int curr_cs = 0;
static thread_local int curr_run = 0;

static std::atomic<int> global_tid(0);
static thread_local int tid = global_tid.fetch_add(1);

static size_t cstr_hash(const char *str) {
    size_t result = 0;
    const size_t prime = 127;
    size_t i = 0;
    while (str[i] != '\0') {
        result = str[i] + (result * prime);
        i++;
    }
    return result;
}

struct MemAccess {
    uint32_t cs;
    uint32_t run;
    const char *parent;
    uint32_t parent_line;
    const char *func;
    uint32_t line;
    void *addr;
    size_t hash_val;

    MemAccess() = default;
    MemAccess(const char *&parent_in, uint32_t parent_line_in,
              const char *&func_in, uint32_t line_in, void *addr_in) :
        cs(curr_cs), run(curr_run), parent(parent_in),
        parent_line(parent_line_in), func(func_in),
        line(line_in), addr(addr_in) {
        hash_val = cs ^ run ^ cstr_hash(parent) ^ parent_line ^
            //    func == func ^ line == line ^
                   reinterpret_cast<uintptr_t>(addr);
    }

    bool operator==(const MemAccess &other) const {
        return cs == other.cs && run == other.run &&
               strcmp(parent, other.parent) == 0 &&
               parent_line == other.parent_line &&
            //    func == other.func && line == other.line &&
               addr == other.addr;
    }
};

std::ostream &operator<<(std::ostream &out, const MemAccess &access) {
    out << access.cs << ' ' << access.run << ' ' << access.parent << ' '
        << access.parent_line << ' ' << access.func << ' '
        << access.line << ' ' << access.addr;
    return out;
}

namespace std {
    template<>
    struct hash<MemAccess> {
        size_t operator()(const MemAccess &access) const {
            return access.hash_val;
        }
    };
}

static std::vector<std::unordered_set<MemAccess>> mem_accesses(1000);

__attribute__((used))
static void InsertRecord(
    const char *parent_func, uint32_t parent_line,
    const char *func, uint32_t line, void *addr) {
    if (should_log) {
        mem_accesses[tid].insert(MemAccess(parent_func, parent_line, func, line, addr));
    }
}

static void DumpMemAccess() {
    std::unordered_set<MemAccess> all_accesses;
    std::ofstream outfile("mem_access");
    for (auto &access : mem_accesses) {
        all_accesses.insert(access.begin(), access.end());
    }
    for (auto &access : all_accesses) {
        outfile << access << std::endl;
    }
    outfile.close();
}

#endif // LOCK_MEM_RECORDER_H_
