#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

BlockBuffer::BlockBuffer(int blockNum)
{
  this->blockNum = blockNum;
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

IndBuffer::IndBuffer(int blockNum) : BlockBuffer(blockNum) {}

IndBuffer::IndBuffer(char blockType) : BlockBuffer(blockType) {}

IndLeaf::IndLeaf(int blockNum) : IndBuffer(blockNum) {}

IndInternal::IndInternal(int blockNum) : IndBuffer(blockNum) {}

// call parent non-default constructor with 'R' denoting record block.
RecBuffer::RecBuffer() : BlockBuffer('R') {}

IndInternal::IndInternal() : IndBuffer('I') {}

IndLeaf::IndLeaf() : IndBuffer('L') {} // this is the way to call parent non-default constructor.
                                       // 'L' used to denote IndLeaf.

BlockBuffer::BlockBuffer(char blockType)
{

  // allocate a block on the disk and a buffer in memory to hold the new block of
  // given type using getFreeBlock function and get the return error codes if any.

  // std::cout<<"hello"<<std::endl;

  int type;

  if (blockType == 'R')
    type = REC;
  else if (blockType == 'I')
    type = IND_INTERNAL;
  else if (blockType == 'L')
    type = IND_LEAF;
  else
    type = UNUSED_BLK;

  int blockNum = getFreeBlock(type);

  // set the blockNum field of the object to that of the allocated block
  // number if the method returned a valid block number,
  // otherwise set the error code returned as the block number.

  if (!(blockNum >= 0 && blockNum < DISK_BLOCKS))
  {
    std::cout << "Error(BlockBuffer.cpp:37):Block is Not Available\n";
  }
  this->blockNum = blockNum;

  // (The caller must check if the constructor allocatted block successfully
  // by checking the value of block number field.)
}

int BlockBuffer::getHeader(struct HeadInfo *head)
{

  // details about this in the getRecord function

  unsigned char *bufferPtr;

  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if (ret != SUCCESS)
  {
    std::cout << "Error with getHeader\n";
    return ret;
  }
  // copying all the bufferPtr header data to the struct headInfo

  memcpy(&head->blockType, bufferPtr, 4);
  memcpy(&head->pblock, bufferPtr + 4, 4);
  memcpy(&head->lblock, bufferPtr + 8, 4);
  memcpy(&head->rblock, bufferPtr + 12, 4);
  memcpy(&head->numEntries, bufferPtr + 16, 4);
  memcpy(&head->numAttrs, bufferPtr + 20, 4);
  memcpy(&head->numSlots, bufferPtr + 24, 4);

  return SUCCESS;
}

int BlockBuffer::setBlockType(int blockType)
{

  /* get the starting address of the buffer containing the block
     using loadBlockAndGetBufferPtr(&bufferPtr). */

  unsigned char *bufferPtr;

  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
  // return the value returned by the call.

  if (ret != SUCCESS)
    return ret;

  // store the input block type in the first 4 bytes of the buffer.
  // (hint: cast bufferPtr to int32_t* and then assign it)
  // *((int32_t *)bufferPtr) = blockType;

  *((int32_t *)bufferPtr) = blockType;

  // update the StaticBuffer::blockAllocMap entry corresponding to the
  // object's block number to `blockType`.

  StaticBuffer::blockAllocMap[this->blockNum] = blockType;

  // update dirty bit by calling StaticBuffer::setDirtyBit()
  ret = StaticBuffer::setDirtyBit(this->blockNum);

  // if setDirtyBit() failed
  // return the returned value from the call

  if (ret != SUCCESS)
  {
    std::cout << "setDirty failed\n";
    return ret;
  }

  return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo *head)
{

  unsigned char *bufferPtr;
  // get the starting address of the buffer containing the block using
  // loadBlockAndGetBufferPtr(&bufferPtr).
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if (ret != SUCCESS)
  {
    // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
    // return the value returned by the call.
    return ret;
  }

  // cast bufferPtr to type HeadInfo*
  struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

  // copy the fields of the HeadInfo pointed to by head (except reserved) to
  // the header of the block (pointed to by bufferHeader)
  // (hint: bufferHeader->numSlots = head->numSlots)

  bufferHeader->blockType = head->blockType;
  bufferHeader->pblock = head->pblock;
  bufferHeader->lblock = head->lblock;
  bufferHeader->rblock = head->rblock;
  bufferHeader->numAttrs = head->numAttrs;
  bufferHeader->numEntries = head->numEntries;
  bufferHeader->numSlots = head->numSlots;

  // update dirty bit by calling StaticBuffer::setDirtyBit()
  // if setDirtyBit() failed, return the error code
  ret = StaticBuffer::setDirtyBit(this->blockNum);

  if (ret != SUCCESS)
  {
    return ret;
  }

  return SUCCESS;
}

int BlockBuffer::getFreeBlock(int blockType)
{

  int freeBlk = -1;

  // std::cout<<"1--\n";

  for (int i = 0; i < DISK_BLOCKS; i++)
  {
    if (StaticBuffer::blockAllocMap[i] == UNUSED_BLK)
    {
      freeBlk = i;
      break;
    }
  }

  // std::cout<<"2--\n";

  if (freeBlk == -1)
    return E_DISKFULL;

  this->blockNum = freeBlk;

  int bufferIdx = StaticBuffer::getFreeBuffer(this->blockNum);

  // std::cout<<"3--\n";

  HeadInfo head;

  head.lblock = head.pblock = head.rblock = -1;
  head.numAttrs = head.numEntries = head.numSlots = 0;

  int ret = setHeader(&head);

  // std::cout<<"4--\n";

  if (ret != SUCCESS)
    return ret;

  ret = setBlockType(blockType);

  if (ret != SUCCESS)
    return ret;

  return freeBlk;
}

int RecBuffer::getRecord(union Attribute *rec, int slotNum)
{

  struct HeadInfo head;
  this->getHeader(&head); // first we get the headerInfo of the current block

  int attrCount = head.numAttrs; // from that we get the number of attributes and total numebr of slots
  int slotCount = head.numSlots;
  /*
      here as said in stage 3.....the disk write and read is an async function so it will be slow to remove that we use caching and here its resolved using a 2-D array
      of size 32 x 2048  where 32 is the maximum # block possible at a time
  */
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS)
  {
    return ret;
  }

  int recordSize = attrCount * ATTR_SIZE;
  int offset = HEADER_SIZE + slotCount + (recordSize * slotNum);
  unsigned char *slotPointer = bufferPtr + offset; // we get the pointer by adding the offset (NOTE: slotCount and attrCount will be a constant for relation block and attribute block)

  memcpy(rec, slotPointer, recordSize);

  return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum)
{

  /* get the starting address of the buffer containing the block
     using loadBlockAndGetBufferPtr(&bufferPtr). */

  unsigned char *bufferPtr;

  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if (ret != SUCCESS)
  {
    return ret;
  }

  /* get the header of the block using the getHeader() function */

  struct HeadInfo head;
  this->getHeader(&head);

  int attrCount = head.numAttrs; // get number of attributes in the block.
  int slotCount = head.numSlots; // get the number of slots in the block.

  // if input slotNum is not in the permitted range return E_OUTOFBOUND.

  if (slotNum >= slotCount)
  {
    return E_OUTOFBOUND;
  }

  /* offset bufferPtr to point to the beginning of the record at required
     slot. the block contains the header, the slotmap, followed by all
     the records. so, for example,
     record at slot x will be at bufferPtr + HEADER_SIZE + (x*recordSize)
     copy the record from `rec` to buffer using memcpy
     (hint: a record will be of size ATTR_SIZE * numAttrs)
  */

  int recordSize = attrCount * ATTR_SIZE;
  int offset = HEADER_SIZE + slotCount + recordSize * slotNum;
  unsigned char *slotPointer = bufferPtr + offset;

  // change starts here

  memcpy(slotPointer, rec, recordSize);

  StaticBuffer::setDirtyBit(this->blockNum);

  /* (the above function call should not fail since the block is already
     in buffer and the blockNum is valid. If the call does fail, there
     exists some other issue in the code) */

  return SUCCESS;
}

