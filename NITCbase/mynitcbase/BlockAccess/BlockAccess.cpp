#include "BlockAccess.h"
#include <iostream>
#include <cstring>
#include <stdlib.h>
RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op)
{
    // get the previous search index of the relation relId from the relation cache
    // (use RelCacheTable::getSearchIndex() function)
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);
    // let block and slot denote the record id of the record being currently checked
    int block = -1, slot = -1;
    // if the current search index record is invalid(i.e. both block and slot = -1)
    if (prevRecId.block == -1 && prevRecId.slot == -1)
    {
        // (no hits from previous search; search should start from the
        // first record itself)

        // get the first record block of the relation from the relation cache
        // (use RelCacheTable::getRelCatEntry() function of Cache Layer)
        RelCatEntry RelCatBuf;
        RelCacheTable::getRelCatEntry(relId, &RelCatBuf);
        block = RelCatBuf.firstBlk;
        slot = 0;
        // block = first record block of the relation
        // slot = 0
    }
    else
    {
        // (there is a hit from previous search; search should start from
        // the record next to the search index record)
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
        // block = search index's block
        // slot = search index's slot + 1
    }

    /* The following code searches for the next record in the relation
       that satisfies the given condition
       We start from the record id (block, slot) and iterate over the remaining
       records of the relation
    */

    RelCatEntry relCatBuffer;
    RelCacheTable::getRelCatEntry(relId, &relCatBuffer);
    while (block != -1)
    {
        /* create a RecBuffer object for block (use RecBuffer Constructor for
           existing block) */
        RecBuffer Buffer(block);
        HeadInfo header;
        Attribute CatRecord[RELCAT_NO_ATTRS];
        // get the record with id (block, slot) using RecBuffer::getRecord()
        Buffer.getRecord(CatRecord, slot);
        // get header of the block using RecBuffer::getHeader() function
        Buffer.getHeader(&header);
        // get slot map of the block using RecBuffer::getSlotMap() function
        unsigned char *slotMap = (unsigned char *)malloc(sizeof(unsigned char) * header.numSlots);
        Buffer.getSlotMap(slotMap);
        // If slot >= the number of slots per block(i.e. no more slots in this block)
        if (slot >= relCatBuffer.numSlotsPerBlk)
        {
            // update block = right block of block
            block = header.rblock;
            // update slot = 0
            slot = 0;
            continue; // continue to the beginning of this while loop
        }

        // if slot is free skip the loop
        // (i.e. check if slot'th entry in slot map of block contains SLOT_UNOCCUPIED)
        if (slotMap[slot] == SLOT_UNOCCUPIED)
        {
            slot++;
            continue;
            // increment slot and continue to the next record slot
        }

        // compare record's attribute value to the the given attrVal as below:
        /*
            firstly get the attribute offset for the attrName attribute
            from the attribute cache entry of the relation using
            AttrCacheTable::getAttrCatEntry()
        */
        AttrCatEntry attrCatBuf;
        AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuf);
        /* use the attribute offset to get the value of the attribute from
           current record */
        Attribute *record = (Attribute *)malloc(sizeof(Attribute) * header.numAttrs);
        Buffer.getRecord(record, slot);
        int attrOffset = attrCatBuf.offset;
        int cmpVal = compareAttrs(record[attrOffset], attrVal, attrCatBuf.attrType); // will store the difference between the attributes
        // set cmpVal using compareAttrs()

        /* Next task is to check whether this record satisfies the given condition.
           It is determined based on the output of previous comparison and
           the op value received.
           The following code sets the cond variable if the condition is satisfied.
        */
        if (
            (op == NE && cmpVal != 0) || // if op is "not equal to"
            (op == LT && cmpVal < 0) ||  // if op is "less than"
            (op == LE && cmpVal <= 0) || // if op is "less than or equal to"
            (op == EQ && cmpVal == 0) || // if op is "equal to"
            (op == GT && cmpVal > 0) ||  // if op is "greater than"
            (op == GE && cmpVal >= 0)    // if op is "greater than or equal to"
        )
        {
            /*
            set the search index in the relation cache as
            the record id of the record that satisfies the given condition
            (use RelCacheTable::setSearchIndex function)
            */
            RecId newIndex;
            newIndex.block = block;
            newIndex.slot = slot;
            RelCacheTable::setSearchIndex(relId, &newIndex);

            return RecId{block, slot};
        }

        slot++;
    }

    // no record in the relation with Id relid satisfies the given condition
    return RecId{-1, -1};
}

