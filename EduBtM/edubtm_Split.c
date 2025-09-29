/******************************************************************************/
/*                                                                            */
/*    ODYSSEUS/EduCOSMOS Educational-Purpose Object Storage System            */
/*                                                                            */
/*    Developed by Professor Kyu-Young Whang et al.                           */
/*                                                                            */
/*    Database and Multimedia Laboratory                                      */
/*                                                                            */
/*    Computer Science Department and                                         */
/*    Advanced Information Technology Research Center (AITrc)                 */
/*    Korea Advanced Institute of Science and Technology (KAIST)              */
/*                                                                            */
/*    e-mail: kywhang@cs.kaist.ac.kr                                          */
/*    phone: +82-42-350-7722                                                  */
/*    fax: +82-42-350-8380                                                    */
/*                                                                            */
/*    Copyright (c) 1995-2013 by Kyu-Young Whang                              */
/*                                                                            */
/*    All rights reserved. No part of this software may be reproduced,        */
/*    stored in a retrieval system, or transmitted, in any form or by any     */
/*    means, electronic, mechanical, photocopying, recording, or otherwise,   */
/*    without prior written permission of the copyright owner.                */
/*                                                                            */
/******************************************************************************/
/*
 * Module: edubtm_Split.c
 *
 * Description : 
 *  This file has three functions about 'split'.
 *  'edubtm_SplitInternal(...) and edubtm_SplitLeaf(...) insert the given item
 *  after spliting, and return 'ritem' which should be inserted into the
 *  parent page.
 *
 * Exports:
 *  Four edubtm_SplitInternal(ObjectID*, BtreeInternal*, Two, InternalItem*, InternalItem*)
 *  Four edubtm_SplitLeaf(ObjectID*, PageID*, BtreeLeaf*, Two, LeafItem*, InternalItem*)
 */


#include <string.h>
#include "EduBtM_common.h"
#include "BfM.h"
#include "EduBtM_Internal.h"



/*@================================
 * edubtm_SplitInternal()
 *================================*/
/*
 * Function: Four edubtm_SplitInternal(ObjectID*, BtreeInternal*,Two, InternalItem*, InternalItem*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  At first, the function edubtm_SplitInternal(...) allocates a new internal page
 *  and initialize it.  Secondly, all items in the given page and the given
 *  'item' are divided by halves and stored to the two pages.  By spliting,
 *  the new internal item should be inserted into their parent and the item will
 *  be returned by 'ritem'.
 *
 *  A temporary page is used because it is difficult to use the given page
 *  directly and the temporary page will be copied to the given page later.
 *
 * Returns:
 *  error code
 *    some errors caused by function calls
 *
 * Note:
 *  The caller should call BfM_SetDirty() for 'fpage'.
 */