/* NOTE: This function will NOT check if the block has been initialised as a
   record or an index block. It will copy whatever content is there in that
   disk block to the buffer.
   Also ensure that all the methods accessing and updating the block's data
   should call the loadBlockAndGetBufferPtr() function before the access or
   update is done. This is because the block might not be present in the
   buffer due to LRU buffer replacement. So, it will need to be bought back
   to the buffer before any operations can be done.
 */

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **bufferPtr)
{

  // this is done to check if the block is already in teh buffer(cache) if not we go inside the if else scope
  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

  if (bufferNum == E_BLOCKNOTINBUFFER)
  {

    // here we get a free buffer for the corresponding blockNum
    bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

    // if we get an out of bound blockNum it means we got an error
    if (bufferNum == E_OUTOFBOUND || bufferNum == FAILURE)
    {
      return bufferNum;
    }

    // since it is not in the buffer(cache) we load it into the cache as we can see the buffer we load is it to blocks cache
    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }
  else
  {

    // set the timestamp of the corresponding buffer to 0 and increment the
    // timestamps of all other occupied buffers in BufferMetaInfo.

    for (int idx = 0; idx < BUFFER_CAPACITY; idx++)
    {

      if (!StaticBuffer::metainfo[idx].free)
      {

        if (idx == bufferNum)
          StaticBuffer::metainfo[idx].timeStamp = 0;
        else
          StaticBuffer::metainfo[idx].timeStamp++;
      }
    }
  }

  // after that we return its pointer....it points to the block in the buffer cache(ie blocks[bufferNum])
  *bufferPtr = StaticBuffer::blocks[bufferNum];

  return SUCCESS;
}

