#include <csignal>
#include <cstdlib>
#include <iostream>

extern void setup();
extern void loop();
extern void teardown();

using namespace std;


namespace {
  void finish(int sig) {
    cerr << endl << endl
      << "** Caught signal " << dec << sig << ", exiting." << endl;
    teardown();
    exit(-1);
  }
}

int main() {
  signal(SIGABRT, finish);
  signal(SIGINT, finish);
  signal(SIGTERM, finish);

  setup();
  while (true) loop();
  return 0;
}

