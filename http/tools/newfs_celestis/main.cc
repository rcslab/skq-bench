
#include <unistd.h>

#include <iostream>


using namespace std;

void
usage() {
  cout << "newfs_celestis: [OPTIONS] DISKPATH" << endl;
  cout << "Options:" << endl;
  cout << "    -s    Specify disk size in megabytes" << endl;
  cout << "    -h    Display this message" << endl;
}

int
main(int argc, char *argv[]) {
  char ch;
  size_t disksz = 0;

  while ((ch = getopt(argc, argv, "hs:")) != -1) {
    switch (ch) {
      case 's':
        disksz = atoll(optarg) << 20;
        break;
      case 'h':
        usage();
        exit(0);
      default:
        usage();
        exit(1);
    }
  }
  argc -= optind;
  argv += optind;

  if (argc != 1) {
    usage();
    exit(1);
  }

  //filesys_create(argv[0], disksz);
  //filesys_check(argv[0]);
}