/* used to get the slotmap from a record block
NOTE: this function expects the caller to allocate memory for `*slotMap`
*/

int RecBuffer::getSlotMap(unsigned char *slotMap)
{
  unsigned char *bufferPtr;

  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if (ret != SUCCESS)
  {
    return ret;
  }

  struct HeadInfo head;

  RecBuffer recordBlk(this->blockNum);

  recordBlk.getHeader(&head);

  int slotCount = head.numSlots;

  unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

  memcpy(slotMap, slotMapInBuffer, slotCount);

  return SUCCESS;
}

int BlockBuffer::getBlockNum()
{
  return this->blockNum;
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
    return ret;

  // get the header of the block using the getHeader() function

  HeadInfo head;

  getHeader(&head);

  int numSlots = head.numSlots;

  unsigned char *offset = bufferPtr + HEADER_SIZE;

  memcpy(offset, slotMap, numSlots);

  ret = StaticBuffer::setDirtyBit(this->blockNum);

  if (ret != SUCCESS)
    return ret;

  return SUCCESS;
}

int compareAttrs(Attribute attr1, Attribute attr2, int attrType)
{

  double diff;

  if (attrType == STRING)
  {
    diff = strcmp(attr1.sVal, attr2.sVal);
  }
  else
  {
    diff = attr1.nVal - attr2.nVal;
  }

  if (diff > 0)
    return 1;
  else if (diff < 0)
    return -1;
  else
    return 0;
}

void BlockBuffer::releaseBlock()
{

  if (this->blockNum == -1)
    return;

  // else

  /* get the buffer number of the buffer assigned to the block
      using StaticBuffer::getBufferNum().
      (this function return E_BLOCKNOTINBUFFER if the block is not
      currently loaded in the buffer)
  */

  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

  if (bufferNum == E_BLOCKNOTINBUFFER)
    return;

  // if the block is present in the buffer, free the buffer
  // by setting the free flag of its StaticBuffer::tableMetaInfo entry
  // to true.
  StaticBuffer::metainfo[bufferNum].free = true;

  // free the block in disk by setting the data type of the entry
  // corresponding to the block number in StaticBuffer::blockAllocMap
  // to UNUSED_BLK.
  StaticBuffer::blockAllocMap[this->blockNum] = UNUSED_BLK;

  // set the object's blockNum to INVALID_BLOCK (-1)
  this->blockNum = -1;
}

int IndInternal::getEntry(void *ptr, int indexNum)
{
  // if the indexNum is not in the valid range of [0, MAX_KEYS_INTERNAL-1]
  //     return E_OUTOFBOUND.

  if (indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL)
    return E_OUTOFBOUND;

  unsigned char *bufferPtr;
  /* get the starting address of the buffer containing the block
     using loadBlockAndGetBufferPtr(&bufferPtr). */

  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
  //     return the value returned by the call.

  if (ret != SUCCESS)
    return ret;

  // typecast the void pointer to an internal entry pointer
  struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;

  /*
  - copy the entries from the indexNum`th entry to *internalEntry
  - make sure that each field is copied individually as in the following code
  - the lChild and rChild fields of InternalEntry are of type int32_t
  - int32_t is a type of int that is guaranteed to be 4 bytes across every
    C++ implementation. sizeof(int32_t) = 4
  */

  /* the indexNum'th entry will begin at an offset of
     HEADER_SIZE + (indexNum * (sizeof(int) + ATTR_SIZE) )         [why?]
     from bufferPtr */
  unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

  memcpy(&(internalEntry->lChild), entryPtr, sizeof(int32_t));
  memcpy(&(internalEntry->attrVal), entryPtr + 4, sizeof(Attribute));
  memcpy(&(internalEntry->rChild), entryPtr + 20, 4);

  return SUCCESS;
}

