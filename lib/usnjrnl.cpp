#include "usnjrnl.h"
#include "utils.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <map>
#include <iostream> // to_string
#include <string> // to_string

UsnJrnl::~UsnJrnl() {
  fclose(fp_in);
  fclose(fp_ofreport);
}

UsnJrnl::UsnJrnl(char *ifname, char *odname) {
  offset = 0;

  if((fp_in = fopen(ifname, "rb")) == NULL) {
    perror("Input File Error");
    exit(EXIT_FAILURE);
  }
  
  string ofreport;
  ofreport = string(odname) + SEP + "usn_analytics_report.txt";
  fp_ofreport = fopen(ofreport.c_str(), "w");
      
  if(fp_ofreport == NULL ) {
    perror("Output File Error");
    exit(EXIT_FAILURE);
  }

  file_size = get_file_size(ifname);
  printf("%llu bytes (%s)\n", file_size, ifname);  
  fprintf(fp_ofreport, "%llu bytes (%s)\n", file_size, ifname);     
}

// Search and create usn/offset table from input
int UsnJrnl::GetAllUsnOffset() {
  int result;
  uint64_t progress = file_size / 10; 
  offset = 0;  
    
  while(offset <= file_size-sizeof(USN_RECORD_V2)) {
    
    if(offset >= progress) {
      printf(".");
      progress += file_size / 10;
    }

    UsnRecord* ur = new UsnRecord(fp_in);
    result = ur->IsValidRecord(offset);

    if(result == NOT_RECORD) {
      offset += 8;
      delete ur;
      continue;
    } else if(result == V2_RECORD) {
	    usn_set.push_back(ur->usn_record.Usn);
	    usn_table[ur->usn_record.Usn] = offset;
	  } else if(result == CORRUPT_RECORD) {
      corrupt_offset_set.push_back(offset);
    } else if (result == V3_RECORD) {
      printf("USN_RECORD_V3 found at offset %lld, skip\n", offset);
	  } else if (result == V4_RECORD) {
      printf("USN_RECORD_V4 found at offset %lld, skip\n", offset);
	  } 
	  offset += ur->usn_record.RecordLength;
    delete ur;
  }
  printf("Done\n");
  if(usn_set.size() == 0) {
    printf("No USN record found. Check input file is correct.\n");
    exit(EXIT_FAILURE);
  } 
  return 0;
}

// USN Sort & Deduplication
int UsnJrnl::PreProcess() {
  uint64_t usn_num = usn_set.size();

  printf("%8lu corrupt records skipped\n", corrupt_offset_set.size());
  printf("%8llu records found\n", usn_num);
  fprintf(fp_ofreport, "%8lu corrupt records skipped\n", corrupt_offset_set.size());
  fprintf(fp_ofreport, "%8llu records\n", usn_num);   

  // USN Sort & Deduplication
  sort(usn_set.begin(), usn_set.end());
  usn_set.erase(std::unique(usn_set.begin(), usn_set.end()), usn_set.end());
  
  if (usn_num != usn_set.size()) {
    printf("%8llu duplicate records found\n", usn_num - usn_set.size()); 
    printf("%8lu unique records found\n", usn_set.size()); 
    fprintf(fp_ofreport, "%8llu duplicate records\n", usn_num - usn_set.size()); 
    fprintf(fp_ofreport, "%8lu unique records\n", usn_set.size()); 
  }
  
  return 0;
}