Four edubtm_SplitInternal(
    ObjectID                    *catObjForFile,         /* IN catalog object of B+ tree file */
    BtreeInternal               *fpage,                 /* INOUT the page which will be splitted */
    Two                         high,                   /* IN slot No. for the given 'item' */
    InternalItem                *item,                  /* IN the item which will be inserted */
    InternalItem                *ritem)                 /* OUT the item which will be returned by spliting */
{
    Four                        e;                      /* error number */
    Two                         i;                      /* slot No. in the given page, fpage */
    Two                         j;                      /* slot No. in the splitted pages */
    Two                         k;                      /* slot No. in the new page */
    Two                         maxLoop;                /* # of max loops; # of slots in fpage + 1 */
    Four                        sum;                    /* the size of a filled area */
    Boolean                     flag=FALSE;             /* TRUE if 'item' become a member of fpage */
    PageID                      newPid;                 /* for a New Allocated Page */
    BtreeInternal               *npage;                 /* a page pointer for the new allocated page */
    Two                         fEntryOffset;           /* starting offset of an entry in fpage */
    Two                         nEntryOffset;           /* starting offset of an entry in npage */
    Two                         entryLen;               /* length of an entry */
    btm_InternalEntry           *fEntry;                /* internal entry in the given page, fpage */
    btm_InternalEntry           *nEntry;                /* internal entry in the new page, npage*/
    Boolean                     isTmp;


    /*@ Allocate a new page and initialize it as an internal page */
    e = btm_AllocPage(catObjForFile, (PageID *)&fpage->hdr.pid, &newPid); 
    if (e < 0) ERR(e);

    /* check this B-tree is temporary */
    e = btm_IsTemporary(catObjForFile, &isTmp); 
    if (e < 0) ERR(e);

    e = edubtm_InitInternal(&newPid, FALSE, isTmp);
    if (e < 0) ERR(e);

    e = BfM_GetNewTrain( &newPid, (char **)&npage, PAGE_BUF );
    if (e < 0) ERR(e);

    /* loop until 'sum' becomes greater than BI_HALF */
    /* j : loop counter, maximum loop count = # of old Slots and a new slot */
    /* i : slot No. variable of fpage */
    maxLoop = fpage->hdr.nSlots+1;
    i = 0; 
    sum = 0;
    flag = FALSE;

    for (j = 0; j < maxLoop && sum < BI_HALF; j++) {
        if (j == high+1) {	/* use the given 'item' */
            entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two)+item->klen);
            flag = TRUE;
        } else {
            fEntryOffset = fpage->slot[-i];
            fEntry = (btm_InternalEntry*)&(fpage->data[fEntryOffset]);
            entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two)+fEntry->klen);           
            i++;		/* increment the slot no. */
        }
        sum += entryLen + sizeof(Two);
    }

    /* i-th old entries are to be remained in 'fpage' */
    fpage->hdr.nSlots = i;

    /*@ fill the new page */
    /* i : slot no. of fpage, continued from the above loop */
    /* j : loop counter, continued from the above loop */
    /* k : slot No. of new page, npage */
    /* (k == -1; special case, constructing 'ritem' */
    for (k = -1; j < maxLoop; j++, k++) {
        if (k == -1) {		/* Construct 'ritem' */
            /* We he InternalEntry as btm_InternalEntry. */
            /* So two their data structures should consider this situation. */
            nEntry = (btm_InternalEntry*)ritem;	    
        } else {
            nEntryOffset = npage->slot[-k] = npage->hdr.free;
            nEntry = (btm_InternalEntry*)&(npage->data[nEntryOffset]);
        }
        
        if (j == high+1) { /* use the given 'item' */
            nEntry->spid = item->spid;
            nEntry->klen = item->klen;
            memcpy(&(nEntry->kval[0]), &(item->kval[0]), nEntry->klen);
            entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two)+nEntry->klen);
        } else {
            fEntryOffset = fpage->slot[-i];
            fEntry = (btm_InternalEntry*)&(fpage->data[fEntryOffset]);
            entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two)+fEntry->klen);
            memcpy((char*)nEntry, (char*)fEntry, entryLen);
            
            if (fEntryOffset + entryLen == fpage->hdr.free)
                fpage->hdr.free -= entryLen;
            else 
                fpage->hdr.unused += entryLen;
            
            i++;		/* increment the slot No. */
        }

        if (k == -1) {		/* Construct 'ritem' */
            /* In this case nEntry points to ritem. */
            npage->hdr.p0 = nEntry->spid;
            ritem->spid = newPid.pageNo;
        } else {
            npage->hdr.free += entryLen;
        }
    }

    npage->hdr.nSlots = k;

    /*@ adjust 'fpage' */
    if (flag) {
        entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two)+item->klen);
        
        if (BI_CFREE(fpage) < entryLen + sizeof(Two))
            btm_CompactInternalPage(fpage, NIL);

        for (i = fpage->hdr.nSlots-1; i >= high+1; i--)
            fpage->slot[-(i+1)] = fpage->slot[-i];
            
        fEntryOffset = fpage->slot[-(high+1)] = fpage->hdr.free;
        fEntry = (btm_InternalEntry*)&(fpage->data[fEntryOffset]);
        
        fEntry->spid = item->spid;
        fEntry->klen = item->klen;
        memcpy(&(fEntry->kval[0]), &(item->kval[0]), fEntry->klen);

        fpage->hdr.free += entryLen;
        fpage->hdr.nSlots++;
    }

    /* If the given page was a root, it is not a root any more */
    if (fpage->hdr.type & ROOT) 
        fpage->hdr.type = INTERNAL;

    e = BfM_SetDirty(&newPid, PAGE_BUF);
    if (e < 0) ERRB1(e, &newPid, PAGE_BUF);

    e = BfM_FreeTrain(&newPid, PAGE_BUF);
    if (e < 0) ERR(e);

    return(eNOERROR);
    
} /* edubtm_SplitInternal() */



