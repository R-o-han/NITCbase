#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <cstring>
#include <bits/stdc++.h>
using namespace std;
int main(int argc, char *argv[])
{
  /* Initialize the Run Copy of Disk */

  Disk disk_run;
  unsigned char buffer[BLOCK_SIZE];
  Disk::readBlock(buffer, 7000);
  char message[] = "hello";
  memcpy(buffer + 20, message, 6);
  // Now, buffer[20] = 'h', buffer[21] = 'e' ...
  Disk::writeBlock(buffer, 7000);
  unsigned char buffer2[BLOCK_SIZE];
  char message3[6];
  Disk::readBlock(buffer2, 7000);
  memcpy(message3, buffer2 + 20, 6);
  cout << message3;
  return 0;
  // StaticBuffer buffer;
  // OpenRelTable cache;

  return FrontendInterface::handleFrontend(argc, argv);
}