// Pack a bunch of records then store usnmain_set
int UsnJrnl::CheckRecords() {
  
  uint64_t i,j;
  uint16_t rec_cnt;
  double time_taken;
  uint64_t progress = usn_set.size() / 20;
  uint64_t rel_ts; // relative timestamp
  vector<uint64_t> skip_set;
  for(i=0; i+1 < usn_set.size(); i++) {
    
    if (i+1 > progress) {
  	  printf(".");
      progress += usn_set.size() / 20;
    }

    // if current record already processed then skip
    if (find(skip_set.begin(), skip_set.end(), i) != skip_set.end()) {
      skip_set.erase(remove(skip_set.begin(), skip_set.end(), i), skip_set.end());
      continue;
    }

    UsnRecord* ur_base = new UsnRecord(fp_in);    
    ur_base->ReadParseRecord(usn_table[usn_set[i]]);
    rec_cnt = 1;
    time_taken = 0;
    
    // DELETE|CLOSE or DELETE|TRANSACT|CLOSE - don't pack
    if (ur_base->usn_record.Reason == (DELETE|CLOSE) || ur_base->usn_record.Reason == (DELETE|TRANSACT|CLOSE)) { 
      UsnMain* um = new UsnMain();
      um->StoreRecord(ur_base, rec_cnt, time_taken);
      usnmain_set.push_back(*um);
      delete um, ur_base;
      continue;
    }

    // if next record already processed then skip
    j=1;
    while(find(skip_set.begin(), skip_set.end(), i+j) != skip_set.end())
      j++;
    
    UsnRecord* ur_next = new UsnRecord(fp_in);    
	  ur_next->ReadParseRecord(usn_table[usn_set[i+j]]);

    // SECURITY -> SECURITY|CLOSE - finish packing
    if ((ur_base->usn_record.Reason == SECURITY) && (ur_next->usn_record.Reason == (SECURITY|CLOSE))) {
      rec_cnt++;
      time_taken += double(ur_next->usn_record.TimeStamp - ur_base->usn_record.TimeStamp) / 10000000; // 1s
      ur_base->usn_record.Reason |= ur_next->usn_record.Reason;
      ur_base->usn_record.FileAttributes |= ur_next->usn_record.FileAttributes;
      UsnMain* um = new UsnMain();
      um->StoreRecord(ur_base, rec_cnt, time_taken);
      usnmain_set.push_back(*um);
      skip_set.push_back(i+j);
      delete um, ur_next, ur_base;
      continue;
    }

    // OLDNAME -> NEWNAME - determine RENAME or MOVE and finish packing
    if ((ur_base->usn_record.Reason & OLDNAME) && (ur_next->usn_record.Reason & NEWNAME)) {
      if (ur_base->file_name == ur_next->file_name) {
        ur_base->usn_record.Reason = MOVE;
        ur_base->file_name += " (" + to_string(ur_base->pid) + " -> " + to_string(ur_next->pid) + ")";
      }
      else {
        ur_base->usn_record.Reason = RENAME;
        ur_base->file_name += " -> " + ur_next->file_name;
      }
      rec_cnt++;
      time_taken += double(ur_next->usn_record.TimeStamp - ur_base->usn_record.TimeStamp) / 10000000;
      ur_base->usn_record.FileAttributes |= ur_next->usn_record.FileAttributes;
      skip_set.push_back(i+j);
      // OLDNAME -> NEWNAME -> NEWNAME|CLOSE - also pack
      rel_ts = ur_next->usn_record.TimeStamp;
      ur_next->ReadParseRecord(usn_table[usn_set[i+j+1]]);
      if (ur_next->usn_record.Reason == (NEWNAME|CLOSE)) {
        ur_base->usn_record.FileAttributes |= ur_next->usn_record.FileAttributes;
        rec_cnt++;
        time_taken += double(ur_next->usn_record.TimeStamp - rel_ts) / 10000000;
        skip_set.push_back(i+j+1);
      }    
      UsnMain* um = new UsnMain();
      um->StoreRecord(ur_base, rec_cnt, time_taken);
      usnmain_set.push_back(*um);
      delete um, ur_next, ur_base;
      continue;
    }
    
    rel_ts = ur_base->usn_record.TimeStamp;
    map<string, uint16_t> filename_vote;
    filename_vote[ur_base->file_name] = 1;
    
    // main pack process
    while(ur_next->usn_record.TimeStamp - rel_ts < 10000000) { // 1s
      if(ur_base->cid == ur_next->cid) { // check if next record is the same id and packable
        // count filename because of garbage exclusion
        if(filename_vote.find(ur_next->file_name) == filename_vote.end())
          filename_vote[ur_next->file_name] = 1;
        else 
          filename_vote[ur_next->file_name]++;
        if (ur_next->usn_record.Reason & (OLDNAME|NEWNAME)) // stop if next operation includes RENAME 
          break;
        rec_cnt++;
        time_taken += double(ur_next->usn_record.TimeStamp - rel_ts) / 10000000;
        ur_base->usn_record.Reason |= ur_next->usn_record.Reason;
        ur_base->usn_record.FileAttributes |= ur_next->usn_record.FileAttributes;
        rel_ts = ur_next->usn_record.TimeStamp;
        skip_set.push_back(i+j);
        if ((ur_next->usn_record.Reason & DELETE) && (ur_next->usn_record.Reason & CLOSE)) // finish if next operation includes DELETE|CLOSE
          break;
      } // skip checking procedure when it doesn't exist relevant record for faster processing
      else if (usnmain_set.size() > 0 && rec_cnt == usnmain_set.back().rec_cnt && ur_base->pid == usnmain_set.back().pid
        && ur_base->usn_record.Reason == usnmain_set.back().reasons_i && ur_base->usn_record.FileAttributes == usnmain_set.back().attrs_i) {
        if (rec_cnt == usnmain_set[usnmain_set.size()-2].rec_cnt && usnmain_set[usnmain_set.size()-2].pid == ur_base->pid
          && ur_base->usn_record.Reason == usnmain_set[usnmain_set.size()-2].reasons_i  && ur_base->usn_record.FileAttributes == usnmain_set[usnmain_set.size()-2].attrs_i) {
          break;
        }
      }
      j++;
      while(find(skip_set.begin(), skip_set.end(), i+j) != skip_set.end())
        j++;
      ur_next->ReadParseRecord(usn_table[usn_set[i+j]]);
    }

    int c=0;
    for(auto x: filename_vote) {
      if(x.second > c) {
        ur_base->file_name = x.first;
        c = x.second;
      }
    }
    // current record is the same pattern as previous record then update last record
    if(usnmain_set.size() > 0 && ur_base->cid == usnmain_set.back().cid && ur_base->pid == usnmain_set.back().pid 
      && ur_base->usn_record.Reason == usnmain_set.back().reasons_i && ur_base->usn_record.FileAttributes == usnmain_set.back().attrs_i) {
      usnmain_set.back().rec_cnt += rec_cnt;
      usnmain_set.back().time_taken = double(ur_base->usn_record.TimeStamp - usnmain_set.back().timestamp_i) / 10000000 + time_taken;
    } else { // not the same then push
      UsnMain* um = new UsnMain();
      um->StoreRecord(ur_base, rec_cnt, time_taken);
      usnmain_set.push_back(*um);
      delete um, ur_next, ur_base;
    }
  }
  // Todo: should process last record even if it's isolated 
  printf("Done\n");
  return 0;
}
  
