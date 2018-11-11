#include "usnrecord.h"
#include "utils.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <iostream>

using namespace std;

UsnRecord::UsnRecord(FILE* fp) {
  fp_in = fp;
}

#ifdef __APPLE__
#  define fseeko64 fseeko
#endif


// Check specified offset starts from USN_RECORD
// return: USN_RECORD_TYPE
int UsnRecord::IsValidRecord(uint64_t _offset) {

  UsnRecord::ReadRecordHeader(_offset);

  // validation check
  if(usn_record_header.RecordLength < 64 || usn_record_header.RecordLength > 576 || usn_record_header.RecordLength % 8 != 0)
    return NOT_RECORD;
      
  if(usn_record_header.MinorVersion != 0)
    return NOT_RECORD;
  
  if(usn_record_header.MajorVersion == 2) {
    UsnRecord::ReadRecord(_offset);
    if(usn_record.FileNameOffset != 60)
      return NOT_RECORD;
	  if(is_valid_ts(usn_record.TimeStamp) && is_valid_usn(usn_record.Usn)
      && usn_record.Reason != 0 && usn_record.FileNameLength < 512 && usn_record.FileNameLength % 2 == 0)
		  return V2_RECORD;
	  else
		  return CORRUPT_RECORD;
  }
  else if(usn_record_header.MajorVersion == 3) {
    UsnRecord::ReadRecordV3(_offset); 
    if(usn_record.FileNameOffset != 76)
      return NOT_RECORD;
	  if(is_valid_ts(usn_record.TimeStamp) && is_valid_usn(usn_record.Usn)
      && usn_record.Reason != 0 && usn_record.FileNameLength < 512 && usn_record.FileNameLength % 2 == 0)
		  return V3_RECORD;
	  else
		  return CORRUPT_RECORD;
  }
  else if(usn_record_header.MajorVersion == 4)
    return V4_RECORD;
  else
    return NOT_RECORD;
}

// Read as USN_RECORD_COMMON_HEADER at offset and store usn_record_header member 
int UsnRecord::ReadRecordHeader(uint64_t _offset) {
  
  offset = _offset;
  data = (unsigned char*)malloc(sizeof(USN_RECORD_COMMON_HEADER));

  fseeko64(fp_in, offset, SEEK_SET);

  if(fread(data, 1, sizeof(USN_RECORD_COMMON_HEADER), fp_in) != sizeof(USN_RECORD_COMMON_HEADER)) {
    free(data);
    return -1;
  }

  memcpy(&(usn_record_header), data, sizeof(USN_RECORD_COMMON_HEADER));
    
  free(data);
  return 0;
}

// Read as USN_RECORD_V2 at offset and store usn_record member 
int UsnRecord::ReadRecord(uint64_t _offset) {
  
  offset = _offset;
  data = (unsigned char*)malloc(sizeof(USN_RECORD_V2));

  fseeko64(fp_in, offset, SEEK_SET);

  if(fread(data, 1, sizeof(USN_RECORD_V2), fp_in) != sizeof(USN_RECORD_V2)) {
    free(data);
    return -1;
  }

  memcpy(&(usn_record), data, sizeof(USN_RECORD_V2));
    
  free(data);
  return 0;
}

// Read as USN_RECORD_V3 at offset, store, and move from usn_record_v3 to usn_record member 
int UsnRecord::ReadRecordV3(uint64_t _offset) {
  
  offset = _offset;
  data = (unsigned char*)malloc(sizeof(USN_RECORD_V3));

  fseeko64(fp_in, offset, SEEK_SET);

  if(fread(data, 1, sizeof(USN_RECORD_V3), fp_in) != sizeof(USN_RECORD_V3)) {
    free(data);
    return -1;
  }

  // Re-read as V3 Structure  
  memcpy(&(usn_record_v3), data, sizeof(USN_RECORD_V3));
  usn_record.RecordLength = usn_record_v3.RecordLength;
  usn_record.MajorVersion = usn_record_v3.MajorVersion;
  usn_record.MinorVersion = usn_record_v3.MinorVersion;
  usn_record.FileReferenceNumber = usn_record_v3.FileReferenceNumber2;
  usn_record.ParentFileReferenceNumber = usn_record_v3.ParentFileReferenceNumber2;
  usn_record.Usn = usn_record_v3.Usn;
  usn_record.TimeStamp = usn_record_v3.TimeStamp;
  usn_record.Reason = usn_record_v3.Reason;
  usn_record.SourceInfo = usn_record_v3.SourceInfo;
  usn_record.SecurityId = usn_record_v3.SecurityId;
  usn_record.FileAttributes = usn_record_v3.FileAttributes;
  usn_record.FileNameLength = usn_record_v3.FileNameLength;
  usn_record.FileNameOffset = usn_record_v3.FileNameOffset;
    
  free(data);
  return 0;
}


