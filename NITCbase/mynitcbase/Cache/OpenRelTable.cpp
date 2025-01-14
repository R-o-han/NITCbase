#include "OpenRelTable.h"
#include <cstring>
#include <stdlib.h>
AttrCacheEntry *createAttrCacheEntryList(int numberOfAttr)
{
    AttrCacheEntry *head = nullptr, *curr = nullptr;
    head = curr = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
    numberOfAttr--;
    while (numberOfAttr--)
    {
        curr->next = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
        curr = curr->next;
    }
    curr->next = nullptr;
    return head;
}
OpenRelTable::OpenRelTable()
{
    // initialize relCache and attrCache with nullpt
    for (int i = 0; i < MAX_OPEN; i++)
    {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
    }

    /************ Setting up Relation Cache entries ************/

    // (we need to populate relation cache with entries for the relation catalog
    //  and attribute catalog.)

    /**** setting up Relation Catalog relation in the Relation Cache Table****/
    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    RelCacheEntry *relCacheEntry = nullptr;
    for (int relId = RELCAT_RELID; relId <= 3; relId++)
    {
        relCatBlock.getRecord(relCatRecord, relId);
        relCacheEntry = (RelCacheEntry *)malloc(sizeof(RelCacheEntry));
        RelCacheTable::recordToRelCatEntry(relCatRecord, &(relCacheEntry->relCatEntry));
        relCacheEntry->recId.block = RELCAT_BLOCK;
        relCacheEntry->recId.slot = relId;
        // allocate this on the heap because we want it to persist outside this function
        RelCacheTable::relCache[relId] = relCacheEntry;
    }

    /************ Setting up Attribute cache entries ************/
    // (we need to populate attribute cache with entries for the relation catalog
    //  and attribute catalog.)

    /**** setting up Relation Catalog relation in the Attribute Cache Table ****/
    RecBuffer attrCatBlock(ATTRCAT_BLOCK);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    AttrCacheEntry *attrCacheEntry = nullptr, *head = nullptr;
    for (int relId = RELCAT_RELID, recordId = 0; relId <= 3; relId++)
    {
        int numberOfAttr = RelCacheTable::relCache[relId]->relCatEntry.numAttrs;
        head = createAttrCacheEntryList(numberOfAttr);
        attrCacheEntry = head;
        while (numberOfAttr--)
        {
            attrCatBlock.getRecord(attrCatRecord, recordId);
            AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
            attrCacheEntry->recId.block = ATTRCAT_BLOCK;
            attrCacheEntry->recId.slot = recordId++;

            attrCacheEntry = attrCacheEntry->next;
        }
        AttrCacheTable::attrCache[relId] = head;
    }
}

/* This function will open a relation having name `relName`.
Since we are currently only working with the relation and attribute catalog, we
will just hardcode it. In subsequent stages, we will loop through all the relations
and open the appropriate one.
*/
int OpenRelTable::getRelId(char relName[ATTR_SIZE])
{
    // if relname is RELCAT_RELNAME, return RELCAT_RELID
    if (strcmp(relName, RELCAT_RELNAME) == 0)
        return RELCAT_RELID;

    // if relname is ATTRCAT_RELNAME, return ATTRCAT_RELID
    if (strcmp(relName, ATTRCAT_RELNAME) == 0)
        return ATTRCAT_RELID;

    if (strcmp(relName, "Students") == 0)
        return 2;
    return E_RELNOTOPEN;
}

OpenRelTable::~OpenRelTable()
{
    // free all the memory that you allocated in the constructor
}