// Create dir_table/path_table and store file_path value in usnmain_set
int UsnJrnl::GetAllDirName() {

  historical_dir hdir;
  uint32_t pid;
  
  // root directory
  hdir.name = "\\";
  hdir.pid = 0;
  hdir.usn = 0;
  path_table.insert(make_pair(5, hdir));

  // directory table
  for(UsnMain x: usnmain_set) {
    if (x.attrs_i & FOLDER) {
      if (x.file_name.size() == 0)
        continue;
      if (x.file_name.find("\\", x.file_name.size()-1) == string::npos) // filename doesn't end with "\"
        continue;
      if (x.file_name == "<Can't Convert>")
        continue;
      if (x.cid == x.pid) // ignore unusual pattern
        continue;
      hdir.name = x.file_name;
      hdir.pid = x.pid;
      hdir.usn = x.usn;
      dir_table.insert(make_pair(x.cid, hdir));
      // hard coding
      if (hdir.name == "Public") {
        hdir.name = "Users";
        hdir.pid = 5;
        hdir.usn = 0;
        dir_table.insert(make_pair(x.cid, hdir));
      } else if (hdir.name == "Default") {
        hdir.name = "Users";
        hdir.pid = 5;
        hdir.usn = 0;
        dir_table.insert(make_pair(x.cid, hdir));
      } else if (hdir.name == "System32") {
        hdir.name = "Windows";
        hdir.pid = 5;
        hdir.usn = 0;
        dir_table.insert(make_pair(x.cid, hdir));
      } else if (hdir.name == "Prefetch") {
        hdir.name = "Windows";
        hdir.pid = 5;
        hdir.usn = 0;
        dir_table.insert(make_pair(x.cid, hdir));
      }
      //fprintf(fp_ofexecuted, "%lld, %s, %lld, %lld\n", x.cid, hdir.name.c_str(), hdir.pid, hdir.usn);
    }
  }
  uint32_t dir_table_size = dir_table.size();

  uint64_t i=1;
  uint64_t progress = dir_table_size / 10;
    
  // fullpath table
  for(auto x : dir_table) {  // x.first is cid, x.second is hdir
    if (i >= progress) {
  	  printf(".");
      progress += dir_table_size / 10;
    }
    ++i;

    hdir = GetHistoricalFileName(x.second, 0);
    path_table.insert(make_pair(x.first, hdir));
    //fprintf(fp_ofopened, "%lld, %s, %lld, %lld\n", x.first, hdir.name.c_str(), hdir.pid, hdir.usn);
  }

  uint64_t usnmain_set_size = usnmain_set.size();
  progress = usnmain_set_size / 10;
  
  // store into file_path with usnmain_set
  for(i=0; i+1 < usnmain_set_size; i++) {
    if (i >= progress) {
  	  printf(".");
      progress += usnmain_set_size / 10;
    }
    // examine correct path
    size_t count;
    count = path_table.count(usnmain_set[i].pid);
    if (count == 0) {
      usnmain_set[i].file_path = "";
    }
    else if (count == 1) {
      auto itr = path_table.find(usnmain_set[i].pid);   
      usnmain_set[i].file_path = itr->second.name;
    }
    else {
      int64_t diff = INT64_MAX;
      string pname;
      auto itr = path_table.equal_range(usnmain_set[i].pid);
      for (auto iterator = itr.first; iterator != itr.second; iterator++) {
      //  if (usn_record.Usn - iterator->second.usn > 0) {
        if (abs(int64_t(usnmain_set[i].usn - iterator->second.usn)) < diff) {
          diff = usnmain_set[i].usn - iterator->second.usn;
          pname = iterator->second.name;
        }
      }
      usnmain_set[i].file_path = pname;
    }
  }
  
  printf("Done\n");
  return 0;
}

