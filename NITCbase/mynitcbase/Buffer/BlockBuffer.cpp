#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>
#include <bits/stdc++.h>

using namespace std;
// the declarations for these functions can be found in "BlockBuffer.h"

BlockBuffer::BlockBuffer(char blockType)
{
  // allocate a block on the disk and a buffer in memory to hold the new block of
  // given type using getFreeBlock function and get the return error codes if any.
  int blocktype = blockType == 'R' ? REC : UNUSED_BLK;
  int blockNum = getFreeBlock(blocktype);
  if (blockNum < 0 || blockNum >= DISK_BLOCKS)
  {
    cout << "ERROR: Block not available\n";
    this->blockNum = blockNum;
    return;
  }
  // set the blockNum field of the object to that of the allocated block
  // number if the method returned a valid block number,
  // otherwise set the error code returned as the block number.
  this->blockNum = blockNum;
  // (The caller must check if the constructor allocatted block successfully
  // by checking the value of block number field.)
}

BlockBuffer::BlockBuffer(int blockNum)
{
  this->blockNum = blockNum;
}

RecBuffer::RecBuffer() : BlockBuffer('R') {}
// call parent non-default constructor with 'R' denoting record block.

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
  int bufferNum = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
  if (bufferNum != SUCCESS)
  {
    return bufferNum;
  }
  // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
  // return the value returned by the call.
  HeadInfo head;
  int ret = BlockBuffer::getHeader(&head);
  if (ret != SUCCESS)
    return ret;
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
  ret = StaticBuffer::setDirtyBit(this->blockNum);
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
  RecBuffer recordBlock(this->blockNum);
  recordBlock.getHeader(&head);
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

int BlockBuffer::setHeader(struct HeadInfo *head)
{

  unsigned char *bufferPtr;
  // get the starting address of the buffer containing the block using
  // loadBlockAndGetBufferPtr(&bufferPtr).
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
  // return the value returned by the call.
  if (ret != SUCCESS)
  {
    return ret;
  }
  // cast bufferPtr to type HeadInfo*
  struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

  // copy the fields of the HeadInfo pointed to by head (except reserved) to
  // the header of the block (pointed to by bufferHeader)
  //(hint: bufferHeader->numSlots = head->numSlots )
  bufferHeader->blockType = head->blockType;
  bufferHeader->pblock = head->pblock;
  bufferHeader->lblock = head->lblock;
  bufferHeader->rblock = head->rblock;
  bufferHeader->numEntries = head->numEntries;
  bufferHeader->numAttrs = head->numAttrs;
  bufferHeader->numSlots = head->numSlots;
  // update dirty bit by calling StaticBuffer::setDirtyBit()
  // if setDirtyBit() failed, return the error code
  ret = StaticBuffer::setDirtyBit(this->blockNum);
  return ret;
  // return SUCCESS;
}

int BlockBuffer::setBlockType(int blockType)
{

  unsigned char *bufferPtr;
  /* get the starting address of the buffer containing the block
     using loadBlockAndGetBufferPtr(&bufferPtr). */
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS)
  {
    return ret;
  }
  // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
  // return the value returned by the call.

  // store the input block type in the first 4 bytes of the buffer.
  // (hint: cast bufferPtr to int32_t* and then assign it)
  // *((int32_t *)bufferPtr) = blockType;
  *((int32_t *)bufferPtr) = blockType;
  // update the StaticBuffer::blockAllocMap entry corresponding to the
  // object's block number to `blockType`.
  StaticBuffer::blockAllocMap[this->blockNum] = blockType;
  // update dirty bit by calling StaticBuffer::setDirtyBit()
  // if setDirtyBit() failed
  return StaticBuffer::setDirtyBit(this->blockNum);
  // return the returned value from the call

  // return SUCCESS
}

int BlockBuffer::getFreeBlock(int blockType)
{

  // iterate through the StaticBuffer::blockAllocMap and find the block number
  // of a free block in the disk.
  int blockNum;
  for (blockNum = 0; blockNum < DISK_BLOCKS; blockNum++)
  {
    if (StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK)
    {
      break;
    }
  }
  // if no block is free, return E_DISKFULL.
  if (blockNum == DISK_BLOCKS)
  {
    return E_DISKFULL;
  }
  // set the object's blockNum to the block number of the free block.
  this->blockNum = blockNum;
  // find a free buffer using StaticBuffer::getFreeBuffer() .
  int ret = StaticBuffer::getFreeBuffer(this->blockNum);
  if (ret < 0 || ret >= BUFFER_CAPACITY)
  {
    printf("BUFFER FULL\n");
    return ret;
  }
  struct HeadInfo head;
  head.pblock = -1;
  head.lblock = -1;
  head.rblock = -1;
  head.numEntries = 0;
  head.numAttrs = 0;
  head.numSlots = 0;
  setHeader(&head);
  // initialize the header of the block passing a struct HeadInfo with values
  // pblock: -1, lblock: -1, rblock: -1, numEntries: 0, numAttrs: 0, numSlots: 0
  // to the setHeader() function.

  // update the block type of the block to the input block type using setBlockType().
  setBlockType(blockType);
  return blockNum;
  // return block number of the free block.
}

int RecBuffer::setSlotMap(unsigned char *slotMap)
{
  unsigned char *bufferPtr;
  /* get the starting address of the buffer containing the block using
     loadBlockAndGetBufferPtr(&bufferPtr). */
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
  // return the value returned by the call.
  if (ret != SUCCESS)
  {
    return ret;
  }
  // get the header of the block using the getHeader() function
  HeadInfo head;
  getHeader(&head);
  int numSlots = head.numSlots;

  // the slotmap starts at bufferPtr + HEADER_SIZE. Copy the contents of the
  // argument `slotMap` to the buffer replacing the existing slotmap.
  // Note that size of slotmap is `numSlots`
  unsigned char *slotPointer = bufferPtr + HEADER_SIZE;
  memcpy(slotPointer, slotMap, numSlots);
  return StaticBuffer::setDirtyBit(this->blockNum);
  // update dirty bit using StaticBuffer::setDirtyBit
  // if setDirtyBit failed, return the value returned by the call

  // return SUCCESS
}

int BlockBuffer::getBlockNum()
{
  return this->blockNum;
  // return corresponding block number.
}