#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <bits/stdc++.h>
using namespace std;

int printAttributeCatalog()
{
  // create objects for the relation catalog and attribute catalog
  RecBuffer relCatBuffer(RELCAT_BLOCK);
  RecBuffer attrCatBuffer(ATTRCAT_BLOCK);

  // load the headers of both the blocks into relCatHeader and attrCatHeader.
  HeadInfo relCatHeader;
  HeadInfo attrCatHeader;

  // (we will implement these functions later)
  relCatBuffer.getHeader(&relCatHeader);
  attrCatBuffer.getHeader(&attrCatHeader);

  for (int i = 0, attrCatSlotIndex = 0; i < relCatHeader.numEntries; i++)
  {

    Attribute relCatRecord[RELCAT_NO_ATTRS]; // will store the record from the relation catalog

    relCatBuffer.getRecord(relCatRecord, i);

    printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

    for (int j = 0; j < relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal; j++, attrCatSlotIndex++)
    {
      Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
      attrCatBuffer.getRecord(attrCatRecord, attrCatSlotIndex);
      // declare attrCatRecord and load the attribute catalog entry into it

      if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0)
      {
        const char *attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";
        printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
      }
      if (attrCatSlotIndex == attrCatHeader.numSlots - 1)
      {
        attrCatSlotIndex = -1;
        attrCatBuffer = RecBuffer(attrCatHeader.rblock);
        attrCatBuffer.getHeader(&attrCatHeader);
      }
    }
    printf("\n");
  }

  return 0;
}

void updateAttributeName(const char *relName, const char *oldAttrName, const char *newAttrName)
{
  RecBuffer attrCatBuffer(ATTRCAT_BLOCK);
  HeadInfo attrCatHeader;
  attrCatBuffer.getHeader(&attrCatHeader);

  for (int i = 0; i < attrCatHeader.numEntries; i++)
  {
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBuffer.getRecord(attrCatRecord, i);

    if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName) == 0 && strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldAttrName) == 0)
    {
      strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newAttrName);
      attrCatBuffer.setRecord(attrCatRecord, i);
      cout << "Updated Successfully\n";
      break;
    }
    // reaching at the end of the block, and thus loading
    // the next block and setting the attrCatHeader & recIndex
    if (i == attrCatHeader.numSlots - 1)
    {
      i = -1;
      attrCatBuffer = RecBuffer(attrCatHeader.rblock);
      attrCatBuffer.getHeader(&attrCatHeader);
    }
  }
}

void checkUsedBlock()
{
  unsigned char buffer[BLOCK_SIZE];
  Disk::readBlock(buffer, 0);
  for (int i = 0; i < BLOCK_SIZE; i++)
  {
    switch ((int)buffer[i])
    {
    case 0:
      cout << i << ": Record Block\n";
      break;
    case 1:
      cout << i << ": Internal Index Block\n";
      break;
    case 2:
      cout << i << ": Leaf Index Block\n";
      break;
    case 4:
      cout << i << ": Block Allocation Map\n";
      break;
    }
  }
}
int main(int argc, char *argv[])
{
  /* Initialize the Run Copy of Disk */

  Disk disk_run;
  StaticBuffer buffer;
  OpenRelTable cache;
  // printAttributeCatalog();
  for (int relId = 0; relId <= 2; relId++)
  {
    RelCatEntry relCatBuf;
    RelCacheTable::getRelCatEntry(relId, &relCatBuf);
    printf("Relation: %s\n", relCatBuf.relName);
    for (int attrIndex = 0; attrIndex < relCatBuf.numAttrs; attrIndex++)
    {
      AttrCatEntry attrCatBuf;
      AttrCacheTable::getAttrCatEntry(relId, attrIndex, &attrCatBuf);
      const char *attrType = attrCatBuf.attrType == NUMBER ? "NUM" : "STR";
      printf("  %s: %s\n", attrCatBuf.attrName, attrType);
    }
    printf("\n");
  }

  return 0;
}