// Search directory structure and return path for creating fullpath table
historical_dir UsnJrnl::GetHistoricalFileName(historical_dir hdir, uint8_t i) {
  uint32_t count;
  
  if(i == 31) // prevent from infinite loop
    return hdir;
  count = dir_table.count(hdir.pid);  
  if (hdir.pid == 5) { // reach root directory
    hdir.name = "\\" + hdir.name;
    return hdir;
  }
  else if (count == 0) { // parent directory not found
    hdir.name = hdir.name;
    return hdir;
  }
  else if (count == 1) { // unique parent directory found
    auto itr = dir_table.find(hdir.pid);    
    hdir.name = itr->second.name + hdir.name;
    hdir.pid = itr->second.pid;
    i++;
    return GetHistoricalFileName(hdir, i);
  } 
  else { // multiple parent directory found
    int64_t diff = INT64_MAX;
    uint32_t pid = 0;
    string pname = "";
    auto itr = dir_table.equal_range(hdir.pid);
    for (auto iterator = itr.first; iterator != itr.second; iterator++) {
//      if (hdir.usn - iterator->second.usn > 0) {
        if (abs(int64_t(hdir.usn - iterator->second.usn)) < diff) {
          diff = hdir.usn - iterator->second.usn;
          pname = iterator->second.name;
          pid = iterator->second.pid;          
        }         
    }
    hdir.name = pname + hdir.name;
    hdir.pid = pid;
    i++;
    return GetHistoricalFileName(hdir, i);
  }  
}

// Path Construction, USN range/Time slot
int UsnJrnl::PostProcess(){
  
  // store usnmain_set file_path value
  GetAllDirName();

  printf("%8lu records after packing\n", usnmain_set.size());       
  fprintf(fp_ofreport, "%8lu records after packing\n", usnmain_set.size()); 
  string timestamp_begin, timestamp_end;
  
  timestamp_begin = usnmain_set[0].timestamp_s;
  timestamp_end = usnmain_set[usnmain_set.size()-1].timestamp_s;
  
  fprintf(fp_ofreport, " Records |                         DateTime                        |             USN              |\n");
  fprintf(fp_ofreport, "%8lu | %s - %s |%13llu - %13llu |\n", usnmain_set.size(), timestamp_begin.c_str(), timestamp_end.c_str(), usnmain_set[0].usn, usnmain_set[usnmain_set.size()-1].usn); 
  fprintf(fp_ofreport, "===================================================================================================\n");

  // USN range, Time slot Breakdown
  uint64_t usn_begin;
  usn_begin = usnmain_set[0].usn;
  int j=0;
  for(int i=1; i <= usnmain_set.size(); ++i) {
    if(usnmain_set[i].usn - usnmain_set[i-1].usn > 1048576) {
      timestamp_end = usnmain_set[i-1].timestamp_s;
      fprintf(fp_ofreport, "%8d |", i-j);
      fprintf(fp_ofreport, " %s - %s |", timestamp_begin.c_str(), timestamp_end.c_str());
      fprintf(fp_ofreport, "%13llu - %13llu |\n", usn_begin, usnmain_set[i-1].usn);
      j = i;
      if(i == usnmain_set.size())
        break;
      usn_begin = usnmain_set[i].usn;
      timestamp_begin = usnmain_set[i].timestamp_s;
    }
  }
  return 0;
}

