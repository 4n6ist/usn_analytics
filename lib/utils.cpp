#include "utils.h"
#include "usnrecord.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <ctype.h>
#include <ctime>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h> // opendir, readdir, closedir

using namespace std;


// To handle processing time
timer::timer() {
  c_start = clock();
  t_start = time(NULL);
}
timer::~timer() {
  clock_t c_end;
  time_t t_end;
  c_end = clock();
  t_end = time(NULL);
  printf("CPU Time: %f\n", (double)(c_end-c_start) / CLOCKS_PER_SEC);
  printf("Processed Time: %ld(s)\n", (t_end-t_start));
}

// Check specified directory is empty
bool is_empty_dir(const char *out_dname){
  DIR *pdir;

  if((pdir = opendir(out_dname)) == NULL)
    return false;

  struct dirent *pent;

  // check there is at least one file
  for(pent = readdir(pdir); pent != NULL; pent = readdir(pdir)) {
    if(strcmp(pent->d_name, ".") != 0 && strcmp(pent->d_name, "..") != 0) {// ignore "." and ".."
      closedir(pdir);
      return false;
    }
  }
  closedir(pdir);
  return true;
}

// Get file size specified with *in_fname 
uint64_t get_file_size(const char *in_fname) {
  FILE *fp;
#ifdef _WIN32
  struct __stat64 st;
#elif __APPLE__
  struct stat st;
#else
  struct stat64 st;
#endif
  int fd;

  fd = open(in_fname, O_RDONLY);
  if (fd == -1)
    perror("cant open file");

  fp = fdopen(fd, "rb");

  if (fp == NULL)
    perror("cant open file");

#ifdef _WIN32
  if (_fstat64(fd, &st) == -1)
#elif __APPLE__
  if (fstat(fd, &st) == -1)
#else
  if (fstat64(fd, &st) == -1)
#endif
    perror("cant get file state");

  if (fclose(fp) != 0)
    perror("cant close file");

  return st.st_size;
}

// Check a specified value is proper USN 
bool is_valid_usn(uint64_t value) {
  if (value > 10000000000000) // means 10T
	  return false;
  else 
	  return true;
}

// Check a specified value is proper timestamp
bool is_valid_ts(uint64_t value) {
  // 11644473600 seconds from 1601/01/01 to 1970/01/01
  time_t epoch = value/(10*1000*1000) - 11644473600L;
  // unix epoch 946684800 is 2000/01/01, 2524608000 is 2050/01/01, 4102444800 is 2100/01/01
  if(epoch < 946684800 || epoch > 2524608000)
    return false;
  else
    return true;  
}

// Convert a specified value as FILETIME to human readable string (s)
string parse_datetime(uint64_t _time, bool lt) {
  char buf[32];
  struct tm *tm_info;
  time_t epoch;

  // 11644473600 seconds from 1601/01/01 to 1970/01/01
  epoch = _time/(10*1000*1000) - 11644473600L;

  if(lt)
    tm_info = localtime(&epoch);
  else
    tm_info = gmtime(&epoch);

  strftime(buf, 20, "%Y/%m/%d %H:%M:%S", tm_info);
  
  return string(buf);
}

// Convert a specified value as FILETIME to human readable string (s)
string parse_datetime_iso8601(uint64_t _time, bool lt) {
  char buf[32];
  struct tm *tm_info;
  time_t epoch;

  // 11644473600 seconds from 1601/01/01 to 1970/01/01
  epoch = _time/(10*1000*1000) - 11644473600L;

  if(lt)
    tm_info = localtime(&epoch);
  else
    tm_info = gmtime(&epoch);

  strftime(buf, 16, "%Y%m%dT%H%M%S", tm_info);
  
  return string(buf);
}