// Parse fileid, parentid and filename
int UsnRecord::ParseRecord() {
  
  cid = uint32_t(usn_record.FileReferenceNumber & 0x0000FFFFFFFFFFFF);
  cid_seq = uint16_t(usn_record.FileReferenceNumber >> 48);
  pid = uint32_t(usn_record.ParentFileReferenceNumber & 0x0000FFFFFFFFFFFF);
  pid_seq = uint16_t(usn_record.ParentFileReferenceNumber >> 48);
  UsnRecord::GetFileName();

  return 0;
}

// Read file name and Convert UTF16 to UTF8
int UsnRecord::GetFileName() {

  data = (unsigned char*)malloc(usn_record.FileNameLength);
  fseeko64(fp_in, offset + usn_record.FileNameOffset, SEEK_SET);
  if(fread(data, 1, usn_record.FileNameLength, fp_in) != usn_record.FileNameLength) {
    free(data);
    return -1;
  }

  file_name = UTF16toUTF8((char16_t*)(data), usn_record.FileNameLength/2);

  // remove illegal characer
  size_t pos;
  while((pos = file_name.find_first_of("/:*?\"<>|\\\t\r\n")) != string::npos)
    file_name.erase(pos, 1);
  
  if (usn_record.FileAttributes & FOLDER)
    file_name += "\\";
  
  free(data);
  return 0;
}

int UsnRecord::ReadParseRecord(uint64_t offset) {
  UsnRecord::ReadRecordHeader(offset);
  if(usn_record_header.MajorVersion == 2)
    UsnRecord::ReadRecord(offset);
  else if(usn_record_header.MajorVersion == 3)
    UsnRecord::ReadRecordV3(offset);
  else
    return -1; // unknown error
  UsnRecord::ParseRecord();
  return 0;
}

// Write a record with all fields
int UsnRecord::WriteRecord(FILE *fp) {
  string reasons_s, attrs_s, timestamp_s;
  fprintf(fp, "\"%llu\"\t", offset);
  fprintf(fp, "\"%u\"\t", usn_record.RecordLength);
  fprintf(fp, "\"%hu\"\t", usn_record.MajorVersion);
  fprintf(fp, "\"%hu\"\t", usn_record.MinorVersion);
  fprintf(fp, "\"%u\"\t", cid);
  fprintf(fp, "\"%hu\"\t", cid_seq);
  fprintf(fp, "\"%u\"\t", pid);
  fprintf(fp, "\"%hu\"\t", pid_seq);
  fprintf(fp, "\"%llu\"\t", usn_record.Usn);
  timestamp_s = parse_datetimemicro(usn_record.TimeStamp, lt);
  fprintf(fp, "\"%s\"\t", timestamp_s.c_str());
  parse_reason(usn_record.Reason, &(reasons_s));
  fprintf(fp, "\"%s(%08x)\"\t", reasons_s.c_str(), usn_record.Reason);
  fprintf(fp, "\"%u\"\t", usn_record.SourceInfo);
  fprintf(fp, "\"%u\"\t", usn_record.SecurityId);
  parse_file_attr(usn_record.FileAttributes, &(attrs_s));
  fprintf(fp, "\"%s(%04x)\"\t", attrs_s.c_str(), usn_record.FileAttributes);
  fprintf(fp, "\"%hu\"\t", usn_record.FileNameLength);
  fprintf(fp, "\"%hu\"\t", usn_record.FileNameOffset);  
  fprintf(fp, "\"%s\"", file_name.c_str());
  fprintf(fp, "\n");
  return 0;
}

int UsnRecord::ReadParseWriteRecord(FILE* fp, uint64_t offset) {
  UsnRecord::ReadRecordHeader(offset);
  if(usn_record_header.MajorVersion == 2)
    UsnRecord::ReadRecord(offset);
  else if(usn_record_header.MajorVersion == 3)
    UsnRecord::ReadRecordV3(offset);
  else
    return -1; // unknown error
  UsnRecord::ParseRecord();
  UsnRecord::WriteRecord(fp);
  return 0;
}

UsnMain::UsnMain() {
}