int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE])
{
    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute newRelationName; // set newRelationName with newName
    strcpy(newRelationName.sVal, newName);
    // search the relation catalog for an entry with "RelName" = newRelationName
    RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, newRelationName, EQ);

    // If relation with name newName already exists (result of linearSearch
    //                                               is not {-1, -1})
    //    return E_RELEXIST;
    if (relcatRecId.block != -1 && relcatRecId.slot != -1)
    {
        return E_RELEXIST;
    }

    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute oldRelationName; // set oldRelationName with oldName
    strcpy(oldRelationName.sVal, oldName);
    // search the relation catalog for an entry with "RelName" = oldRelationName
    relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, oldRelationName, EQ);
    // If relation with name oldName does not exist (result of linearSearch is {-1, -1})
    //    return E_RELNOTEXIST;
    if (relcatRecId.block == -1 && relcatRecId.slot == -1)
    {
        return E_RELNOTEXIST;
    }

    /* get the relation catalog record of the relation to rename using a RecBuffer
       on the relation catalog [RELCAT_BLOCK] and RecBuffer.getRecord function
    */
    RecBuffer relcatBuff(relcatRecId.block);
    Attribute relRec[RELCAT_NO_ATTRS];
    relcatBuff.getRecord(relRec, relcatRecId.slot);

    /* update the relation name attribute in the record with newName.
       (use RELCAT_REL_NAME_INDEX) */

    strcpy(relRec[RELCAT_REL_NAME_INDEX].sVal, newName);
    // set back the record value using RecBuffer.setRecord
    relcatBuff.setRecord(relRec, relcatRecId.slot);
    /*
    update all the attribute catalog entries in the attribute catalog corresponding
    to the relation with relation name oldName to the relation name newName
    */
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    /* reset the searchIndex of the attribute catalog using
       RelCacheTable::resetSearchIndex() */
    int numberOfAttributes = relRec[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    for (int i = 0; i < numberOfAttributes; i++)
    {
        RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, oldRelationName, EQ);
        RecBuffer attrcatBuff(attrcatRecId.block);
        Attribute attrRec[ATTRCAT_NO_ATTRS];
        attrcatBuff.getRecord(attrRec, attrcatRecId.slot);
        strcpy(attrRec[ATTRCAT_REL_NAME_INDEX].sVal, newName);
        attrcatBuff.setRecord(attrRec, attrcatRecId.slot);
    }
    // for i = 0 to numberOfAttributes :
    //     linearSearch on the attribute catalog for relName = oldRelationName
    //     get the record using RecBuffer.getRecord
    //
    //     update the relName field in the record to newName
    //     set back the record using RecBuffer.setRecord

    return SUCCESS;
}

