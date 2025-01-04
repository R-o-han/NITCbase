#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <bits/stdc++.h>
#include <cstring>
using namespace std;

int main(int argc, char *argv[]) {
  /* Initialize the Run Copy of Disk */
  
  Disk disk_run;
  unsigned char buffer[BLOCK_SIZE];
  Disk::readBlock(buffer, 7000);
  char message[] = "hello";
  memcpy(buffer + 20, message, 6);
  //Now, buffer[20] = 'h', buffer[21] = 'e' ...
  Disk::writeBlock(buffer, 7000);
  unsigned char buffer2[BLOCK_SIZE];
  char message2[6];
  Disk::readBlock(buffer2, 7000);
  memcpy(message2, buffer2 + 20, 6);
  cout << message2;
  return 0;
  // StaticBuffer buffer;
  // OpenRelTable cache;
  return FrontendInterface::handleFrontend(argc, argv);
}