int UsnMain::StoreRecord(UsnRecord* ur) {
  usn = ur->usn_record.Usn;
  timestamp_i = ur->usn_record.TimeStamp;
  timestamp_s = parse_datetimemicro(ur->usn_record.TimeStamp, lt);
  file_name = ur->file_name;
  reasons_i = ur->usn_record.Reason;
  parse_reason(ur->usn_record.Reason, &(reasons_s));
  attrs_i = ur->usn_record.FileAttributes;
  parse_file_attr(ur->usn_record.FileAttributes, &(attrs_s)); 
  cid = ur->cid;
  pid = ur->pid;
  return 0;
}

int UsnMain::StoreRecord(UsnRecord* ur, uint16_t _rec_cnt, double _time_taken) {
  usn = ur->usn_record.Usn;
  rec_cnt = _rec_cnt;
  timestamp_i = ur->usn_record.TimeStamp;
  timestamp_s = parse_datetimemicro(ur->usn_record.TimeStamp, lt);
  time_taken = _time_taken;
  file_name = ur->file_name;
  reasons_i = ur->usn_record.Reason;
  parse_reason(ur->usn_record.Reason, &(reasons_s));
  attrs_i = ur->usn_record.FileAttributes;
  parse_file_attr(ur->usn_record.FileAttributes, &(attrs_s)); 
  cid = ur->cid;
  pid = ur->pid;
  return 0;
}

// Write a record with primary fields
int UsnMain::WriteBundledRecord(FILE *fp) {
  fprintf(fp, "\"%llu\"\t", usn);
  fprintf(fp, "\"%u\"\t", rec_cnt);
  fprintf(fp, "\"%s\"\t", timestamp_s.c_str());
  fprintf(fp, "\"%f\"\t", time_taken);
  fprintf(fp, "\"%s\"\t", file_name.c_str());
  fprintf(fp, "\"%s\"\t", reasons_s.c_str());
  fprintf(fp, "\"%s\"\t", attrs_s.c_str());
  fprintf(fp, "\"%u\"\t", cid);
  fprintf(fp, "\"%u\"\t", pid);
  fprintf(fp, "\"%s\"\t", file_path.c_str());
  fprintf(fp, "\n");
  return 0;
}

UsnExecuted::UsnExecuted() {
}

int UsnExecuted::StoreRecord(UsnMain* um, uint16_t _exe_cnt) {
  usn = um->usn;
  timestamp_s = um->timestamp_s;
  file_name = um->file_name;
  exe_name = file_name.substr(0, file_name.size()-12); // "-XXXXXXXX.pf" length 
  transform(exe_name.begin(), exe_name.end(), exe_name.begin(), ::tolower);  
  exe_cnt = _exe_cnt;
  rec_cnt = um->rec_cnt;
  reasons_s = um->reasons_s;  
  time_taken = um->time_taken;
  cid = um->cid;
  return 0;
}

// Write a record for prefetch file record
int UsnExecuted::WriteExecutedRecord(FILE *fp) {
  fprintf(fp, "\"%llu\"\t", usn);
  fprintf(fp, "\"%s\"\t", timestamp_s.c_str());
  fprintf(fp, "\"%s\"\t", exe_name.c_str());
  fprintf(fp, "\"%u\"\t", exe_cnt);
  fprintf(fp, "\"%s\"\t", file_name.c_str());
  fprintf(fp, "\"%s\"\t", reasons_s.c_str());
  fprintf(fp, "\"%u\"\t", rec_cnt);
  fprintf(fp, "\"%f\"\t", time_taken);
  fprintf(fp, "\"%u\"\t", cid);
  fprintf(fp, "\n");
  return 0;
}

UsnOpened::UsnOpened() {
}

int UsnOpened::StoreRecord(UsnMain* um) {
  usn = um->usn;
  timestamp_s = um->timestamp_s;
  file_path = um->file_path;
  file_name = um->file_name;
  reasons_s = um->reasons_s;  
  rec_cnt = um->rec_cnt;
  time_taken = um->time_taken;
  cid = um->cid;
  pid = um->pid;
  return 0;
}

// Write a record for lnk/objectid file
int UsnOpened::WriteOpenedRecord(FILE *fp) {
  fprintf(fp, "\"%llu\"\t", usn);
  fprintf(fp, "\"%s\"\t", timestamp_s.c_str());
  fprintf(fp, "\"%s\"\t", file_path.c_str());
  fprintf(fp, "\"%s\"\t", file_name.c_str());
  fprintf(fp, "\"%s\"\t", reasons_s.c_str());
  fprintf(fp, "\"%u\"\t", rec_cnt);
  fprintf(fp, "\"%f\"\t", time_taken);
  fprintf(fp, "\"%u\"\t", cid);
  fprintf(fp, "\"%u\"\t", pid);
  fprintf(fp, "\n");
  return 0;
}