int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE])
{

    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute relNameAttr; // set relNameAttr to relName
    strcpy(relNameAttr.sVal, relName);
    // Search for the relation with name relName in relation catalog using linearSearch()
    RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, relNameAttr, EQ);
    // If relation with name relName does not exist (search returns {-1,-1})
    //    return E_RELNOTEXIST;
    if (relcatRecId.block == -1 && relcatRecId.slot == -1)
    {
        return E_RELNOTEXIST;
    }
    /* reset the searchIndex of the attribute catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    /* declare variable attrToRenameRecId used to store the attr-cat recId
    of the attribute to rename */
    RecId attrToRenameRecId{-1, -1};
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    /* iterate over all Attribute Catalog Entry record corresponding to the
       relation to find the required attribute */
    while (true)
    {

        // linear search on the attribute catalog for RelName = relNameAttr
        RecId searchIndex = BlockAccess::linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);
        // if there are no more attributes left to check (linearSearch returned {-1,-1})
        //     break;
        if (searchIndex.block == -1 && searchIndex.slot == -1)
        {
            break;
        }
        /* Get the record from the attribute catalog using RecBuffer.getRecord
          into attrCatEntryRecord */
        RecBuffer attrcatBuff(searchIndex.block);
        attrcatBuff.getRecord(attrCatEntryRecord, searchIndex.slot);
        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0)
        {
            attrToRenameRecId = searchIndex;
            break;
        }
        // if attrCatEntryRecord.attrName = oldName
        //     attrToRenameRecId = block and slot of this record
        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0)
        {
            return E_ATTREXIST;
        }
        // if attrCatEntryRecord.attrName = newName
        //     return E_ATTREXIST;
    }

    // if attrToRenameRecId == {-1, -1}
    //     return E_ATTRNOTEXIST;

    if (attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1)
    {
        return E_ATTRNOTEXIST;
    }
    // Update the entry corresponding to the attribute in the Attribute Catalog Relation.
    /*   declare a RecBuffer for attrToRenameRecId.block and get the record at
         attrToRenameRecId.slot */
    RecBuffer attrcatBuff(attrToRenameRecId.block);
    attrcatBuff.getRecord(attrCatEntryRecord, attrToRenameRecId.slot);
    //   update the AttrName of the record with newName
    strcpy(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);
    //   set back the record with RecBuffer.setRecord
    attrcatBuff.setRecord(attrCatEntryRecord, attrToRenameRecId.slot);
    return SUCCESS;
}

