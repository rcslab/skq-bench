#include <unistd.h>

#include <iostream>

using namespace std;

void
usage() {
  cout << "fsck_celestis: [OPTIONS] DISKPATH" << endl;
  cout << "Options:" << endl;
  cout << "    -h    Display this message" << endl;
}

int
main(int argc, char *argv[]) {
  char ch;

  while ((ch = getopt(argc, argv, "h")) != -1) {
    switch (ch) {
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

  //filesys_check(argv[0]);
}
