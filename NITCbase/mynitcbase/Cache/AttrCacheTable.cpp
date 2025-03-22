#include "AttrCacheTable.h"

#include <cstring>
#include <iostream>

AttrCacheEntry *AttrCacheTable::attrCache[MAX_OPEN];

int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry *attrCatBuf)
{
    if (relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }

    if (attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }
    // here the attrOffset is for getting the particular attribute of teh relation from the linked list
    for (AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next)
    {
        if (entry->attrCatEntry.offset == attrOffset)
        {

            strcpy(attrCatBuf->relName, entry->attrCatEntry.relName);
            strcpy(attrCatBuf->attrName, entry->attrCatEntry.attrName);

            attrCatBuf->attrType = entry->attrCatEntry.attrType;
            attrCatBuf->primaryFlag = entry->attrCatEntry.primaryFlag;
            attrCatBuf->rootBlock = entry->attrCatEntry.rootBlock;
            attrCatBuf->offset = entry->attrCatEntry.offset;

            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

/* Converts a attribute catalog record to AttrCatEntry struct
    We get the record as Attribute[] from the BlockBuffer.getRecord() function.
    This function will convert that to a struct AttrCatEntry type.
*/

void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS], AttrCatEntry *attrCatEntry)
{
    strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);
    strcpy(attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal);
    attrCatEntry->attrType = (int)record[ATTRCAT_ATTR_TYPE_INDEX].nVal;
    attrCatEntry->primaryFlag = (bool)record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;
    attrCatEntry->rootBlock = (int)record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
    attrCatEntry->offset = (int)record[ATTRCAT_OFFSET_INDEX].nVal;
}

/* returns the attribute with name `attrName` for the relation corresponding to relId
NOTE: this function expects the caller to allocate memory for `*attrCatBuf`
*/

int AttrCacheTable::getAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry *attrCatBuf)
{

    if (relId < 0 || relId >= MAX_OPEN)
    {
        return E_OUTOFBOUND;
    }
    if (attrCache[relId] == nullptr)
    {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next)
    {
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0)
        {

            strcpy(attrCatBuf->relName, entry->attrCatEntry.relName);
            strcpy(attrCatBuf->attrName, entry->attrCatEntry.attrName);

            attrCatBuf->attrType = entry->attrCatEntry.attrType;
            attrCatBuf->primaryFlag = entry->attrCatEntry.primaryFlag;
            attrCatBuf->rootBlock = entry->attrCatEntry.rootBlock;
            attrCatBuf->offset = entry->attrCatEntry.offset;

            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::getSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex)
{

    if (relId < 0 or relId >= MAX_OPEN /*relId is outside the range [0, MAX_OPEN-1]*/)
    {
        return E_OUTOFBOUND;
    }

    if (AttrCacheTable::attrCache[relId] == nullptr /*entry corresponding to the relId in the Attribute Cache Table is free*/)
    {
        return E_RELNOTOPEN;
    }

    /* each attribute corresponding to relation with relId */
    for (auto attribute = AttrCacheTable::attrCache[relId]; attribute != nullptr; attribute = attribute->next)
    {
        /* attrName/offset field of the AttrCatEntry
            is equal to the input attrName/attrOffset */
        if (strcmp(attrName, attribute->attrCatEntry.attrName) == 0)
        {
            // copy the searchIndex field of the corresponding Attribute Cache entry
            // in the Attribute Cache Table to input searchIndex variable.
            searchIndex->block = attribute->searchIndex.block;
            searchIndex->index = attribute->searchIndex.index;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::getSearchIndex(int relId, int attrOffset, IndexId *searchIndex)
{

    if (relId < 0 or relId >= MAX_OPEN /*relId is outside the range [0, MAX_OPEN-1]*/)
    {
        return E_OUTOFBOUND;
    }

    if (AttrCacheTable::attrCache[relId] == nullptr /*entry corresponding to the relId in the Attribute Cache Table is free*/)
    {
        return E_RELNOTOPEN;
    }

    for (auto attribute = AttrCacheTable::attrCache[relId]; attribute != nullptr; attribute = attribute->next /* each attribute corresponding to relation with relId */)
    {
        /* attrName/offset field of the AttrCatEntry
            is equal to the input attrName/attrOffset */
        if (attrOffset == attribute->attrCatEntry.offset)
        {
            // copy the searchIndex field of the corresponding Attribute Cache entry
            // in the Attribute Cache Table to input searchIndex variable.
            searchIndex->block = attribute->searchIndex.block;
            searchIndex->index = attribute->searchIndex.index;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex)
{

    if (relId < 0 or relId >= MAX_OPEN /*relId is outside the range [0, MAX_OPEN-1]*/)
    {
        return E_OUTOFBOUND;
    }

    if (AttrCacheTable::attrCache[relId] == nullptr /*entry corresponding to the relId in the Attribute Cache Table is free*/)
    {
        return E_RELNOTOPEN;
    }

    for (auto attribute = AttrCacheTable::attrCache[relId]; attribute != nullptr; attribute = attribute->next /* each attribute corresponding to relation with relId */)
    {
        /* attrName/offset field of the AttrCatEntry
            is equal to the input attrName/attrOffset */
        if (strcmp(attrName, attribute->attrCatEntry.attrName) == 0)
        {
            // copy the input searchIndex variable to the searchIndex field of the
            // corresponding Attribute Cache entry in the Attribute Cache Table.
            attribute->searchIndex.block = searchIndex->block;
            attribute->searchIndex.index = searchIndex->index;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, int attrOffset, IndexId *searchIndex)
{

    if (relId < 0 or relId >= MAX_OPEN /*relId is outside the range [0, MAX_OPEN-1]*/)
    {
        return E_OUTOFBOUND;
    }

    if (AttrCacheTable::attrCache[relId] == nullptr /*entry corresponding to the relId in the Attribute Cache Table is free*/)
    {
        return E_RELNOTOPEN;
    }

    for (auto attribute = AttrCacheTable::attrCache[relId]; attribute != nullptr; attribute = attribute->next /* each attribute corresponding to relation with relId */)
    {
        if (attrOffset == attribute->attrCatEntry.offset /* attrName/offset field of the AttrCatEntry
               is equal to the input attrName/attrOffset */
        )
        {
            // copy the input searchIndex variable to the searchIndex field of the
            // corresponding Attribute Cache entry in the Attribute Cache Table.
            attribute->searchIndex.block = searchIndex->block;
            attribute->searchIndex.index = searchIndex->index;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::resetSearchIndex(int relId, char attrName[ATTR_SIZE])
{

    // declare an IndexId having value {-1, -1}
    // set the search index to {-1, -1} using AttrCacheTable::setSearchIndex
    // return the value returned by setSearchIndex

    IndexId indexId = {-1, -1};
    return AttrCacheTable::setSearchIndex(relId, attrName, &indexId);
}

int AttrCacheTable::resetSearchIndex(int relId, int attrOffset)
{

    // declare an IndexId having value {-1, -1}
    // set the search index to {-1, -1} using AttrCacheTable::setSearchIndex
    // return the value returned by setSearchIndex

    IndexId indexId = {-1, -1};
    return AttrCacheTable::setSearchIndex(relId, attrOffset, &indexId);
}