// Convert a specified value as FILETIME to human readable string (us)
string parse_datetimemicro(uint64_t _time, bool lt) {  
  char buf1[32], buf2[7];
  struct tm *tm_info;
  int microseconds;
  time_t epoch;
  string time_str;
 
  // 11644473600.0 seconds from 1601/01/01 to 1970/01/01
  epoch = _time/(10*1000*1000) - 11644473600L;
  microseconds = (_time%(10*1000*1000))/10;
 
  if(lt)
    tm_info = localtime(&epoch);
  else
    tm_info = gmtime(&epoch);
 
  strftime(buf1, 20, "%Y/%m/%d %H:%M:%S", tm_info);
  sprintf(buf2, "%06d", microseconds);  
  time_str = string(buf1) + "." + string(buf2);
  
  return time_str;
}

// Multiple string list join up with delimiter to one string
// ref: http://marycore.jp/prog/cpp/vector-join/
string join(const vector<string>& v, const char* delim = 0) {
  std::string s;
  if (!v.empty()) {
    s += v[0];
    for (decltype(v.size()) i = 1, c = v.size(); i < c; ++i) {
      if (delim) s += delim;
      s += v[i];
    }
  }
  return s;
}

// Convert a specified value as FILE_ATTRIBUTE to human readable string
// in: flags, out: file_attrs_str
// return: flag element count
uint16_t parse_file_attr(uint32_t flags, string *file_attrs_str) {
  vector<string> file_attr_str;
  
  if (flags & RDONLY)
    file_attr_str.push_back("RDONLY");
  if (flags & HIDDEN)
    file_attr_str.push_back("HIDDEN");
  if (flags & SYSTEM)
    file_attr_str.push_back("SYSTEM");
  if (flags & FOLDER)
    file_attr_str.push_back("FOLDER");
  if (flags & ARCHIVE)
    file_attr_str.push_back("ARCHIVE");
  if (flags & DEV)
    file_attr_str.push_back("DEV");
  if (flags & NORMAL)
    file_attr_str.push_back("NORMAL");
  if (flags & TEMP)
    file_attr_str.push_back("TEMP");
  if (flags & SPARSE)
    file_attr_str.push_back("SPARSE");
  if (flags & RP)
    file_attr_str.push_back("RP");
  if (flags & COMP)
    file_attr_str.push_back("COMP");
  if (flags & OFFLINE)
    file_attr_str.push_back("OFFLINE");
  if (flags & NOINDEX)
    file_attr_str.push_back("NOINDEX");
  if (flags & CRYPT)
    file_attr_str.push_back("CRYPT");
  if (flags & STREAM)
    file_attr_str.push_back("STREAM");
  if (flags & NOSCRUB)
    file_attr_str.push_back("NOSCRUB");
  if (flags & RECALL_OPEN)
    file_attr_str.push_back("RECALL_OPEN");
  if (flags & RECALL_DATA)
    file_attr_str.push_back("RECALL_DATA");
  
  *file_attrs_str = join(file_attr_str, "|");
  return file_attr_str.size();
}