// for main output header
int UsnJrnl::WriteBundledHeader(FILE *fp, bool lt) {
  string tzstr;
  tzstr = get_timezone_str(lt);

  fprintf(fp, "\"Usn\"\t");
  fprintf(fp, "\"Records\"\t");
  fprintf(fp, "\"TimeStamp(%s)\"\t", tzstr.c_str());
  fprintf(fp, "\"TimeTaken\"\t");
  fprintf(fp, "\"FileName\"\t");
  fprintf(fp, "\"Reason\"\t");
  fprintf(fp, "\"FileAttr\"\t");
  fprintf(fp, "\"FileID\"\t");
  fprintf(fp, "\"ParentID\"\t");
  fprintf(fp, "\"Path\"");
  fprintf(fp, "\n");
  return 0;
}

// Write bundled records 
int UsnJrnl::WriteBundledRecords(char *odname, bool lt) {

  uint64_t usnmain_set_size = usnmain_set.size();
  uint64_t split_size = 1000000;
  uint64_t progress = usnmain_set_size / 10;

  FILE *fp_ofmain;
  string ofmain;
  
  for(uint64_t i=0; i < usnmain_set_size;) {

    ofmain = string(odname) + SEP + "usn_analytics_records-" + parse_datetime_iso8601(usnmain_set[i].timestamp_i, lt) + ".csv";

    if((fp_ofmain = fopen(ofmain.c_str(), "w")) == NULL) {
      perror("Output Records File Error");
      exit(EXIT_FAILURE);
    }
    WriteBundledHeader(fp_ofmain, lt);
    for(uint64_t j=0; i < usnmain_set_size && j < split_size; i++, j++) {    
      usnmain_set[i].WriteBundledRecord(fp_ofmain);
      if (i > progress) {
        printf(".");
        progress += usnmain_set_size / 10;
      }
    }
    fclose(fp_ofmain);

  }
  
  printf("Done\n");
  return 0;
}

// for executed output header
int UsnJrnl::WriteExecutedHeader(FILE *fp, bool lt) {
  string tzstr;
  tzstr = get_timezone_str(lt);

  fprintf(fp, "\"Usn\"\t");
  fprintf(fp, "\"TimeStamp(%s)\"\t", tzstr.c_str());
  fprintf(fp, "\"ExeName\"\t");
  fprintf(fp, "\"ExeCount\"\t");
  fprintf(fp, "\"FileName\"\t");
  fprintf(fp, "\"Reason\"\t");
  fprintf(fp, "\"Records\"\t");
  fprintf(fp, "\"TimeTaken\"\t");
  fprintf(fp, "\"FileID\"");
  fprintf(fp, "\n");
  return 0;
}

