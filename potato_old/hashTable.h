#ifndef __PO_HASHTABLE_H__
#define __PO_HASHTABLE_H__

#include "configure.h"

// hash table array starting at data
// 1. First uses hkey to get array index
// 2. Then starting at that index finds element with key ekey in the array
// 3. Then if hkey != ekey sets key in the array to hkey
// 4. Returns pointer to that found array element
//
// the first piece of each element is the int key
/*
 * This is all the code we use to use a hash table.  It's just more lines
 * of code if we make it more formal and make a struct HashTable and make
 * the internals opaque.  Since this is inline and does not add additional
 * dereferencing.  This will be faster and use less system resources
 * than just about any other way.  This one function with one or two
 * additional lines of code can do anything for a fixed size hash table.
 */
static inline
void *poHT_findInt(void *data, size_t elementSize, int hkey,
        int ekey, uint32_t tableLen)
{
    DASSERT(hkey >= 0);
    DASSERT(ekey >= -1);

    uint32_t i;
    i = (poMurmurHash(&hkey, sizeof(hkey), 0/*seed*/)%tableLen)*
        elementSize;
    uint8_t *ptr;
    ptr = data;

#ifdef PO_DEBUG
    int lap = 0;
#endif

    while(*((int *)&ptr[i]) != ekey)
    {
        i += elementSize;
        if(i == tableLen*elementSize)
        {
            DVASSERT(++lap < 2, "can't find key(fd)="
                    "%d in full hash table\n", ekey);
            i = 0;
        }
    }

    if(hkey != ekey)
    {
        DASSERT(ekey == -1);
        // We make a new entry in the table
        *((int *)&ptr[i]) = hkey;
    }

    return &ptr[i];
}

// Like above with unint64_t keys
static inline
void *poHT_findUInt64(void *data, size_t elementSize, uint64_t hkey,
        uint64_t ekey, uint32_t tableLen)
{
    uint32_t i;
    i = (poMurmurHash(&hkey, sizeof(hkey), 0/*seed*/)%tableLen)*
        elementSize;
    uint8_t *ptr;
    ptr = data;

#ifdef PO_DEBUG
    int lap = 0;
#endif

    while(*((int *)&ptr[i]) != ekey)
    {
        i += elementSize;
        if(i == tableLen*elementSize)
        {
            DVASSERT(++lap < 2, "can't find key="
                    "%"PRIu64" in full hash table\n", ekey);
            i = 0;
        }
    }

    if(hkey != ekey)
    {
        DASSERT(ekey == -1);
        // We make a new entry in the table
        *((int *)&ptr[i]) = hkey;
    }

    return &ptr[i];
}

#endif // #ifndef __PO_HASHTABLE_H__
