#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <getopt.h>
#include <sys/stat.h> // handle file/directory 
#ifdef _WIN32
  #include <direct.h> // directory creation
#endif

#include "usnjrnl.h"
#include "utils.h"

using namespace std;

// global variables
bool lt = true; // localtime or UTC
bool raw = false; // output all of raw records

#ifdef _WIN32
  char SEP = '\\';
#else // unix
 char SEP = '/';
#endif

void usage(void) {
	printf("USN Analytics (https://www.kazamiya.net/usn_analytics/) v.201801\n\n");
	printf("Usage  : usn_analytics.exe [-ru] -o output input\n\n");
	printf("     -r: parse all of USN_RECORD and write to all.csv with raw style\n");
	printf("     -u: treat a timestamp as UTC (default: Local Time)\n");
	printf(" -o out: specify a output directory\n");
	printf("     in: specify a bunch of data including USN_RECORD\n\n");
}

void process(char *ifname, char *odname) {
    
  UsnJrnl usnjrnl = UsnJrnl(ifname, odname);

  printf("Search USNRECORD");
  usnjrnl.GetAllUsnOffset();
  if (raw == true) {
    usnjrnl.WriteAllRecords(odname, lt);
  } else {
    usnjrnl.PreProcess();
    printf("Check records");
    usnjrnl.CheckRecords();
    printf("Path construction");
    usnjrnl.PostProcess();
    printf("Write records");
    usnjrnl.WriteBundledRecords(odname, lt);
    printf("Check executed trace");
    usnjrnl.WriteExecutedRecords(odname, lt);
    printf("Check opened trace");
    usnjrnl.WriteOpenedRecords(odname, lt);
    printf("Check suspicious trace");
    usnjrnl.WriteSuspiciousInfo();
  }
}

int main(int argc, char **argv) {
    
  char *ifname = NULL;
  char *odname = NULL;
  timer measure_time;

  int opt;
  int longindex;
  
  struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"output", required_argument, NULL, 'o'},
    {"raw", no_argument, NULL, 'r'}, 
    {"utc", no_argument, NULL, 'u'}, 
    {0, 0, 0, 0},
  };

  while((opt = getopt_long(argc, argv, "ho:ru", longopts, &longindex)) != -1) {
    switch(opt) {     
      case 'h':
        usage();
        exit(EXIT_FAILURE);
      case 'o':
        odname = optarg;
        break;
      case 'r':
        raw = true;
        break;
      case 'u':
        lt = false;
        break;
    }
  }
  for (int i = optind; i < argc; i++)
    ifname = argv[i];

  if (!ifname || !odname) {
    usage();
    exit(EXIT_FAILURE);
  }
  
  struct stat st;
  if (is_empty_dir(odname) == false) {
    if (stat(odname, &st) == 0) {
      std::cout << odname << " already exist" << std::endl;
      std::cout << "Please specify a empty directory or new directory" << std::endl;
      exit(EXIT_FAILURE);
    }
    int dresult;
    // try to create directory
#ifdef _WIN32
    dresult = _mkdir(odname);
#else
    dresult = mkdir(odname, 0775);
#endif

    if(dresult != 0) {
      std::cout << "unable to create " << odname << std::endl;
      std::cout << "Please check existence of parent directory and/or permissions" << std::endl;
      exit(EXIT_FAILURE);
    }
  } 

  process(ifname, odname);
  return 0;
}