// Write Executed records (Prefetch)
int UsnJrnl::WriteExecutedRecords(char *odname, bool lt) {

  vector<UsnExecuted> usnexecuted_set;  

  // create usnexecuted_set
  for(int i=0; i < usnmain_set.size(); i++) { 
    if (usnmain_set[i].file_name.size() >= 16 
      && usnmain_set[i].file_name.find(".pf", usnmain_set[i].file_name.size()-3) != string::npos
      && usnmain_set[i].file_name.rfind("-", usnmain_set[i].file_name.size()-12) != string::npos) {
      if (usnmain_set[i].reasons_i & CREATE || usnmain_set[i].reasons_i & EXTEND) {
        // calculate run count based on prefetch file name
        uint16_t exe_count=1;
        for(int j=0; j < usnexecuted_set.size(); j++) {
          if(usnmain_set[i].file_name == usnexecuted_set[j].file_name)
            exe_count++;
        }
        UsnExecuted* ue = new UsnExecuted();
        ue->StoreRecord(&usnmain_set[i], exe_count);
        usnexecuted_set.push_back(*ue);
        delete ue;
      }
    }
  }

  map<string, uint16_t> exe_name_table; // exe_name, exe_cnt

  // create exe_name table
  for(int i=0; i < usnexecuted_set.size(); i++)
    if(exe_name_table.find(usnexecuted_set[i].exe_name) == exe_name_table.end())
      exe_name_table[usnexecuted_set[i].exe_name] = usnexecuted_set[i].exe_cnt;
    else if(usnexecuted_set[i].exe_cnt > exe_name_table[usnexecuted_set[i].exe_name])
      exe_name_table[usnexecuted_set[i].exe_name] = usnexecuted_set[i].exe_cnt;

  // write to report  
  fprintf(fp_ofreport, "\n[Prefetch Exe Name] %lu exe (name, count)\n", exe_name_table.size());
  for(auto x: exe_name_table)
    fprintf(fp_ofreport, "%s, %d\n", x.first.c_str(), x.second);

  // write to file
  FILE *fp_ofexecuted;
  string ofexecuted;
  ofexecuted = string(odname) + SEP + "usn_analytics_executed.csv";

  if((fp_ofexecuted = fopen(ofexecuted.c_str(), "w")) == NULL) {
    perror("Output Records File Error");
    exit(EXIT_FAILURE);
  } 

  WriteExecutedHeader(fp_ofexecuted, lt);

  for(int i=0; i < usnexecuted_set.size(); i++)
    usnexecuted_set[i].WriteExecutedRecord(fp_ofexecuted);
    
  printf("...Done\n");
  fclose(fp_ofexecuted);
  return 0;
}

// for opened output header
int UsnJrnl::WriteOpenedHeader(FILE *fp, bool lt) {
  string tzstr;
  tzstr = get_timezone_str(lt);

  fprintf(fp, "\"Usn\"\t");
  fprintf(fp, "\"TimeStamp(%s)\"\t", tzstr.c_str());
  fprintf(fp, "\"Path\"\t");
  fprintf(fp, "\"FileName\"\t");
  fprintf(fp, "\"Reason\"\t");
  fprintf(fp, "\"Records\"\t");
  fprintf(fp, "\"TimeTaken\"\t");
  fprintf(fp, "\"FileID\"\t");
  fprintf(fp, "\"ParentID\"");
  fprintf(fp, "\n");
  return 0;
}

// Write Opened records (LNK/ObjectID)
int UsnJrnl::WriteOpenedRecords(char *odname, bool lt) {
  
  vector<UsnOpened> usnopened_set;

  // create usnexecuted_set
  for(int i=0; i < usnmain_set.size(); i++) { 
    if (usnmain_set[i].file_name.size() >= 4 && usnmain_set[i].file_name.find(".lnk", usnmain_set[i].file_name.size()-4) != string::npos) {
      if(usnmain_set[i].reasons_i != (SECURITY|CLOSE) && !(usnmain_set[i].reasons_i & DELETE)) {
        UsnOpened* uo = new UsnOpened();
        uo->StoreRecord(&usnmain_set[i]);
        usnopened_set.push_back(*uo);
        delete uo;
      }
    } else if (usnmain_set[i].reasons_i & (OBJECTID) && !(usnmain_set[i].reasons_i & DELETE)) {
      UsnOpened* uo = new UsnOpened();
      uo->StoreRecord(&usnmain_set[i]);
      usnopened_set.push_back(*uo);
      delete uo;
    }
  }

  map<string, uint16_t> open_name_table; // open_name, open_cnt

  // create open_name table (open_name, open_cnt)
  for(int i=0; i < usnopened_set.size(); i++)
    if(open_name_table.find(usnopened_set[i].file_name) == open_name_table.end())
      open_name_table[usnopened_set[i].file_name] = 1;
    else
      open_name_table[usnopened_set[i].file_name]++;

  // write to report  
  int i=0;
  fprintf(fp_ofreport, "\n[File Open] %lu files (name, count)\n", open_name_table.size());
  for(auto x: open_name_table) {
    fprintf(fp_ofreport, "%s, %d\n", x.first.c_str(), x.second);
    i++;
    if(i > 1024) {
      fprintf(fp_ofreport, "reached 1024 files...skip the rest\n");
      break;
    }
  }
  
  FILE *fp_ofopened;
  string ofopened;
  ofopened = string(odname) + SEP + "usn_analytics_opened.csv";

  if((fp_ofopened = fopen(ofopened.c_str(), "w")) == NULL) {
    perror("Output Records File Error");
    exit(EXIT_FAILURE);
  } 
  
  WriteOpenedHeader(fp_ofopened, lt);
  // write to file
  for(int i=0; i < usnopened_set.size(); i++)
    usnopened_set[i].WriteOpenedRecord(fp_ofopened);

  printf("...Done\n");
  fclose(fp_ofopened);
  return 0;
}

