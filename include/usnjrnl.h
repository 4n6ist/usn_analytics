#ifndef _INCLUDE_USNJRNL_H
#define _INCLUDE_USNJRNL_H

#include <cstdint>
#include <cstdio>
#include <vector>
#include <map>

#include "usnrecord.h"

#pragma pack(1)

extern bool raw; // true: output all of raw records, false: no output
extern char SEP;

using namespace std;

class UsnRecord;
class UsnMain;
class UsnExecuted;
class UsnOpened;

// To maintain directory name to id historically
struct historical_dir {
  string name;
  uint32_t pid; // actually should be uint48_t
  uint64_t usn;
};

class UsnJrnl {
private:
  int WriteBundledHeader(FILE*, bool);
  int WriteExecutedHeader(FILE*, bool);
  int WriteOpenedHeader(FILE*, bool);
  int WriteAllHeader(FILE*, bool);
  int GetAllDirName();
  historical_dir GetHistoricalFileName(historical_dir, uint8_t);

public:
  uint64_t file_size;
  uint64_t offset;
  FILE *fp_in;
  FILE *fp_ofreport;
  vector<uint64_t> usn_set;
  vector<uint64_t> corrupt_offset_set;
  vector<UsnMain> usnmain_set;
  map<uint64_t, uint64_t> usn_table; // usn_set, offset
  multimap<uint32_t, historical_dir> dir_table; // id, name(current dir)/pid/usn
  multimap<uint32_t, historical_dir> path_table; // id, name(fullpath dir)/pid/usn

public:
  UsnJrnl(char*, char*);
  ~UsnJrnl();
  int GetAllUsnOffset();
  int PreProcess();
  int CheckRecords();
  int PostProcess();
  int WriteSuspiciousInfo();
  int WriteBundledRecords(char*, bool);
  int WriteExecutedRecords(char*, bool);
  int WriteOpenedRecords(char*, bool);
  int WriteAllRecords(char*, bool);
  void FindInsertCount(string*, string, map<string, uint16_t>*);
  void WriteFileNameList(string, map<string, uint16_t>*);
};

#endif // _INCLUDE_USNJRNL_H