// Convert a specified value as USN_REASON to human readable string
// in: flags, out: reasons_str
// return: flag element count
uint16_t parse_reason(uint32_t flags, string *reasons_str) {
  vector<string> reason_str;
  
  if (flags & CREATE)
    reason_str.push_back("CREATE");  
  if (flags & EXTEND)
    reason_str.push_back("EXTEND");
  if (flags & OVERWRITE)
    reason_str.push_back("OVERWRITE");
  if (flags & TRUNC)
    reason_str.push_back("TRUNC");
  if (flags & DELETE)
    reason_str.push_back("DELETE");
  if (flags & OLDNAME)
    reason_str.push_back("OLDNAME");
  if (flags & NEWNAME)
    reason_str.push_back("NEWNAME");
  if (flags & INFO)
    reason_str.push_back("INFO");
  if (flags & SECURITY)
    reason_str.push_back("SECURITY");
  if (flags & OBJECTID)
    reason_str.push_back("OBJECTID");
  if (flags & EA)
    reason_str.push_back("EA");
  if (flags & COMPRESS)
    reason_str.push_back("COMPRESS");
  if (flags & ENCRYPT)
    reason_str.push_back("ENCRYPT");
  if (flags & LINK)
    reason_str.push_back("LINK");
  if (flags & INDEX)
    reason_str.push_back("INDEX");
  if (flags & REPARSE)
    reason_str.push_back("REPARSE");
  if (flags & STREAM)
    reason_str.push_back("STREAM");
  if (flags & NAMED_O)
    reason_str.push_back("NAMED_O");
  if (flags & NAMED_E)
    reason_str.push_back("NAMED_E");
  if (flags & NAMED_T)
    reason_str.push_back("NAMED_T");
  if (flags & TRANSACT)
    reason_str.push_back("TRANSACT");
  if (flags & INTEGRITY)
    reason_str.push_back("INTEGRITY");
  if (flags & RENAME)
    reason_str.push_back("RENAME");
  if (flags & MOVE)
    reason_str.push_back("MOVE");
  if (flags & CLOSE)
    reason_str.push_back("CLOSE");

  *reasons_str = join(reason_str, "|");
  return reason_str.size();
}

// Convert UTF16 to UTF8
// [in] s: UTF16 string, n: size
// return: UTF8 string
// ref: ntfs-3g, unistr.c
string UTF16toUTF8(char16_t *s, int n) {
  int half = 0;
  char *out = (char*)malloc(6);
  string res;

  for (int i = 0; i < n; ++i) {
    uint16_t c = s[i];
    char *t = out;
    memset(t, 0, 6);
    if (half) {
      if ((c >= 0xdc00) && (c < 0xe000)) {
        *t++ = 0xf0 + (((half + 64) >> 8) & 7);
        *t++ = 0x80 + (((half + 64) >> 2) & 63);
        *t++ = 0x80 + ((c >> 6) & 15) + ((half & 3) << 4);
        *t++ = 0x80 + (c & 63);
        half = 0;
      } else {
        return "<Can't Convert>";
      }
    } else if (c < 0x80) {
      *t++ = c;
    } else {
      if (c < 0x800) {
        *t++ = (0xc0 | ((c >> 6) & 0x3f));
        *t++ = 0x80 | (c & 0x3f);
      } else if (c < 0xd800) {
        *t++ = 0xe0 | (c >> 12);
        *t++ = 0x80 | ((c >> 6) & 0x3f);
        *t++ = 0x80 | (c & 0x3f);
      } else if (c < 0xdc00)
        half = c;
      else if (c >= 0xe000) {
        *t++ = 0xe0 | (c >> 12);
        *t++ = 0x80 | ((c >> 6) & 0x3f);
        *t++ = 0x80 | (c & 0x3f);
      } else {
        return "<Can't Convert>";
      }
    }
    res += string(out);
  }
  free(out);
  return res;
}


// Get time zone designators (±hh:mm) on running PC
// if lt is false, return +00:00
string get_timezone_str(bool lt) {
  if (lt) {
    time_t t1, t2;
    time(&t1);
    struct tm *tm_info;
    int tz_hour, tz_min;
    char tzstr[16];

    tm_info = localtime(&t1);
    tz_hour = tm_info->tm_hour;
    tz_min = tm_info->tm_min;	

    tm_info = gmtime(&t1);
    tz_hour -= tm_info->tm_hour;
    tz_min -= tm_info->tm_min;

    t2 = mktime(tm_info);

    if(t1-t2 > 0 && tz_hour < 0)
      tz_hour += 24;

    if(tz_min < 0) {
      tz_min += 60;  
      tz_hour--;
    }
    
    if(tz_hour >= 0)
      sprintf(tzstr, "+%02d:%02d", tz_hour, tz_min); 
    else
      sprintf(tzstr, "%02d:%02d", tz_hour, tz_min);

    return string(tzstr);
  }
  else 
    return "+00:00";
}


