#include "BPlusTree.h"
#include <bits/stdc++.h>
#include <cstring>

static int comparisions = 0;

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op)
{
    // declare searchIndex which will be used to store search index for attrName.
    IndexId searchIndex;
    /* get the search index corresponding to attribute with name attrName
           using AttrCacheTable::getSearchIndex(). */
    AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);
    /* load the attribute cache entry into attrCatEntry using
         AttrCacheTable::getAttrCatEntry(). */
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    int type = attrCatEntry.attrType;
    // declare variables block and index which will be used during search
    int block, index;
    /* searchIndex == {-1, -1}*/

    if (searchIndex.block == -1 && searchIndex.index == -1)
    {
        // (search is done for the first time)

        // start the search from the first entry of root.
        block = attrCatEntry.rootBlock;
        index = 0;
        /* attrName doesn't have a B+ tree (block == -1)*/
        if (block == -1)
        {
            return RecId{-1, -1};
        }
    }
    else
    {
        /*a valid searchIndex points to an entry in the leaf index of the attribute's
       B+ Tree which had previously satisfied the op for the given attrVal.*/
        block = searchIndex.block;
        index = searchIndex.index + 1;

        // load block into leaf using IndLeaf::IndLeaf().
        IndLeaf leaf(block);

        // declare leafHead which will be used to hold the header of leaf.
        HeadInfo leafHead;
        // load header into leafHead using BlockBuffer::getHeader().

        leaf.getHeader(&leafHead);
        if (index >= leafHead.numEntries)
        {
            /* (all the entries in the block has been searched; search from the
            beginning of the next leaf index block. */

            // update block to rblock of current block and index to 0.
            block = leafHead.rblock;
            index = 0;
            if (block == -1)
            {
                // (end of linked list reached - the search is done.)
                return RecId{-1, -1};
            }
        }
    }

    /******  Traverse through all the internal nodes according to value
           of attrVal and the operator op                             ******/

    /* (This section is only needed when
        - search restarts from the root block (when searchIndex is reset by caller)
        - root is not a leaf
        If there was a valid search index, then we are already at a leaf block
        and the test condition in the following loop will fail)
    */
    /* block is of type IND_INTERNAL */
    while (StaticBuffer::getStaticBlockType(block) == IND_INTERNAL)
    { // load the block into internalBlk using IndInternal::IndInternal().
        IndInternal internalBlk(block);
        HeadInfo intHead;
        // load the header of internalBlk into intHead using BlockBuffer::getHeader()
        internalBlk.getHeader(&intHead);

        // declare intEntry which will be used to store an entry of internalBlk.
        InternalEntry intEntry;

        if (op == NE || op == LT || op == LE)
        {
            /*
                        - NE: need to search the entire linked list of leaf indices of the B+ Tree,
                        starting from the leftmost leaf index. Thus, always move to the left.

                        - LT and LE: the attribute values are arranged in ascending order in the
                        leaf indices of the B+ Tree. Values that satisfy these conditions, if
                        any exist, will always be found in the left-most leaf index. Thus,
                        always move to the left.
                        */

            // load entry in the first slot of the block into intEntry
            // using IndInternal::getEntry().

            internalBlk.getEntry(&intEntry, 0);
            block = intEntry.lChild;
        }
        else
        {
            /*
            - EQ, GT and GE: move to the left child of the first entry that is
            greater than (or equal to) attrVal
            (we are trying to find the first entry that satisfies the condition.
            since the values are in ascending order we move to the left child which
            might contain more entries that satisfy the condition)
            */

            /*
             traverse through all entries of internalBlk and find an entry that
             satisfies the condition.
             if op == EQ or GE, then intEntry.attrVal >= attrVal
             if op == GT, then intEntry.attrVal > attrVal
             Hint: the helper function compareAttrs() can be used for comparing
            */
            bool found = false;
            int index = 0;
            while (index < intHead.numEntries)
            {
                internalBlk.getEntry(&intEntry, index);
                int cmpVal = compareAttrs(intEntry.attrVal, attrVal, type);
                if ((op == EQ && cmpVal == 0) || (op == GE && cmpVal >= 0) || (op == GT && cmpVal > 0))
                {
                    found = true;
                    break;
                }
                index++;
            }

            if (found)
            {
                // move to the left child of that entry
                block = intEntry.lChild;
            }
            else
            {
                // move to the right child of the last entry of the block
                // i.e numEntries - 1 th entry of the block
                block = intEntry.rChild;
            }
        }
    }

    // NOTE: `block` now has the block number of a leaf index block.

    /******  Identify the first leaf index entry from the current position
                that satisfies our condition (moving right)             ******/
    while (block != -1)
    {
        // load the block into leafBlk using IndLeaf::IndLeaf().
        IndLeaf leafBlk(block);
        HeadInfo leafHead;

        // load the header to leafHead using BlockBuffer::getHeader().
        leafBlk.getHeader(&leafHead);

        // declare leafEntry which will be used to store an entry from leafBlk
        Index leafEntry;

        while (index < leafHead.numEntries)
        {
            // load entry corresponding to block and index into leafEntry
            // using IndLeaf::getEntry().
            leafBlk.getEntry(&leafEntry, index);
            int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, type);
            // ++comparisions;
            // std::cout << comparisions << ") Comparisons\n";
            if (
                (op == EQ && cmpVal == 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == LT && cmpVal < 0) ||
                (op == GT && cmpVal > 0) ||
                (op == GE && cmpVal >= 0) ||
                (op == NE && cmpVal != 0))
            {
                IndexId searchIndex{block, index};

                AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
                // return the recId {leafEntry.block, leafEntry.slot}.
                return RecId{leafEntry.block, leafEntry.slot};
            }
            else if ((op == EQ || op == LE || op == LT) && cmpVal > 0)
            {
                /*future entries will not satisfy EQ, LE, LT since the values
                    are arranged in ascending order in the leaves */

                // return RecId {-1, -1};
                return RecId{-1, -1};
            }
            ++index;
        }
        /*only for NE operation do we have to check the entire linked list;
               for all the other op it is guaranteed that the block being searched
               will have an entry, if it exists, satisying that op. */
        if (op != NE)
            break;

        // block = next block in the linked list, i.e., the rblock in leafHead.
        // update index to 0.

        block = leafHead.rblock;
        index = 0;
    }

    // no entry satisying the op was found; return the recId {-1,-1}
    return RecId{-1, -1};
}

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE])
{

    // if relId is either RELCAT_RELID or ATTRCAT_RELID:
    //     return E_NOTPERMITTED;
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
        return E_NOTPERMITTED;

    // get the attribute catalog entry of attribute `attrName`
    // using AttrCacheTable::getAttrCatEntry()
    // if getAttrCatEntry fails
    //     return the error code from getAttrCatEntry
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS)
    {
        std::cout << "bplus create error" << std::endl;
        return ret;
    }

    /* an index already exists for the attribute (check rootBlock field) */
    if (attrCatEntry.rootBlock != -1)
        return SUCCESS;

    /******Creating a new B+ Tree ******/

    // get a free leaf block using constructor 1 to allocate a new blocks
    IndLeaf rootBlockBuf;

    // (if the block could not be allocated, the appropriate error code
    //  will be stored in the blockNum member field of the object)

    // declare rootBlock to store the blockNumber of the new leaf block
    int rootBlock = rootBlockBuf.getBlockNum();

    // if there is no more disk space for creating an index
    if (rootBlock == E_DISKFULL)
        return E_DISKFULL;

    attrCatEntry.rootBlock = rootBlock;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

    // load the relation catalog entry into relCatEntry
    // using RelCacheTable::getRelCatEntry().
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    /***** Traverse all the blocks in the relation and insert them one
               by one into the B+ Tree *****/
    int block = relCatEntry.firstBlk;
    while (block != -1)
    {
        // declare a RecBuffer object for `block`
        RecBuffer recBuff(block);
        // load the slot map into slotMap using RecBuffer::getSlotMap().
        unsigned char slotMap[relCatEntry.numSlotsPerBlk];
        recBuff.getSlotMap(slotMap);

        // for every occupied slot of the block
        for (int slot = 0; slot < relCatEntry.numSlotsPerBlk; slot++)
        {
            if (slotMap[slot] == SLOT_OCCUPIED)
            {
                // load the record corresponding to the slot into `record`
                // using RecBuffer::getRecord().
                Attribute record[relCatEntry.numAttrs];
                recBuff.getRecord(record, slot);

                // declare recId and store the rec-id of this record in it
                // RecId recId{block, slot};
                RecId recId{block, slot};
                // insert the attribute value corresponding to attrName from the record
                // into the B+ tree using bPlusInsert.
                // (note that bPlusInsert will destroy any existing bplus tree if
                // insert fails i.e when disk is full)
                // retVal = bPlusInsert(relId, attrName, attribute value, recId);
                int ret = BPlusTree::bPlusInsert(relId, attrName, record[attrCatEntry.offset], recId);

                if (ret == E_DISKFULL)
                    return E_DISKFULL;
            }
        }

        // get the header of the block using BlockBuffer::getHeader()
        // set block = rblock of current block (from the header)
        HeadInfo head;
        recBuff.getHeader(&head);
        block = head.rblock;
    }
    return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum)
{
    /*rootBlockNum lies outside the valid range [0,DISK_BLOCKS-1]*/
    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS)
        return E_OUTOFBOUND;

    /* type of block (using StaticBuffer::getStaticBlockType())*/
    int type = StaticBuffer::getStaticBlockType(rootBlockNum);
    if (type == IND_LEAF)
    {
        // declare an instance of IndLeaf for rootBlockNum using appropriate
        // constructor

        // release the block using BlockBuffer::releaseBlock().
        IndLeaf indLeaf(rootBlockNum);
        indLeaf.releaseBlock();
        return SUCCESS;
    }
    else if (type == IND_INTERNAL)
    {
        // declare an instance of IndInternal for rootBlockNum using appropriate
        // constructor
        IndInternal indInternal(rootBlockNum);

        // load the header of the block using BlockBuffer::getHeader().
        HeadInfo head;
        indInternal.getHeader(&head);

        InternalEntry internalEntry;
        indInternal.getEntry(&internalEntry, 0);
        if (internalEntry.lChild != -1)
        {
            int ret = BPlusTree::bPlusDestroy(internalEntry.lChild);
            if (ret != SUCCESS)
                return ret;
        }

        /*iterate through all the entries of the internalBlk and destroy the lChild
        of the first entry and rChild of all entries using BPlusTree::bPlusDestroy().
        (the rchild of an entry is the same as the lchild of the next entry.
         take care not to delete overlapping children more than once ) */

        for (int i; i < head.numEntries; i++)
        {
            indInternal.getEntry(&internalEntry, i);
            int ret = BPlusTree::bPlusDestroy(internalEntry.rChild);
            if (ret != SUCCESS)
                return ret;
        }
        // release the block using BlockBuffer::releaseBlock().
        indInternal.releaseBlock();
        return SUCCESS;
    }
    else
    {
        // (block is not an index block.)
        return E_INVALIDBLOCK;
    }
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId)
{

    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    // if getAttrCatEntry() failed
    //     return the error code
    if (ret != SUCCESS)
    {

        std::cout << "error in bPlusInsert" << std::endl;
        return ret;
    }
    int blockNum = attrCatEntry.rootBlock /* rootBlock of B+ Tree (from attrCatEntry) */;

    if (blockNum == -1 /* there is no index on attribute (rootBlock is -1) */)
    {
        return E_NOINDEX;
    }

    // find the leaf block to which insertion is to be done using the
    // findLeafToInsert() function

    int leafBlkNum = BPlusTree::findLeafToInsert(blockNum, attrVal, attrCatEntry.attrType) /* findLeafToInsert(root block num, attrVal, attribute type) */;
    // std::cout << "bptree:391->leafBlkNum " << leafBlkNum << "\n";
    // insert the attrVal and recId to the leaf block at blockNum using the
    // insertIntoLeaf() function.
    // declare a struct Index with attrVal = attrVal, block = recId.block and
    // slot = recId.slot to pass as argument to the function.
    // insertIntoLeaf(relId, attrName, leafBlkNum, Index entry)
    // NOTE: the insertIntoLeaf() function will propagate the insertion to the
    //       required internal nodes by calling the required helper functions
    //       like insertIntoInternal() or createNewRoot()
    Index index;
    index.attrVal = attrVal;
    index.block = recId.block;
    index.slot = recId.slot;

    ret = BPlusTree::insertIntoLeaf(relId, attrName, leafBlkNum, index);
    if (ret == E_DISKFULL /*insertIntoLeaf() returns E_DISKFULL */)
    {
        // destroy the existing B+ tree by passing the rootBlock to bPlusDestroy().
        BPlusTree::bPlusDestroy(blockNum);
        // update the rootBlock of attribute catalog cache entry to -1 using
        // AttrCacheTable::setAttrCatEntry().
        attrCatEntry.rootBlock = -1;
        AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

        return E_DISKFULL;
    }
    return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType)
{
    int blockNum = rootBlock;

    while (StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF /*block is not of type IND_LEAF */)
    { // use StaticBuffer::getStaticBlockType()

        // declare an IndInternal object for block using appropriate constructor
        IndInternal indInternal(blockNum);
        // get header of the block using BlockBuffer::getHeader()
        HeadInfo head;
        indInternal.getHeader(&head);
        InternalEntry entry;
        /* iterate through all the entries, to find the first entry whose
             attribute value >= value to be inserted.
             NOTE: the helper function compareAttrs() declared in BlockBuffer.h
                   can be used to compare two Attribute values. */
        bool found = false;
        int i;
        for (i = 0; i < head.numEntries; i++)
        {
            indInternal.getEntry(&entry, i);
            if (compareAttrs(entry.attrVal, attrVal, attrType) >= 0)
            {
                found = true;
                break;
            }
        }
        if (!found /*no such entry is found*/)
        {
            InternalEntry entry;
            indInternal.getEntry(&entry, head.numEntries - 1);
            blockNum = entry.rChild;
            // set blockNum = rChild of (nEntries-1)'th entry of the block
            // (i.e. rightmost child of the block)
        }
        else
        {
            InternalEntry entry;
            indInternal.getEntry(&entry, i);
            blockNum = entry.lChild;
            // set blockNum = lChild of the entry that was found
        }
    }
    return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry)
{
    // std::cout << "insert into leaf \n";
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    // declare an IndLeaf instance for the block using appropriate constructor
    IndLeaf indLeaf(blockNum);
    HeadInfo blockHeader;
    // store the header of the leaf index block into blockHeader
    // using BlockBuffer::getHeader()
    indLeaf.getHeader(&blockHeader);
    // the following variable will be used to store a list of index entries with
    // existing indices + the new index to insert
    Index indices[blockHeader.numEntries + 1];

    /*
    Iterate through all the entries in the block and copy them to the array indices.
    Also insert `indexEntry` at appropriate position in the indices array maintaining
    the ascending order.
    - use IndLeaf::getEntry() to get the entry
    - use compareAttrs() declared in BlockBuffer.h to compare two Attribute structs
    */

    int index = 0;
    bool isNotAdded = true;
    for (int i = 0; i < blockHeader.numEntries; i++)
    {
        Index temp;

        indLeaf.getEntry(&temp, i);

        if (compareAttrs(temp.attrVal, indexEntry.attrVal, attrCatEntry.attrType) > 0 && isNotAdded)
        {
            indices[index++] = indexEntry;
            isNotAdded = false;
        }

        indices[index++] = temp;
    }

    if (isNotAdded)
        indices[index] = indexEntry;

    if (blockHeader.numEntries < MAX_KEYS_LEAF)
    {
        // (leaf block has not reached max limit)

        // increment blockHeader.numEntries and update the header of block
        // using BlockBuffer::setHeader().
        blockHeader.numEntries++;
        indLeaf.setHeader(&blockHeader);
        // iterate through all the entries of the array `indices` and populate the
        // entries of block with them using IndLeaf::setEntry().
        for (int i = 0; i < blockHeader.numEntries; i++)
        {
            indLeaf.setEntry(&(indices[i]), i);
        }

        return SUCCESS;
    }

    // If we reached here, the `indices` array has more than entries than can fit
    // in a single leaf index block. Therefore, we will need to split the entries
    // in `indices` between two leaf blocks. We do this using the splitLeaf() function.
    // This function will return the blockNum of the newly allocated block or
    // E_DISKFULL if there are no more blocks to be allocated.

    int newRightBlk = splitLeaf(blockNum, indices);

    // if splitLeaf() returned E_DISKFULL
    //     return E_DISKFULL

    if (newRightBlk == E_DISKFULL)
        return E_DISKFULL;

    /* the current leaf block was not the root */
    if (blockHeader.pblock != -1)
    { // check pblock in header
      // insert the middle value from `indices` into the parent block using the
      // insertIntoInternal() function. (i.e the last value of the left block)

        // the middle value will be at index 31 (given by constant MIDDLE_INDEX_LEAF)

        // create a struct InternalEntry with attrVal = indices[MIDDLE_INDEX_LEAF].attrVal,
        // lChild = currentBlock, rChild = newRightBlk and pass it as argument to
        InternalEntry entry;
        entry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal;
        entry.lChild = blockNum;
        entry.rChild = newRightBlk;

        // the insertIntoInternalFunction as follows

        // insertIntoInternal(relId, attrName, parent of current block, new internal entry)
        int ret = insertIntoInternal(relId, attrName, blockHeader.pblock, entry);

        if (ret == E_DISKFULL)
        {

            std::cout << "Disk full while inserting->not root" << std::endl;

            return E_DISKFULL;
        }
    }
    else
    {
        // the current block was the root block and is now split. a new internal index
        // block needs to be allocated and made the root of the tree.
        // To do this, call the createNewRoot() function with the following arguments

        // createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal,
        //               current block, new right block)
        int ret = createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlk);
        if (ret == E_DISKFULL)
        {

            std::cout << "Disk full while inserting->root" << std::endl;

            return E_DISKFULL;
        }
    }

    // if either of the above calls returned an error (E_DISKFULL), then return that
    // else return SUCCESS
    return SUCCESS;
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[])
{
    // std::cout << "splitting leaf " << leafBlockNum << "\n";
    // declare rightBlk, an instance of IndLeaf using constructor 1 to obtain new
    // leaf index block that will be used as the right block in the splitting

    // declare leftBlk, an instance of IndLeaf using constructor 2 to read from
    // the existing leaf block
    IndLeaf rightBlk;

    IndLeaf leftBlk(leafBlockNum);
    int rightBlkNum = rightBlk.getBlockNum() /* block num of right blk */;
    int leftBlkNum = leftBlk.getBlockNum() /* block num of left blk */;

    if (rightBlkNum == E_DISKFULL)
    { /* newly allocated block has blockNum E_DISKFULL */
        //(failed to obtain a new leaf index block because the disk is full)

        // std::cout << "diskfull in splitleaf" << std::endl;
        return E_DISKFULL;
    }
    HeadInfo leftBlkHeader, rightBlkHeader;
    // get the headers of left block and right block using BlockBuffer::getHeader()
    rightBlk.getHeader(&rightBlkHeader);
    leftBlk.getHeader(&leftBlkHeader);
    // set rightBlkHeader with the following values
    // - number of entries = (MAX_KEYS_LEAF+1)/2 = 32,
    // - pblock = pblock of leftBlk
    // - lblock = leftBlkNum
    // - rblock = rblock of leftBlk
    // and update the header of rightBlk using BlockBuffer::setHeader()
    rightBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    rightBlkHeader.pblock = leftBlkHeader.pblock;
    rightBlkHeader.lblock = leftBlkNum;
    rightBlkHeader.rblock = leftBlkHeader.rblock;
    rightBlkHeader.blockType = leftBlkHeader.blockType;
    rightBlk.setHeader(&rightBlkHeader);
    // set leftBlkHeader with the following values
    // - number of entries = (MAX_KEYS_LEAF+1)/2 = 32
    // - rblock = rightBlkNum
    // and update the header of leftBlk using BlockBuffer::setHeader() */
    leftBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    leftBlkHeader.rblock = rightBlkNum;
    leftBlk.setHeader(&leftBlkHeader);
    // set the first 32 entries of leftBlk = the first 32 entries of indices array

    for (int slot = 0; slot < 32; slot++)
    {
        leftBlk.setEntry(&indices[slot], slot);
        rightBlk.setEntry(&indices[MIDDLE_INDEX_LEAF + 1 + slot], slot);
    }
    // and set the first 32 entries of newRightBlk = the next 32 entries of
    // indices array using IndLeaf::setEntry().

    return rightBlkNum;
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE],
                                  int intBlockNum, InternalEntry intEntry)
{
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatEntryBuffer;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    // declare intBlk, an instance of IndInternal using constructor 2 for the block
    // corresponding to intBlockNum
    IndInternal internalBlock(intBlockNum);

    HeadInfo blockHeader;
    // load blockHeader with header of intBlk using BlockBuffer::getHeader().
    internalBlock.getHeader(&blockHeader);

    // declare internalEntries to store all existing entries + the new entry
    InternalEntry internalEntries[blockHeader.numEntries + 1];

    /*
    Iterate through all the entries in the block and copy them to the array
    `internalEntries`. Insert `indexEntry` at appropriate position in the
    array maintaining the ascending order.
        - use IndInternal::getEntry() to get the entry
        - use compareAttrs() to compare two structs of type Attribute

    Update the lChild of the internalEntry immediately following the newly added
    entry to the rChild of the newly added entry.
    */

    // bool inserted = false;
    int insertedIndex = -1;
    for (int entryindex = 0; entryindex < blockHeader.numEntries; entryindex++)
    {
        InternalEntry internalBlockEntry;
        internalBlock.getEntry(&internalBlockEntry, entryindex);

        if (compareAttrs(internalBlockEntry.attrVal, intEntry.attrVal, attrCatEntryBuffer.attrType) <= 0)
        {
            // the new value is lesser (or equal to), hence does not go here
            internalEntries[entryindex] = internalBlockEntry;
        }
        else
        {
            // // setting the previous entry's rChild to the lChild of this entry
            // if (entryindex > 0)
            // {
            //     internalBlock.getEntry(&internalBlockEntry, entryindex-1);
            //     internalBlockEntry.rChild = intEntry.lChild;
            // }

            // setting the correct entry in `internalEntries`
            internalEntries[entryindex] = intEntry;
            insertedIndex = entryindex;

            // // setting the following entry's lChild to the rChild of this entry
            // internalBlock.getEntry(&internalBlockEntry, entryindex+1);
            // internalBlockEntry.lChild = intEntry.rChild;

            for (entryindex++; entryindex <= blockHeader.numEntries; entryindex++)
            {
                internalBlock.getEntry(&internalBlockEntry, entryindex - 1);
                internalEntries[entryindex] = internalBlockEntry;
            }

            break;
        }
    }

    // inserting the last remaining entry, if any
    //// internalEntries[blockHeader.numEntries] = inserted ? lastEntry : intEntry;
    if (insertedIndex == -1)
    {
        internalEntries[blockHeader.numEntries] = intEntry;
        insertedIndex = blockHeader.numEntries;
    }

    // setting the previous entry's rChild to lChild of `intEntry`
    if (insertedIndex > 0)
    {
        // InternalEntry entry;
        // internalBlock.getEntry(&entry, insertedIndex-1);
        internalEntries[insertedIndex - 1].rChild = intEntry.lChild;
    }

    // setting the following entry's lChild to rChild of `intEntry`
    if (insertedIndex < blockHeader.numEntries)
    {
        // InternalEntry entry;
        // internalBlock.getEntry(&entry, insertedIndex+1);
        internalEntries[insertedIndex + 1].lChild = intEntry.rChild;
    }

    if (blockHeader.numEntries < MAX_KEYS_INTERNAL)
    {
        // (internal index block has not reached max limit)

        // increment blockheader.numEntries and update the header of intBlk
        // using BlockBuffer::setHeader().
        blockHeader.numEntries++;
        internalBlock.setHeader(&blockHeader);

        // iterate through all entries in internalEntries array and populate the
        // entries of intBlk with them using IndInternal::setEntry().
        for (int entryindex = 0; entryindex < blockHeader.numEntries; entryindex++)
            internalBlock.setEntry(&internalEntries[entryindex], entryindex);

        return SUCCESS;
    }

    // If we reached here, the `internalEntries` array has more than entries than
    // can fit in a single internal index block. Therefore, we will need to split
    // the entries in `internalEntries` between two internal index blocks. We do
    // this using the splitInternal() function.
    // This function will return the blockNum of the newly allocated block or
    // E_DISKFULL if there are no more blocks to be allocated.

    int newRightBlk = splitInternal(intBlockNum, internalEntries);

    // if (/* splitInternal() returned E_DISKFULL */)
    if (newRightBlk == E_DISKFULL)
    {
        // Using bPlusDestroy(), destroy the right subtree, rooted at intEntry.rChild.
        // This corresponds to the tree built up till now that has not yet been
        // connected to the existing B+ Tree

        BPlusTree::bPlusDestroy(intEntry.rChild);

        return E_DISKFULL;
    }

    // if the current block was not the root
    if (blockHeader.pblock != -1) // (check pblock in header)
    {
        // insert the middle value from `internalEntries` into the parent block
        // using the insertIntoInternal() function (recursively).

        // create a struct InternalEntry with lChild = current block, rChild = newRightBlk
        // and attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal
        // and pass it as argument to the insertIntoInternalFunction as follows

        InternalEntry middleEntry;
        middleEntry.lChild = intBlockNum, middleEntry.rChild = newRightBlk;

        // the middle value will be at index 50 (given by constant MIDDLE_INDEX_INTERNAL)
        middleEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;

        // insertIntoInternal(relId, attrName, parent of current block, new internal entry)
        return insertIntoInternal(relId, attrName, blockHeader.pblock, middleEntry);
        // // if (ret == E_DISKFULL) return E_DISKFULL;
    }
    else
    {
        // the current block was the root block and is now split. a new internal index
        // block needs to be allocated and made the root of the tree.
        // To do this, call the createNewRoot() function with the following arguments

        return createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal,
                             intBlockNum, newRightBlk);

        // createNewRoot(relId, attrName,
        //               internalEntries[MIDDLE_INDEX_INTERNAL].attrVal,
        //               current block, new right block)

        //// if (ret == E_DISKFULL) return E_DISKFULL;
    }

    // if either of the above calls returned an error (E_DISKFULL), then return that
    // else return SUCCESS
    return SUCCESS;
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[])
{
    // declare rightBlk, an instance of IndInternal using constructor 1 to obtain new
    // internal index block that will be used as the right block in the splitting
    IndInternal rightBlock;

    // declare leftBlk, an instance of IndInternal using constructor 2 to read from
    // the existing internal index block
    IndInternal leftBlock(intBlockNum);

    int leftBlockNum = leftBlock.getBlockNum();
    int rightBlockNum = rightBlock.getBlockNum();

    // if newly allocated block has blockNum E_DISKFULL
    if (rightBlockNum == E_DISKFULL)
    {
        //(failed to obtain a new internal index block because the disk is full)
        return E_DISKFULL;
    }

    HeadInfo leftBlockHeader, rightBlockHeader;

    // get the headers of left block and right block using BlockBuffer::getHeader()
    leftBlock.getHeader(&leftBlockHeader);
    rightBlock.getHeader(&rightBlockHeader);

    // set rightBlkHeader with the following values
    // - number of entries = (MAX_KEYS_INTERNAL)/2 = 50
    // - pblock = pblock of leftBlk
    // and update the header of rightBlk using BlockBuffer::setHeader()

    rightBlockHeader.numEntries = MAX_KEYS_INTERNAL / 2;
    rightBlockHeader.pblock = leftBlockHeader.pblock;

    rightBlock.setHeader(&rightBlockHeader);

    // set leftBlkHeader with the following values
    // - number of entries = (MAX_KEYS_INTERNAL)/2 = 50
    // - rblock = rightBlkNum
    // and update the header using BlockBuffer::setHeader()

    leftBlockHeader.numEntries = MAX_KEYS_INTERNAL / 2;
    leftBlockHeader.rblock = rightBlockNum;

    leftBlock.setHeader(&leftBlockHeader);

    /*
    - set the first 50 entries of leftBlk = index 0 to 49 of internalEntries
      array
    - set the first 50 entries of newRightBlk = entries from index 51 to 100
      of internalEntries array using IndInternal::setEntry().
      (index 50 will be moving to the parent internal index block)
    */

    for (int entryindex = 0; entryindex < MIDDLE_INDEX_INTERNAL; entryindex++)
    {
        leftBlock.setEntry(&internalEntries[entryindex], entryindex);
        rightBlock.setEntry(&internalEntries[entryindex + MIDDLE_INDEX_INTERNAL + 1], entryindex);
    }

    /* block type of a child of any entry of the internalEntries array */;
    //            (use StaticBuffer::getStaticBlockType())
    int type = StaticBuffer::getStaticBlockType(internalEntries[0].lChild);

    // for (/* each child block of the new right block */)
    BlockBuffer blockbuffer(internalEntries[MIDDLE_INDEX_INTERNAL + 1].lChild);

    HeadInfo blockHeader;
    blockbuffer.getHeader(&blockHeader);

    blockHeader.pblock = rightBlockNum;
    blockbuffer.setHeader(&blockHeader);

    for (int entryindex = 0; entryindex < MIDDLE_INDEX_INTERNAL; entryindex++)
    {
        // declare an instance of BlockBuffer to access the child block using
        // constructor 2
        BlockBuffer blockbuffer(internalEntries[entryindex + MIDDLE_INDEX_INTERNAL + 1].rChild);

        // update pblock of the block to rightBlkNum using BlockBuffer::getHeader()
        // and BlockBuffer::setHeader().
        // HeadInfo blockHeader;
        blockbuffer.getHeader(&blockHeader);

        blockHeader.pblock = rightBlockNum;
        blockbuffer.setHeader(&blockHeader);
    }

    return rightBlockNum;
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild)
{
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatEntryBuffer;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    // declare newRootBlk, an instance of IndInternal using appropriate constructor
    // to allocate a new internal index block on the disk
    IndInternal newRootBlock;

    int newRootBlkNum = newRootBlock.getBlockNum();

    if (newRootBlkNum == E_DISKFULL)
    {
        // (failed to obtain an empty internal index block because the disk is full)

        // Using bPlusDestroy(), destroy the right subtree, rooted at rChild.
        // This corresponds to the tree built up till now that has not yet been
        // connected to the existing B+ Tree

        BPlusTree::bPlusDestroy(rChild);

        return E_DISKFULL;
    }

    // update the header of the new block with numEntries = 1 using
    // BlockBuffer::getHeader() and BlockBuffer::setHeader()
    HeadInfo blockHeader;
    newRootBlock.getHeader(&blockHeader);

    blockHeader.numEntries = 1;
    newRootBlock.setHeader(&blockHeader);

    // create a struct InternalEntry with lChild, attrVal and rChild from the
    // arguments and set it as the first entry in newRootBlk using IndInternal::setEntry()
    InternalEntry internalentry;
    internalentry.lChild = lChild, internalentry.rChild = rChild, internalentry.attrVal = attrVal;

    newRootBlock.setEntry(&internalentry, 0);

    // declare BlockBuffer instances for the `lChild` and `rChild` blocks using
    // appropriate constructor and update the pblock of those blocks to `newRootBlkNum`
    // using BlockBuffer::getHeader() and BlockBuffer::setHeader()
    BlockBuffer leftChildBlock(lChild);
    BlockBuffer rightChildBlock(rChild);

    HeadInfo leftChildHeader, rightChildHeader;
    leftChildBlock.getHeader(&leftChildHeader);
    rightChildBlock.getHeader(&rightChildHeader);

    leftChildHeader.pblock = newRootBlkNum;
    rightChildHeader.pblock = newRootBlkNum;

    leftChildBlock.setHeader(&leftChildHeader);
    rightChildBlock.setHeader(&rightChildHeader);

    // update rootBlock = newRootBlkNum for the entry corresponding to `attrName`
    // in the attribute cache using AttrCacheTable::setAttrCatEntry().
    attrCatEntryBuffer.rootBlock = newRootBlkNum;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    return SUCCESS;
}