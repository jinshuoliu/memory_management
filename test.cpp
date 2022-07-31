#include <new>
#include <iostream>
#include <cassert>
using namespace std;

void noMoreMemory()
{
  cerr << "out of memory";
  abort();
}

int main()
{
  set_new_handler(noMoreMemory);

  long* p = new long[100000000000000];
  assert(p);

  long l = 1000000000000000000000000;
  p = new long[l];
  assert(p);

  system("pause");
  return 0;
}