/*@================================
 * edubtm_SplitLeaf()
 *================================*/
/*
 * Function: Four edubtm_SplitLeaf(ObjectID*, PageID*, BtreeLeaf*, Two, LeafItem*, InternalItem*)
 *
 * Description: 
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  The function edubtm_SplitLeaf(...) is similar to edubtm_SplitInternal(...) except
 *  that the entry of a leaf differs from the entry of an internal and the first
 *  key value of a new page is used to make an internal item of their parent.
 *  Internal pages do not maintain the linked list, but leaves do it, so links
 *  are properly updated.
 *
 * Returns:
 *  Error code
 *  eDUPLICATEDOBJECTID_BTM
 *    some errors caused by function calls
 *
 * Note:
 *  The caller should call BfM_SetDirty() for 'fpage'.
 */
Four edubtm_SplitLeaf(
    ObjectID                    *catObjForFile, /* IN catalog object of B+ tree file */
    PageID                      *root,          /* IN PageID for the given page, 'fpage' */
    BtreeLeaf                   *fpage,         /* INOUT the page which will be splitted */
    Two                         high,           /* IN slotNo for the given 'item' */
    LeafItem                    *item,          /* IN the item which will be inserted */
    InternalItem                *ritem)         /* OUT the item which will be returned by spliting */
{
    Four                        e;              /* error number */
    Two                         i;              /* slot No. in the given page, fpage */
    Two                         j;              /* slot No. in the splitted pages */
    Two                         k;              /* slot No. in the new page */
    Two                         maxLoop;        /* # of max loops; # of slots in fpage + 1 */
    Four                        sum;            /* the size of a filled area */
    PageID                      newPid;         /* for a New Allocated Page */
    PageID                      nextPid;        /* for maintaining doubly linked list */
    BtreeLeaf                   tpage;          /* a temporary page for the given page */
    BtreeLeaf                   *npage;         /* a page pointer for the new page */
    BtreeLeaf                   *mpage;         /* for doubly linked list */
    btm_LeafEntry               *itemEntry;     /* entry for the given 'item' */
    btm_LeafEntry               *fEntry;        /* an entry in the given page, 'fpage' */
    btm_LeafEntry               *nEntry;        /* an entry in the new page, 'npage' */
    ObjectID                    *iOidArray;     /* ObjectID array of 'itemEntry' */
    ObjectID                    *fOidArray;     /* ObjectID array of 'fEntry' */
    Two                         fEntryOffset;   /* starting offset of 'fEntry' */
    Two                         nEntryOffset;   /* starting offset of 'nEntry' */
    Two                         oidArrayNo;     /* element No in an ObjectID array */
    Two                         alignedKlen;    /* aligned length of the key length */
    Two                         itemEntryLen;   /* length of entry for item */
    Two                         entryLen;       /* entry length */
    Boolean                     flag;
    Boolean                     isTmp;


    /*
    ** To item' uniformly without considering whether 'item' is a new
    ** entry or an ObjectID is inserted, we copy the entry for 'item' into
    ** 'tpage'. If an ObjectID is inserted, the corresponding entry is moved
    *  from the fpage into the 'tpage' and it is deleted from the 'fpage'.
    */

    itemEntry = (btm_LeafEntry*)&(tpage.data[0]);
    if (item->nObjects == 0) {	/* a new entry */

        alignedKlen = ALIGNED_LENGTH(item->klen);
        itemEntry->nObjects = 1;
        itemEntry->klen = item->klen;	
        memcpy(&(itemEntry->kval[0]), &(item->kval[0]), itemEntry->klen);
        memcpy(&(itemEntry->kval[alignedKlen]), (char*)&(item->oid), OBJECTID_SIZE);
        
        itemEntryLen = BTM_LEAFENTRY_FIXED + alignedKlen + OBJECTID_SIZE;

    } else if (item->nObjects > 0) { /* an ObjectID is inserted */

        fEntryOffset = fpage->slot[-(high+1)];
        fEntry = (btm_LeafEntry*)&(fpage->data[fEntryOffset]);
        alignedKlen = ALIGNED_LENGTH(fEntry->klen);
        fOidArray = (ObjectID*)&(fEntry->kval[alignedKlen]);
        
        e = btm_BinarySearchOidArray(fOidArray, &(item->oid), fEntry->nObjects, &oidArrayNo);
        if (e == TRUE) ERR(eDUPLICATEDOBJECTID_BTM);

        itemEntry->nObjects = fEntry->nObjects + 1;
        itemEntry->klen = fEntry->klen;
        memcpy(&(itemEntry->kval[0]), &(fEntry->kval[0]), itemEntry->klen);
        iOidArray = (ObjectID*)&(itemEntry->kval[alignedKlen]);
        memcpy((char*)&(iOidArray[0]), (char*)&(fOidArray[0]), OBJECTID_SIZE * (oidArrayNo + 1));
        iOidArray[oidArrayNo + 1] = item->oid;
        memcpy((char*)&(iOidArray[oidArrayNo + 2]), (char*)&(fOidArray[oidArrayNo + 1]), OBJECTID_SIZE * (fEntry->nObjects - (oidArrayNo + 1)));

        itemEntryLen = BTM_LEAFENTRY_FIXED + alignedKlen + OBJECTID_SIZE * (itemEntry->nObjects);

        /* delete the fEntry from the fpage */
        for (i = high + 1; i < fpage->hdr.nSlots; i++)
            fpage->slot[-i] = fpage->slot[-(i + 1)];
        fpage->hdr.nSlots--;
        if (fEntryOffset + itemEntryLen - sizeof(ObjectID) == fpage->hdr.free)
            fpage->hdr.free -= itemEntryLen - sizeof(ObjectID);
        else
            fpage->hdr.unused += itemEntryLen - sizeof(ObjectID);

    }

    /*@ Allocate a new page and initialize it as a leaf page */
    e = btm_AllocPage(catObjForFile, (PageID *)&fpage->hdr.pid, &newPid); 
    if (e < 0) ERR(e);

    /* check this B-tree is temporary */
    e = btm_IsTemporary(catObjForFile, &isTmp);
    if (e < 0) ERR(e);

    /* Initialize the new page to the leaf page. */
    e = edubtm_InitLeaf(&newPid, FALSE, isTmp);
    if (e < 0) ERR(e);

    e = BfM_GetNewTrain(&newPid, (char **)&npage, PAGE_BUF);
    if (e < 0) ERR(e);

    /* loop until 'sum' becomes greater than BL_HALF */
    /* j : loop counter, maximum loop count = # of old Slots and a new slot */
    /* i : slot No variable of fpage */
    maxLoop = fpage->hdr.nSlots + 1;
    flag = FALSE;		/* itemEntry is to be placed on new page. */
    for (sum = 0, i = 0, j = 0; j < maxLoop && sum < BL_HALF; j++) {

        if (j == high + 1) {	/* use itemEntry */	    
            sum += itemEntryLen;
            flag = TRUE;	/* itemEntry is to be placed on fpage. */
        } else {	    
            fEntryOffset = fpage->slot[-i];
            fEntry = (btm_LeafEntry*)&(fpage->data[fEntryOffset]);
            sum += BTM_LEAFENTRY_FIXED + ALIGNED_LENGTH(fEntry->klen) + ((fEntry->nObjects > 0) ? OBJECTID_SIZE * fEntry->nObjects : sizeof(ShortPageID));
            i++;		/* increment the slot No. */
        }		
        sum += sizeof(Two);	/* slot space */
    }

    /* i-th old entries will be remained in 'fpage' */
    fpage->hdr.nSlots = i;

    /*@ fill the new page */
    /* i : slot no. of fpage, continued from above loop */
    /* j : loop counter, continued from above loop */
    /* k : slot no. of new page, npage */
    for (k = 0; j < maxLoop; j++, k++) {
        nEntryOffset = npage->slot[-k] = npage->hdr.free;
        nEntry = (btm_LeafEntry*)&(npage->data[nEntryOffset]);
        
        if (j == high + 1) {	/* use 'itemEntry' */
            entryLen = itemEntryLen;
            memcpy((char*)nEntry, (char*)itemEntry, entryLen);
        } else {
            fEntryOffset = fpage->slot[-i];
            fEntry = (btm_LeafEntry*)&(fpage->data[fEntryOffset]);
            
            if (fEntry->nObjects < 0) {	/* overflow page */
                entryLen = BTM_LEAFENTRY_FIXED + ALIGNED_LENGTH(fEntry->klen) + sizeof(ShortPageID);
            } else {
                entryLen = BTM_LEAFENTRY_FIXED + ALIGNED_LENGTH(fEntry->klen) + fEntry->nObjects * OBJECTID_SIZE;
            }
            
            memcpy((char*)nEntry, (char*)fEntry, entryLen);
            
            /* free the space from the fpage */
            if (fEntryOffset + entryLen == fpage->hdr.free)
                fpage->hdr.free -= entryLen;
            else
                fpage->hdr.unused += entryLen;

            i++;		/* increment the slot No. */
        }
        
        npage->hdr.free += entryLen;
    }

    npage->hdr.nSlots = k;

    /*@ adjust 'fpage' */
    /* if the given item is to be stored in 'fpage', place it in 'fpage' */
    if (flag) {
        if (BL_CFREE(fpage) < itemEntryLen + sizeof(Two))
            edubtm_CompactLeafPage(fpage, NIL);

        /* Empty the (high + 1)-th slot of 'fpage' */
        for (i = fpage->hdr.nSlots - 1; i >= high + 1; i--)
            fpage->slot[-(i + 1)] = fpage->slot[-i];
        
        fEntryOffset = fpage->slot[-(high + 1)] = fpage->hdr.free;
        fEntry = (btm_LeafEntry*)&(fpage->data[fEntryOffset]);
        memcpy((char*)fEntry, (char*)itemEntry, itemEntryLen);
        
        fpage->hdr.free += itemEntryLen;
        fpage->hdr.nSlots++;
    }

    /* Construct 'ritem' which will be inserted into its parent */
    /* The key of ritem is that of the 0-th slot of npage. */
    nEntryOffset = npage->slot[0];
    nEntry = (btm_LeafEntry*)&(npage->data[nEntryOffset]);	
    ritem->spid = newPid.pageNo;
    ritem->klen = nEntry->klen;
    memcpy(&(ritem->kval[0]), &(nEntry->kval[0]), ritem->klen);

    /* If the given page was a root, it is not a root any more. */
    if (fpage->hdr.type & ROOT) fpage->hdr.type = LEAF;

    /* Leaves are connected by doubly linked list, so it should update the links. */
    MAKE_PAGEID(nextPid, root->volNo, fpage->hdr.nextPage);
    npage->hdr.nextPage = nextPid.pageNo;
    npage->hdr.prevPage = root->pageNo;
    fpage->hdr.nextPage = newPid.pageNo;

    if (nextPid.pageNo != NIL) {
        e = BfM_GetTrain(&nextPid, (char **)&mpage, PAGE_BUF);
        if (e < 0)  ERRB1(e, &newPid, PAGE_BUF);

        mpage->hdr.prevPage = newPid.pageNo;
        
        e = BfM_SetDirty(&nextPid, PAGE_BUF);
        if (e < 0)  ERRB1(e, &newPid, PAGE_BUF);
        
        e = BfM_FreeTrain(&nextPid, PAGE_BUF);
        if (e < 0)  ERRB1(e, &newPid, PAGE_BUF);
    }

    e = BfM_SetDirty(&newPid, PAGE_BUF);
    if (e < 0)  ERRB1(e, &newPid, PAGE_BUF);

    e = BfM_FreeTrain(&newPid, PAGE_BUF);
    if (e < 0) ERR(e);

    return(eNOERROR);

    
    
} /* edubtm_SplitLeaf() */
