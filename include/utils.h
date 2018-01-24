#ifndef _INCLUDE_UTILS_H
#define _INCLUDE_UTILS_H

#include <cstdint>
#include <string>
#include <vector>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>

using namespace std;

class timer {
  clock_t c_start;
  time_t t_start;
public:
  timer();
  ~timer();
};
bool is_empty_dir(const char *);
uint64_t get_file_size(const char*);
string parse_datetime(uint64_t, bool);
string parse_datetime_iso8601(uint64_t, bool);
string parse_datetimemicro(uint64_t, bool);
string join(const vector<string>&, const char*);
uint16_t parse_file_attr(uint32_t, string*);
uint16_t parse_reason(uint32_t, string*);
string UTF16toUTF8(char16_t*, int);
bool is_valid_ts(uint64_t);
bool is_valid_usn(uint64_t);
string get_timezone_str (bool);
  
#endif // _INCLUDE_UTILS_H