int BlockAccess::insert(int relId, Attribute *record)
{
    // get the relation catalog entry from relation cache
    // ( use RelCacheTable::getRelCatEntry() of Cache Layer)
    RelCatEntry relCatBuff;
    RelCacheTable::getRelCatEntry(relId, &relCatBuff);
    int blockNum = relCatBuff.firstBlk; /* first record block of the relation (from the rel-cat entry)*/

    // rec_id will be used to store where the new record will be inserted
    RecId rec_id = {-1, -1};

    int numOfSlots = relCatBuff.numSlotsPerBlk; /* number of slots per record block */
    int numOfAttributes = relCatBuff.numAttrs;  /* number of attributes of the relation */

    int prevBlockNum = -1; /* block number of the last element in the linked list = -1 */

    /*
        Traversing the linked list of existing record blocks of the relation
        until a free slot is found OR
        until the end of the list is reached
    */
    while (blockNum != -1)
    {
        // create a RecBuffer object for blockNum (using appropriate constructor!)
        RecBuffer recbuff(blockNum);
        // get header of block(blockNum) using RecBuffer::getHeader() function
        HeadInfo head;
        recbuff.getHeader(&head);
        // get slot map of block(blockNum) using RecBuffer::getSlotMap() function
        unsigned char *slotMap = (unsigned char *)malloc(sizeof(unsigned char) * head.numSlots);
        recbuff.getSlotMap(slotMap);
        // search for free slot in the block 'blockNum' and store it's rec-id in rec_id
        // (Free slot can be found by iterating over the slot map of the block)
        /* slot map stores SLOT_UNOCCUPIED if slot is free and
           SLOT_OCCUPIED if slot is occupied) */

        /* if a free slot is found, set rec_id and discontinue the traversal
           of the linked list of record blocks (break from the loop) */
        for (int i = 0; i < head.numSlots; i++)
        {
            if (slotMap[i] == SLOT_UNOCCUPIED)
            {
                rec_id.block = blockNum;
                rec_id.slot = i;
                break;
            }
        }
        if (rec_id.block != -1 && rec_id.slot != -1)
        {
            break;
        }
        /* otherwise, continue to check the next block by updating the
           block numbers as follows:
              update prevBlockNum = blockNum
              update blockNum = header.rblock (next element in the linked
                                               list of record blocks)
        */
        prevBlockNum = blockNum;
        blockNum = head.rblock;
    }

    //  if no free slot is found in existing record blocks (rec_id = {-1, -1})
    if (rec_id.block == -1 && rec_id.slot == -1)
    {
        // if relation is RELCAT, do not allocate any more blocks
        //     return E_MAXRELATIONS;
        if (relId == RELCAT_RELID)
        {
            return E_MAXRELATIONS;
        }
        RecBuffer recBuff;
        blockNum = recBuff.getBlockNum();
        // Otherwise,
        // get a new record block (using the appropriate RecBuffer constructor!)
        // get the block number of the newly allocated block
        // (use BlockBuffer::getBlockNum() function)
        // let ret be the return value of getBlockNum() function call
        if (blockNum == E_DISKFULL)
        {
            return E_DISKFULL;
        }

        // Assign rec_id.block = new block number(i.e. ret) and rec_id.slot = 0
        rec_id.block = blockNum;
        rec_id.slot = 0;

        HeadInfo head;
        head.blockType = REC;
        head.pblock = head.rblock = -1;
        head.lblock = -1;
        head.numEntries = 0;
        head.numAttrs = relCatBuff.numAttrs;
        head.numSlots = relCatBuff.numSlotsPerBlk;
        recBuff.setHeader(&head);
        /*
            set the header of the new record block such that it links with
            existing record blocks of the relation
            set the block's header as follows:
            blockType: REC, pblock: -1
            lblock
                  = -1 (if linked list of existing record blocks was empty
                         i.e this is the first insertion into the relation)
                  = prevBlockNum (otherwise),
            rblock: -1, numEntries: 0,
            numSlots: numOfSlots, numAttrs: numOfAttributes
            (use BlockBuffer::setHeader() function)
        */
        unsigned char *slotMap = (unsigned char *)malloc(sizeof(unsigned char) * relCatBuff.numSlotsPerBlk);
        for (int i = 0; i < relCatBuff.numSlotsPerBlk; i++)
        {
            slotMap[i] = SLOT_UNOCCUPIED;
        }
        recBuff.setSlotMap(slotMap);
        /*
            set block's slot map with all slots marked as free
            (i.e. store SLOT_UNOCCUPIED for all the entries)
            (use RecBuffer::setSlotMap() function)
        */

        // if prevBlockNum != -1
        if (prevBlockNum != -1)
        {
            RecBuffer recBuff(prevBlockNum);
            HeadInfo head;
            recBuff.getHeader(&head);
            head.rblock = blockNum;
            recBuff.setHeader(&head);
            // create a RecBuffer object for prevBlockNum
            // get the header of the block prevBlockNum and
            // update the rblock field of the header to the new block
            // number i.e. rec_id.block
            // (use BlockBuffer::setHeader() function)
        }
        // else
        else
        {
            relCatBuff.firstBlk = rec_id.block;
            RelCacheTable::setRelCatEntry(relId, &relCatBuff);
            // update first block field in the relation catalog entry to the
            // new block (using RelCacheTable::setRelCatEntry() function)
        }
        relCatBuff.lastBlk = rec_id.block;
        RelCacheTable::setRelCatEntry(relId, &relCatBuff);
        // update last block field in the relation catalog entry to the
        // new block (using RelCacheTable::setRelCatEntry() function)
    }
    RecBuffer recBuff(rec_id.block);
    int ret = recBuff.setRecord(record, rec_id.slot);
    if (ret != SUCCESS)
    {
        printf("Record not saved\n");
        return 0;
    }
    // create a RecBuffer object for rec_id.block
    // insert the record into rec_id'th slot using RecBuffer.setRecord())
    unsigned char *slotMap = (unsigned char *)malloc(sizeof(unsigned char) * relCatBuff.numSlotsPerBlk);
    recBuff.getSlotMap(slotMap);
    slotMap[rec_id.slot] = SLOT_OCCUPIED;
    recBuff.setSlotMap(slotMap);
    /* update the slot map of the block by marking entry of the slot to
       which record was inserted as occupied) */
    // (ie store SLOT_OCCUPIED in free_slot'th entry of slot map)
    // (use RecBuffer::getSlotMap() and RecBuffer::setSlotMap() functions)
    HeadInfo head;
    recBuff.getHeader(&head);
    head.numEntries = head.numEntries + 1;
    recBuff.setHeader(&head);
    // increment the numEntries field in the header of the block to
    // which record was inserted
    // (use BlockBuffer::getHeader() and BlockBuffer::setHeader() functions)
    relCatBuff.numRecs++;
    RelCacheTable::setRelCatEntry(relId, &relCatBuff);
    // Increment the number of records field in the relation cache entry for
    // the relation. (use RelCacheTable::setRelCatEntry function)

    return SUCCESS;
}