// Write Suspicious Info
int UsnJrnl::WriteSuspiciousInfo() {
  map<string, uint16_t> job_table, exe_table, dll_table, scr_table, ps1_table, vb_table, bat_table, tck_table;
  map<string, string> psexec_table;  // timestamp, filename
  map<string, string> paexec_table;  // timestamp, filename
  //map<string, uint16_t> stream_table, ea_table;
  int i;
  
  for(i=0; i < usnmain_set.size(); i++) {
    if (usnmain_set[i].file_name.size() < 5 )
      continue;
    if (usnmain_set[i].reasons_i == (SECURITY|CLOSE))
      continue;
      
    FindInsertCount(&(usnmain_set[i].file_name), ".job", &job_table);
    FindInsertCount(&(usnmain_set[i].file_name), ".exe", &exe_table);
    FindInsertCount(&(usnmain_set[i].file_name), ".dll", &dll_table);
    FindInsertCount(&(usnmain_set[i].file_name), ".scr", &scr_table);
    FindInsertCount(&(usnmain_set[i].file_name), ".vba", &vb_table);
    FindInsertCount(&(usnmain_set[i].file_name), ".vbe", &vb_table);
    FindInsertCount(&(usnmain_set[i].file_name), ".vbs", &vb_table);
    FindInsertCount(&(usnmain_set[i].file_name), ".ps1", &ps1_table);
    FindInsertCount(&(usnmain_set[i].file_name), ".bat", &bat_table);
    FindInsertCount(&(usnmain_set[i].file_name), ".tck", &tck_table);

#ifdef _WIN32
    if (stricmp(usnmain_set[i].file_name.c_str(), "PSEXESVC.exe") == 0)      
       psexec_table[usnmain_set[i].timestamp_s] = usnmain_set[i].file_name;
    if (strncmp(usnmain_set[i].file_name.c_str(), "PAExec-", 7) == 0)      
       paexec_table[usnmain_set[i].timestamp_s] = usnmain_set[i].file_name;
#else
    if (strcasecmp(usnmain_set[i].file_name.c_str(), "PSEXESVC.exe") == 0)      
       psexec_table[usnmain_set[i].timestamp_s] = usnmain_set[i].file_name;    
    if (strncmp(usnmain_set[i].file_name.c_str(), "PAExec-", 7) == 0)      
       paexec_table[usnmain_set[i].timestamp_s] = usnmain_set[i].file_name;    
#endif
        
  }
  
  WriteFileNameList("job", &job_table);
  WriteFileNameList("exe", &exe_table);
  WriteFileNameList("dll", &dll_table);
  WriteFileNameList("scr", &scr_table);
  WriteFileNameList("ps1", &ps1_table);
  WriteFileNameList("vbe/vbs", &vb_table);
  WriteFileNameList("bat", &bat_table);
  WriteFileNameList("tck", &tck_table);

  fprintf(fp_ofreport, "\n[PSEXESVC] %lu files (timestamp, name)\n", psexec_table.size());
  i=0;
  for(auto x: psexec_table) {
    fprintf(fp_ofreport, "%s, %s\n", x.first.c_str(), x.second.c_str());
    i++;
    if(i > 1024) {
      fprintf(fp_ofreport, "reached 1024 files...skip the rest\n");
      break;
    }      
  }

  fprintf(fp_ofreport, "\n[PAExec-] %lu files (timestamp, name)\n", paexec_table.size());
  i=0;
  for(auto x: paexec_table) {
    fprintf(fp_ofreport, "%s, %s\n", x.first.c_str(), x.second.c_str());
    i++;
    if(i > 1024) {
      fprintf(fp_ofreport, "reached 1024 files...skip the rest\n");
      break;
    }      
  }

  printf("...Done\n");  

  return 0;
}

