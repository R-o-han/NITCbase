#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>
#include <bits/stdc++.h>

using namespace std;
// the declarations for these functions can be found in "BlockBuffer.h"

BlockBuffer::BlockBuffer(int blockNum)
{
  this->blockNum = blockNum;
}

// calls the parent class constructor
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

// load the block header into the argument pointer
int BlockBuffer::getHeader(struct HeadInfo *head)
{
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS)
  {
    return ret;
  }
  // unsigned char buffer[BLOCK_SIZE];
  // Disk::readBlock(buffer,this->blockNum);   not needed as we are using load and get buffer
  // read the block at this.blockNum into the buffer

  // populate the numEntries, numAttrs and numSlots fields in *head
  memcpy(&head->pblock, bufferPtr + 4, 4);
  memcpy(&head->lblock, bufferPtr + 8, 4);
  memcpy(&head->rblock, bufferPtr + 12, 4);
  memcpy(&head->numEntries, bufferPtr + 16, 4);
  memcpy(&head->numAttrs, bufferPtr + 20, 4);
  memcpy(&head->numSlots, bufferPtr + 24, 4);

  return SUCCESS;
}

// load the record at slotNum into the argument pointer
int RecBuffer::getRecord(union Attribute *rec, int slotNum)
{
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS)
  {
    return ret;
  }

  struct HeadInfo head;

  // get the header using this.getHeader() function
  this->getHeader(&head);
  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;
  // read the block at this.blockNum into a buffer
  /* record at slotNum will be at offset HEADER_SIZE + slotMapSize + (recordSize * slotNum)
     - each record will have size attrCount * ATTR_SIZE
     - slotMap will be of size slotCount
  */
  int recordSize = attrCount * ATTR_SIZE;
  unsigned char *slotPointer = bufferPtr + (32 + slotCount + (recordSize * slotNum));

  // load the record into the rec data structure
  memcpy(rec, slotPointer, recordSize);
  return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum)
{
  unsigned char *bufferPtr;
  /* get the starting address of the buffer containing the block
     using loadBlockAndGetBufferPtr(&bufferPtr). */
  int bufferNum = loadBlockAndGetBufferPtr(&bufferPtr);
  if (bufferNum != SUCCESS)
  {
    return bufferNum;
  }
  // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
  // return the value returned by the call.
  HeadInfo head;
  BlockBuffer::getHeader(&head);
  /* get the header of the block using the getHeader() function */
  int attrCount = head.numAttrs;
  // get number of attributes in the block.
  int slotCount = head.numSlots;
  // get the number of slots in the block.
  if (slotNum > slotCount || slotNum < 0)
  {
    return E_OUTOFBOUND;
  }
  // if input slotNum is not in the permitted range return E_OUTOFBOUND.

  /* offset bufferPtr to point to the beginning of the record at required
     slot. the block contains the header, the slotmap, followed by all
     the records. so, for example,
     record at slot x will be at bufferPtr + HEADER_SIZE + (x*recordSize)
     copy the record from `rec` to buffer using memcpy
     (hint: a record will be of size ATTR_SIZE * numAttrs)
  */
  int recordSize = attrCount * ATTR_SIZE;
  unsigned char *slotPointer = bufferPtr + (32 + slotCount + (recordSize * slotNum));
  memcpy(slotPointer, rec, recordSize);
  int ret = StaticBuffer::setDirtyBit(this->blockNum);
  if (ret != SUCCESS)
  {
    cout << "something wrong with the setDirty function\n";
  }
  // update dirty bit using setDirtyBit()

  /* (the above function call should not fail since the block is already
     in buffer and the blockNum is valid. If the call does fail, there
     exists some other issue in the code) */
  return SUCCESS;
  // return SUCCESS
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr)
{
  /* check whether the block is already present in the buffer
       using StaticBuffer.getBufferNum() */
  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
  if (bufferNum != E_BLOCKNOTINBUFFER)
  {
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
      StaticBuffer::metainfo[bufferIndex].timeStamp++;
    }
    StaticBuffer::metainfo[bufferNum].timeStamp = 0;
  }
  else
  {
    bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);
    if (bufferNum == E_OUTOFBOUND)
    {
      return E_OUTOFBOUND;
    }
    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }

  *buffPtr = StaticBuffer::blocks[bufferNum];
  // if present (!=E_BLOCKNOTINBUFFER),
  // set the timestamp of the corresponding buffer to 0 and increment the
  // timestamps of all other occupied buffers in BufferMetaInfo.

  // else
  // get a free buffer using StaticBuffer.getFreeBuffer()

  // if the call returns E_OUTOFBOUND, return E_OUTOFBOUND here as
  // the blockNum is invalid

  // Read the block into the free buffer using readBlock()

  // store the pointer to this buffer (blocks[bufferNum]) in *buffPtr
  return SUCCESS;
  // return SUCCESS;
}

/* used to get the slotmap from a record block
NOTE: this function expects the caller to allocate memory for `*slotMap`
*/
int RecBuffer::getSlotMap(unsigned char *slotMap)
{
  unsigned char *bufferPtr;

  // get the starting address of the buffer containing the block using loadBlockAndGetBufferPtr().
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS)
  {
    return ret;
  }

  struct HeadInfo head;
  RecBuffer::getHeader(&head);
  // get the header of the block using getHeader() function

  int slotCount = head.numSlots;

  // get a pointer to the beginning of the slotmap in memory by offsetting HEADER_SIZE
  unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

  // copy the values from `slotMapInBuffer` to `slotMap` (size is `slotCount`)
  memcpy(slotMap, slotMapInBuffer, slotCount);

  return SUCCESS;
}

int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType)
{

  return attrType == NUMBER ? (attr1.nVal < attr2.nVal ? -1 : (attr1.nVal > attr2.nVal ? 1 : 0)) : strcmp(attr1.sVal, attr2.sVal);
}
