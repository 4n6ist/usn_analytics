#ifndef _INCLUDE_USNRECORD_H
#define _INCLUDE_USNRECORD_H

#include <cstdint>
#include <vector>
#include <cstring>
#include <iostream>

#ifndef _WIN32
#define MAX_PATH 260
#endif

#pragma pack(1)

extern bool raw; // true: output all of raw records, false: no output
extern bool lt; // true: localtime, false: utc

using namespace std;

struct USN_RECORD_V2 {
  uint32_t RecordLength;
  uint16_t MajorVersion;
  uint16_t MinorVersion;
  uint64_t FileReferenceNumber;
  uint64_t ParentFileReferenceNumber;
  uint64_t Usn;
  uint64_t TimeStamp;
  uint32_t Reason;
  uint32_t SourceInfo;
  uint32_t SecurityId;
  uint32_t FileAttributes;
  uint16_t FileNameLength;
  uint16_t FileNameOffset;
};

enum USN_REASON {
  OVERWRITE   = 0x00000001, // DATA_OVERWRITE
  EXTEND      = 0x00000002, // DATA_EXTEND
  TRUNC       = 0x00000004, // DATA_TRUNCATION
  NAMED_O     = 0x00000010, // NAMED_DATA_OVERWRITE
  NAMED_E     = 0x00000020, // NAMED_DATA_EXTEND
  NAMED_T     = 0x00000040, // NAMED_DATA_TRUNCATION
  CREATE      = 0x00000100, // FILE_CREATE
  DELETE      = 0x00000200, // FILE_DELETE
  EA          = 0x00000400, // EA_CHANGE
  SECURITY    = 0x00000800, // SECURITY_CHANGE 
  OLDNAME     = 0x00001000, // RENAME_OLD_NAME
  NEWNAME     = 0x00002000, // RENAME_NEW_NAME
  INDEX       = 0x00004000, // INDEXABLE_CHANGE
  INFO        = 0x00008000, // BASIC_INFO_CHANGE
  LINK        = 0x00010000, // HARD_LINK_CHANGE
  COMPRESS    = 0x00020000, // COMPRESSION_CHANGE
  ENCRYPT     = 0x00040000, // ENCRYPTION_CHANGE
  OBJECTID    = 0x00080000, // OBJECT_ID_CHANGE
  REPARSE     = 0x00100000, // REPARSE_POINT_CHANGE
  STREAM      = 0x00200000, // STREAM_CHANGE
  TRANSACT    = 0x00400000, // TRANSACTED_CHANGE
  INTEGRITY   = 0x00800000, // INTEGRITY_CHANGE
  RENAME      = 0x01000000, // added unofficially
  MOVE        = 0x02000000, // added unofficially
  CLOSE       = 0x80000000  // CLOSE
};

enum USN_RECORD_TYPE {
  V2_RECORD = 2,
  V3_RECORD = 3,
  V4_RECORD = 4,
  NOT_RECORD = -1,
  CORRUPT_RECORD = -2
};

enum FILE_ATTRIBUTE {
  RDONLY      = 0x00000001, // READ_ONLY
  HIDDEN      = 0x00000002, // HIDDEN
  SYSTEM      = 0x00000004, // SYSTEM
  FOLDER      = 0x00000010, // DIRECTORY
  ARCHIVE     = 0x00000020, // ARCHIVE
  DEV         = 0x00000040, // DEVICE
  NORMAL      = 0x00000080, // NORMAL
  TEMP        = 0x00000100, // TEMPORARY
  SPARSE      = 0x00000200, // SPARSE_FILE
  RP          = 0x00000400, // REPARSE_POINT
  COMP        = 0x00000800, // COMPRESSED
  OFFLINE     = 0x00001000, // OFFLINE
  NOINDEX     = 0x00002000, // NOT_INDEXED
  CRYPT       = 0x00004000, // ENCRYPTED
  ISTREAM     = 0x00008000, // INTEGRITY_STREAM
  NOSCRUB     = 0x00020000, // NO_SCRUB_DATA
  RECALL_OPEN = 0x00040000, // RECALL_ON_OPEN
  RECALL_DATA = 0x00400000  // RECALL_ON_DATA_ACCESS  
};

class UsnRecord {
private:
  FILE *fp_in;
  unsigned char *data;

private:
  int GetFileName();
  int ParseRecord();
  int WriteRecord(FILE*);
  
public:
  USN_RECORD_V2 usn_record;
  uint64_t offset;
  uint32_t cid; // actually should be uint48_t
  uint16_t cid_seq;
  uint32_t pid; // actually should be uint48_t
  uint16_t pid_seq;
  string file_name;

public:
  UsnRecord(FILE*);
  int IsValidRecord(uint64_t);
  int ReadRecord(uint64_t);
  int ReadParseRecord(uint64_t);
  int ReadParseWriteRecord(FILE*, uint64_t);
};

class UsnMain {
public:
  uint64_t usn;
  uint16_t rec_cnt;
  uint64_t timestamp_i;
  string timestamp_s;
  double time_taken;
  string file_name;
  uint32_t reasons_i;
  string reasons_s;
  uint32_t attrs_i;
  string attrs_s;
  uint32_t cid; // actually should be uint48_t
  uint32_t pid; // actually should be uint48_t
  string file_path;

public:
  UsnMain();
  int StoreRecord(UsnRecord*);
  int StoreRecord(UsnRecord*, uint16_t, double);
  int WriteBundledRecord(FILE*);
};

class UsnExecuted {
public:
  uint64_t usn;
  string timestamp_s;
  string exe_name;
  uint16_t exe_cnt;
  string file_name;
  string reasons_s;
  uint16_t rec_cnt;
  double time_taken;
  uint32_t cid; // actually should be uint48_t
  uint64_t timestamp_i;
  uint32_t reasons_i;

public:
  UsnExecuted();
  int StoreRecord(UsnMain*, uint16_t);
  int WriteExecutedRecord(FILE*);
};

class UsnOpened {
public:
  uint64_t usn;
  string timestamp_s;
  string file_path;
  string file_name;
  string reasons_s;
  uint16_t rec_cnt;
  double time_taken;
  uint32_t cid; // actually should be uint48_t
  uint32_t pid; // actually should be uint48_t
  uint64_t timestamp_i;
  uint32_t reasons_i;

public:
  UsnOpened();
  int StoreRecord(UsnMain*);
  int WriteOpenedRecord(FILE*);
};

#endif // _INCLUDE_USN_RECORD_H