// for -r option output header
int UsnJrnl::WriteAllHeader(FILE *fp, bool lt) {
  string tzstr;
  tzstr = get_timezone_str(lt);
  
  fprintf(fp, "\"Offset\"\t");
  fprintf(fp, "\"RecLength\"\t");
  fprintf(fp, "\"MajorVer\"\t");
  fprintf(fp, "\"MinorVer\"\t");
  fprintf(fp, "\"FileID\"\t");
  fprintf(fp, "\"FileSeq\"\t");
  fprintf(fp, "\"ParentID\"\t");
  fprintf(fp, "\"ParentSeq\"\t");
  fprintf(fp, "\"Usn\"\t");
  fprintf(fp, "\"TimeStamp(%s)\"\t", tzstr.c_str());
  fprintf(fp, "\"Reason\"\t");
  fprintf(fp, "\"SourceInfo\"\t");
  fprintf(fp, "\"SecurityId\"\t");
  fprintf(fp, "\"FileAttr\"\t");
  fprintf(fp, "\"FileNameLength\"\t");
  fprintf(fp, "\"FileNameOffset\"\t");
  fprintf(fp, "\"FileName\"");
  fprintf(fp, "\n");
  return 0;
}

void UsnJrnl::FindInsertCount(string *name, string ext, map<string, uint16_t> *table) {
  
  uint8_t start = (*name).size()-ext.size();

#ifdef _WIN32
  if (stricmp((*name).substr(start, ext.size()).c_str(), ext.c_str()) == 0) {
#else
  if (strcasecmp((*name).substr(start, ext.size()).c_str(), ext.c_str()) == 0) {  
#endif
//  if((*name).find(ext, (*name).size()-strlen(ext)) != string::npos) {
    if((*table).find(*name) == (*table).end())
      (*table).insert(make_pair((*name), 1));
    else
      (*table)[*(name)]++;
    return;
  }
  return;
}

void UsnJrnl::WriteFileNameList(string name, map<string, uint16_t> *table){
  fprintf(fp_ofreport, "\n[%s] %lu files (name, count)\n", name.c_str(), (*table).size());
  int i=0;
  for(auto x: (*table)) {
    fprintf(fp_ofreport, "%s, %d\n", x.first.c_str(), x.second);
    i++;
    if(i > 1024) {
      fprintf(fp_ofreport, "reached 1024 files...skip the rest\n");
      break;
    }      
  }
  return;
}

// Write header and all records if -r option is enabled
int UsnJrnl::WriteAllRecords(char *odname, bool lt) {

  FILE *fp_ofraw;
  string ofraw;
  ofraw = string(odname) + SEP + "usn_parse_all.csv";

  if((fp_ofraw = fopen(ofraw.c_str(), "w")) == NULL) {
    perror("Output All Raw File Error");
    exit(EXIT_FAILURE);
  } 

  uint64_t usn_num = usn_set.size();
  printf("%8lu corrupt records skipped\n", corrupt_offset_set.size());
  printf("%8llu records found\n", usn_num);
  fprintf(fp_ofreport, "%8lu corrupt records skipped\n", corrupt_offset_set.size());
  fprintf(fp_ofreport, "%8llu records\n", usn_num);   

  // USN Sort & Deduplication
  sort(usn_set.begin(), usn_set.end());
  usn_set.erase(std::unique(usn_set.begin(), usn_set.end()), usn_set.end());
  if (usn_num != usn_set.size()) {
    printf("%8llu duplicate records found\n", usn_num - usn_set.size()); 
    printf("%8lu unique records found\n", usn_set.size()); 
    fprintf(fp_ofreport, "%8llu duplicate records\n", usn_num - usn_set.size()); 
    fprintf(fp_ofreport, "%8lu unique records\n", usn_set.size()); 
  }

  WriteAllHeader(fp_ofraw, lt);
  printf("Write all records");

  uint64_t usn_set_size = usn_set.size();
  uint64_t i=1;
  uint64_t progress = usn_set_size / 10;

  UsnRecord* ur = 0;
  ur = new UsnRecord(fp_in);

  for(uint64_t x: usn_set) {
    ur->ReadParseWriteRecord(fp_ofraw, usn_table[x]);
    if (i >= progress) {
  	  printf(".");
      progress += usn_set_size / 10;
    }
    ++i;
  }
  
  printf("Done\n");
  delete ur;
  fclose(fp_ofraw);
  return 0;
}