int IndLeaf::getEntry(void *ptr, int indexNum)
{

  // if the indexNum is not in the valid range of [0, MAX_KEYS_LEAF-1]
  //     return E_OUTOFBOUND.

  if (indexNum < 0 || indexNum >= MAX_KEYS_LEAF)
    return E_OUTOFBOUND;

  unsigned char *bufferPtr;

  /* get the starting address of the buffer containing the block
     using loadBlockAndGetBufferPtr(&bufferPtr). */

  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
  //     return the value returned by the call.

  if (ret != SUCCESS)
    return ret;

  // copy the indexNum'th Index entry in buffer to memory ptr using memcpy

  /* the indexNum'th entry will begin at an offset of
     HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE)  from bufferPtr */
  unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
  memcpy((struct Index *)ptr, entryPtr, LEAF_ENTRY_SIZE);

  return SUCCESS;
}

int IndLeaf::setEntry(void *ptr, int indexNum)
{

  // if the indexNum is not in the valid range of [0, MAX_KEYS_LEAF-1]
  if (indexNum < 0 || indexNum >= MAX_KEYS_LEAF)
  {
    return E_OUTOFBOUND;
  }

  unsigned char *bufferPtr;
  /* get the starting address of the buffer containing the block
     using loadBlockAndGetBufferPtr(&bufferPtr). */

  // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
  //     return the value returned by the call.

  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if (ret != SUCCESS)
    return ret;

  // copy the Index at ptr to indexNum'th entry in the buffer using memcpy

  /* the indexNum'th entry will begin at an offset of
     HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE)  from bufferPtr */
  unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
  memcpy(entryPtr, (struct Index *)ptr, LEAF_ENTRY_SIZE);

  // update dirty bit using setDirtyBit()
  ret = StaticBuffer::setDirtyBit(this->blockNum);
  // if setDirtyBit failed, return the value returned by the call

  if (ret != SUCCESS)
  {
    std::cout << "Error in Set Entry for leaf node" << std::endl;
    return ret;
  }

  // return SUCCESS
  return SUCCESS;
}

int IndInternal::setEntry(void *ptr, int indexNum)
{

  // if the indexNum is not in the valid range of [0, MAX_KEYS_INTERNAL-1]
  //     return E_OUTOFBOUND.

  if (indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL)
    return E_OUTOFBOUND;

  unsigned char *bufferPtr;
  /* get the starting address of the buffer containing the block
     using loadBlockAndGetBufferPtr(&bufferPtr). */

  // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
  //     return the value returned by the call.

  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if (ret != SUCCESS)
    return ret;

  // typecast the void pointer to an internal entry pointer
  struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;

  /*
  - copy the entries from *internalEntry to the indexNum`th entry
  - make sure that each field is copied individually as in the following code
  - the lChild and rChild fields of InternalEntry are of type int32_t
  - int32_t is a type of int that is guaranteed to be 4 bytes across every
    C++ implementation. sizeof(int32_t) = 4
  */

  /* the indexNum'th entry will begin at an offset of
     HEADER_SIZE + (indexNum * (sizeof(int) + ATTR_SIZE) )         [why?] //beacuse lchild(which is an integer) + attr val(ATTR_SIZE) = 20;
     from bufferPtr */

  unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

  memcpy(entryPtr, &(internalEntry->lChild), 4);
  memcpy(entryPtr + 4, &(internalEntry->attrVal), ATTR_SIZE);
  memcpy(entryPtr + 20, &(internalEntry->rChild), 4);

  // update dirty bit using setDirtyBit()
  // if setDirtyBit failed, return the value returned by the call

  ret = StaticBuffer::setDirtyBit(this->blockNum);

  if (ret != SUCCESS)
  {
    std::cout << "Error in Set Entry for internal node" << std::endl;
    return ret;
  }

  return SUCCESS;
}