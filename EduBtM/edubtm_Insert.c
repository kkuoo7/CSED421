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
 * Module: edubtm_Insert.c
 *
 * Description : 
 *  This function edubtm_Insert(...) recursively calls itself until the type
 *  of a root page becomes LEAF.  If the given root page is an internal,
 *  it recursively calls itself using a proper child.  If the result of
 *  the call occur spliting, merging, or redistributing the children, it
 *  may insert, delete, or replace its own internal item, and if the given
 *  root page may be merged, splitted, or redistributed, it affects the
 *  return values.
 *
 * Exports:
 *  Four edubtm_Insert(ObjectID*, PageID*, KeyDesc*, KeyValue*, ObjectID*,
 *                  Boolean*, Boolean*, InternalItem*, Pool*, DeallocListElem*)
 *  Four edubtm_InsertLeaf(ObjectID*, PageID*, BtreeLeaf*, KeyDesc*, KeyValue*,
 *                      ObjectID*, Boolean*, Boolean*, InternalItem*)
 *  Four edubtm_InsertInternal(ObjectID*, BtreeInternal*, InternalItem*,
 *                          Two, Boolean*, InternalItem*)
 */


#include <string.h>
#include "EduBtM_common.h"
#include "BfM.h"
#include "OM_Internal.h"	/* for SlottedPage containing catalog object */
#include "EduBtM_Internal.h"



/*@================================
 * edubtm_Insert()
 *================================*/
/*
 * Function: Four edubtm_Insert(ObjectID*, PageID*, KeyDesc*, KeyValue*,
 *                           ObjectID*, Boolean*, Boolean*, InternalItem*,
 *                           Pool*, DeallocListElem*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  If the given root is a leaf page, it should get the correct entry in the
 *  leaf. If the entry is already in the leaf, it simply insert it into the
 *  entry and increment the number of ObjectIDs.  If it is not in the leaf it
 *  makes a new entry and insert it into the leaf.
 *  If there is not enough spage in the leaf, the page should be splitted.  The
 *  overflow page may be used or created by this routine. It is created when
 *  the size of the entry is greater than a third of a page.
 * 
 *  'h' is TRUE if the given root page is splitted and the entry item will be
 *  inserted into the parent page.  'f' is TRUE if the given page is not half
 *  full because of creating a new overflow page.
 *
 * Returns:
 *  Error code
 *    eBADBTREEPAGE_BTM
 *    some errors caused by function calls
 */
Four edubtm_Insert(
    ObjectID                    *catObjForFile,         /* IN catalog object of B+-tree file */
    PageID                      *root,                  /* IN the root of a Btree */
    KeyDesc                     *kdesc,                 /* IN Btree key descriptor */
    KeyValue                    *kval,                  /* IN key value */
    ObjectID                    *oid,                   /* IN ObjectID which will be inserted */
    Boolean                     *f,                     /* OUT whether it is merged by creating a new overflow page */
    Boolean                     *h,                     /* OUT whether it is splitted */
    InternalItem                *item,                  /* OUT Internal Item which will be inserted */
                                                        /*     into its parent when 'h' is TRUE */
    Pool                        *dlPool,                /* INOUT pool of dealloc list */
    DeallocListElem             *dlHead)                /* INOUT head of the dealloc list */
{
    Four                        e;                      /* error number */
    Boolean                     lh;                     /* local 'h' */
    Boolean                     lf;                     /* local 'f' */
    Two                         idx;                    /* index for the given key value */
    PageID                      newPid;                 /* a new PageID */
    KeyValue                    tKey;                   /* a temporary key */
    InternalItem                litem;                  /* a local internal item */
    BtreePage                   *apage;                 /* a pointer to the root page */
    btm_InternalEntry           *iEntry;                /* an internal entry */
    Two                         iEntryOffset;           /* starting offset of an internal entry */
    SlottedPage                 *catPage;               /* buffer page containing the catalog object */
    sm_CatOverlayForBtree       *catEntry;              /* pointer to Btree file catalog information */
    PhysicalFileID              pFid;                   /* B+-tree file's FileID */


    /* Error check whether using not supported functionality by EduBtM */
    int i;
    for(i=0; i<kdesc->nparts; i++)
    {
        if(kdesc->kpart[i].type!=SM_INT && kdesc->kpart[i].type!=SM_VARSTRING)
            ERR(eNOTSUPPORTED_EDUBTM);
    }

    
    /* Get the B+ tree file's FileID from the catalog object */
    e = BfM_GetTrain((TrainID*)catObjForFile, (char**)&catPage, PAGE_BUF);
    if (e < 0) ERR(e);
    
    GET_PTR_TO_CATENTRY_FOR_BTREE(catObjForFile, catPage, catEntry);
    
    MAKE_PHYSICALFILEID(pFid, catEntry->fid.volNo, catEntry->firstPage); 
    
    e = BfM_FreeTrain((TrainID*)catObjForFile, PAGE_BUF);
    if (e < 0) ERR(e);
    

    /*@ Initially the flags are FALSE */
    *h = *f = FALSE;
    
    e = BfM_GetTrain(root, (char **)&apage, PAGE_BUF);
    if (e < 0) ERR(e);

    if (apage->any.hdr.type & INTERNAL) {	/* Internal */
	
        /*@ Search the correct position */
        (Boolean) edubtm_BinarySearchInternal(&(apage->bi), kdesc, kval, &idx);

        /*@ Get the correct child page */
        if(idx >= 0) {
            iEntryOffset = apage->bi.slot[-idx];
            iEntry = (btm_InternalEntry*)&(apage->bi.data[iEntryOffset]);
            MAKE_PAGEID(newPid, root->volNo, iEntry->spid);
        } else
            MAKE_PAGEID(newPid, root->volNo, apage->bi.hdr.p0);
        
        /* Recursively call using the child */
        e = edubtm_Insert(catObjForFile, &newPid, kdesc, kval, oid, &lf, &lh,
                &litem, dlPool, dlHead);
        if (e < 0) ERRB1(e, root, PAGE_BUF);
        
        if (lh) {		/* the child was splitted */
            /*@ find the correct position */
            tKey.len = litem.klen;
            memcpy(&(tKey.val[0]), &(litem.kval[0]), tKey.len);
            (Boolean) edubtm_BinarySearchInternal(&(apage->bi), kdesc, &tKey, &idx);
            
            /* Insert the returned internal item into the given root page */
            e = edubtm_InsertInternal(catObjForFile, &(apage->bi), &litem, idx, h, item);
            if (e < 0) ERRB1(e, root, PAGE_BUF);
            
            e = BfM_SetDirty(root, PAGE_BUF);
            if (e < 0) ERRB1(e, root, PAGE_BUF);
            
        } else if( lf ) {	/* the child is not half full */
            e = btm_Underflow(&pFid, apage, &newPid, idx, f, &lh, &litem, 
                    dlPool, dlHead);
            if (e < 0) ERRB1(e, root, PAGE_BUF);
            
            if (lh) {
                /*@ find the correct position */
                tKey.len = litem.klen;
                memcpy(&(tKey.val[0]), &(litem.kval[0]), tKey.len);
                (Boolean) edubtm_BinarySearchInternal(&(apage->bi), kdesc, &tKey, &idx);
                
                e = edubtm_InsertInternal(catObjForFile, &(apage->bi),
                            &litem, idx, h, item);
                if (e < 0) ERRB1(e, root, PAGE_BUF);
            }
            
            e = BfM_SetDirty(root, PAGE_BUF);
            if( e < 0 ) ERRB1(e, root, PAGE_BUF );
        }
	
    } else if( apage->any.hdr.type & LEAF ) {

        e = edubtm_InsertLeaf(catObjForFile, root, &(apage->bl),
                    kdesc, kval, oid, f, h, item);
        if (e < 0) ERRB1(e, root, PAGE_BUF);
        
        e = BfM_SetDirty(root, PAGE_BUF);
        if (e < 0) ERRB1(e, root, PAGE_BUF);
	
    } else
        ERRB1(eBADBTREEPAGE_BTM, root, PAGE_BUF);
    
    e = BfM_FreeTrain(root, PAGE_BUF);
    if (e < 0) ERR(e);
    
    return(eNOERROR);
    
}   /* edubtm_Insert() */



/*@================================
 * edubtm_InsertLeaf()
 *================================*/
/*
 * Function: Four edubtm_InsertLeaf(ObjectID*, PageID*, BtreeLeaf*, KeyDesc*,
 *                               KeyValue*, ObjectID*, Boolean*, Boolean*,
 *                               InternalItem*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  Insert into the given leaf page an ObjectID with the given key.
 *
 * Returns:
 *  Error code
 *    eDUPLICATEDKEY_BTM
 *    eDUPLICATEDOBJECTID_BTM
 *    some errors causd by function calls
 *
 * Side effects:
 *  1) f : TRUE if the leaf page is underflowed by creating an overflow page
 *  2) h : TRUE if the leaf page is splitted by inserting the given ObjectID
 *  3) item : item to be inserted into the parent
 */
Four edubtm_InsertLeaf(
    ObjectID                    *catObjForFile, /* IN catalog object of B+-tree file */
    PageID                      *pid,           /* IN PageID of Leag Page */
    BtreeLeaf                   *page,          /* INOUT pointer to buffer page of Leaf page */
    KeyDesc                     *kdesc,         /* IN Btree key descriptor */
    KeyValue                    *kval,          /* IN key value */
    ObjectID                    *oid,           /* IN ObjectID which will be inserted */
    Boolean                     *f,             /* OUT whether it is merged by creating */
                                                /*     a new overflow page */
    Boolean                     *h,             /* OUT whether it is splitted */
    InternalItem                *item)          /* OUT Internal Item which will be inserted */
                                                /*     into its parent when 'h' is TRUE */
{
    Four                        e;              /* error number */
    Two                         i;
    Two                         idx;            /* index for the given key value */
    LeafItem                    leaf;           /* a Leaf Item */
    Boolean                     found;          /* search result */
    btm_LeafEntry               *entry;         /* an entry in a leaf page */
    Two                         entryOffset;    /* start position of an entry */
    Two                         alignedKlen;    /* aligned length of the key length */
    PageID                      ovPid;          /* PageID of an overflow page */
    Two                         entryLen;       /* length of an entry */
    ObjectID                    *oidArray;      /* an array of ObjectIDs */
    Two                         oidArrayElemNo; /* an index for the ObjectID array */


    /* Error check whether using not supported functionality by EduBtM */
    for(i=0; i<kdesc->nparts; i++)
    {
        if(kdesc->kpart[i].type!=SM_INT && kdesc->kpart[i].type!=SM_VARSTRING)
            ERR(eNOTSUPPORTED_EDUBTM);
    }

    
    /*@ Initially the flags are FALSE */
    *h = *f = FALSE;
    
    /*@ Search the leaf entry */
    found = edubtm_BinarySearchLeaf(page, kdesc, kval, &idx);
    
    /* check duplicated key */
    if ((found) && (kdesc->flag & KEYFLAG_UNIQUE))
        return (eDUPLICATEDKEY_BTM);

    if (found) {	/* found */
        entryOffset = page->slot[-idx];
        entry = (btm_LeafEntry*)&(page->data[entryOffset]);
        alignedKlen = ALIGNED_LENGTH(entry->klen);

        /* the total length of the entry which includes the given ObjectID */
        entryLen = BTM_LEAFENTRY_FIXED + alignedKlen +
        (entry->nObjects+1)*OBJECTID_SIZE;
                
        if(OBJECTID_SIZE <= BL_FREE(page)) { /* enough space */

            /*@ Find the correct position which the given object is inserted */
            oidArray = (ObjectID*)&(entry->kval[alignedKlen]);

            e = btm_BinarySearchOidArray(oidArray, oid, entry->nObjects, &oidArrayElemNo);
            if (e) ERR(eDUPLICATEDOBJECTID_BTM);

            if (entryOffset + entryLen - OBJECTID_SIZE == page->hdr.free &&
                BL_CFREE(page) >= OBJECTID_SIZE) {		

                page->hdr.free += OBJECTID_SIZE;
                
            } else if (BL_CFREE(page) >= entryLen) {

                entryOffset = page->slot[-idx] = page->hdr.free;
                memcpy(&(page->data[entryOffset]), (char*)entry, entryLen-OBJECTID_SIZE);
                entry = (btm_LeafEntry*)&(page->data[entryOffset]);
                oidArray = (ObjectID*)&(entry->kval[alignedKlen]);
                
                page->hdr.free += entryLen;
                page->hdr.unused += entryLen - OBJECTID_SIZE;
                
            } else {
                edubtm_CompactLeafPage(page, idx);

                entryOffset = page->slot[-idx];
                entry = (btm_LeafEntry*)&(page->data[entryOffset]);
                oidArray = (ObjectID*)&(entry->kval[alignedKlen]);

                page->hdr.free += OBJECTID_SIZE;
            }
            
            /*
            ** empty (oidArrayElemNo+1)-th element by moving
            ** the ObjectIDs.
            */
            memmove((char*)&(oidArray[oidArrayElemNo+2]), (char*)&(oidArray[oidArrayElemNo+1]),
                OBJECTID_SIZE*(entry->nObjects-(oidArrayElemNo+1)));
            oidArray[oidArrayElemNo+1] = *oid;

            entry->nObjects++;
        
        } else {	/* split */
        
            /*
            ** There is no enough spage, split the leaf page and insert
            ** the ObjectID. The key value is extracted directly from the
            ** idx-th entry of the leaf page in btm_SplitLeaf().
            */
            leaf.nObjects = entry->nObjects;
            leaf.oid = *oid;
            
            e = edubtm_SplitLeaf(catObjForFile, pid, page, idx-1, &leaf, item);
            if (e < 0) ERR(e);
            
            *h = TRUE;	/* Mark */
        }
    }
	else {	/* not found */
        
        /* Make a new entry and insert it into the leaf page */
        
        /* the length of a new entry */
        alignedKlen = ALIGNED_LENGTH(kval->len);
        entryLen = BTM_LEAFENTRY_FIXED + alignedKlen + sizeof(ObjectID);

        /* There is enough space? We should count the slot space. */
        if (entryLen + sizeof(Two) <= BL_FREE(page)) { 	/*enough space */

            if (BL_CFREE(page) < entryLen+sizeof(Two))
            edubtm_CompactLeafPage(page, NIL);
            
            /* Move slots for a new slot */
            for(i = page->hdr.nSlots-1; i >= idx+1; i--)
            page->slot[-(i+1)] = page->slot[-i];
            
            /*@ Store the new entry */
            entryOffset = page->slot[-(idx+1)] = page->hdr.free;
            entry = (btm_LeafEntry*)&(page->data[entryOffset]);

            entry->nObjects = 1;
            entry->klen = kval->len;
            memcpy(&(entry->kval[0]), &(kval->val[0]), entry->klen);
            *((ObjectID*)&(entry->kval[alignedKlen])) = *oid;
            
            page->hdr.free += entryLen;	/* slot size is not included */
            page->hdr.nSlots++;
            
        } else {	/* There is not enough space */
            
            /* Construct the new entry, leaf item */
            leaf.nObjects = 0;
            leaf.klen = kval->len;
            memcpy(&(leaf.kval[0]), &(kval->val[0]), leaf.klen);
            leaf.oid = *oid;
            
            e = edubtm_SplitLeaf(catObjForFile, pid, page, idx, &leaf, item);
            if (e < 0) ERR(e);
            
            *h = TRUE;	/* mark */
        }	
    }

    return(eNOERROR);
    
} /* edubtm_InsertLeaf() */



/*@================================
 * edubtm_InsertInternal()
 *================================*/
/*
 * Function: Four edubtm_InsertInternal(ObjectID*, BtreeInternal*, InternalItem*, Two, Boolean*, InternalItem*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  This routine insert the given internal item into the given page. If there
 *  is not enough space in the page, it should split the page and the new
 *  internal item should be returned for inserting into the parent.
 *
 * Returns:
 *  Error code
 *    some errors caused by function calls
 *
 * Side effects:
 *  h:	TRUE if the page is splitted
 *  ritem: an internal item which will be inserted into parent
 *          if spliting occurs.
 */
Four edubtm_InsertInternal(
    ObjectID            *catObjForFile, /* IN catalog object of B+-tree file */
    BtreeInternal       *page,          /* INOUT Page Pointer */
    InternalItem        *item,          /* IN Iternal item which is inserted */
    Two                 high,           /* IN index in the given page */
    Boolean             *h,             /* OUT whether the given page is splitted */
    InternalItem        *ritem)         /* OUT if the given page is splitted, the internal item may be returned by 'ritem'. */
{
    Four                e;              /* error number */
    Two                 i;              /* index */
    Two                 entryOffset;    /* starting offset of an internal entry */
    Two                 entryLen;       /* length of the new entry */
    btm_InternalEntry   *entry;         /* an internal entry of an internal page */

    
    /*@ Initially the flag are FALSE */
    *h = FALSE;
    
    /* the length of a entry */
    entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two)+item->klen);

    /* There is enough space? We should count the slot space. */
    if(BI_FREE(page) < entryLen+sizeof(Two)) {   /* not enough */
	
        /* Insert the item after spliting the given internal page */
        e = edubtm_SplitInternal(catObjForFile, page, high, item, ritem);
        if (e < 0) ERR(e);
        
        *h = TRUE;	/* mark */
        
    } else {	/* enough space */

        if (BI_CFREE(page) < entryLen + sizeof(Two))
            edubtm_CompactInternalPage(page, NIL);
        
        /* Move slots for a new slot */
        for(i = page->hdr.nSlots-1; i > high; i--)
            page->slot[-(i+1)] = page->slot[-i];
        
        /*@ store the new entry and update some informations */
        entryOffset = page->slot[-(high+1)] = page->hdr.free;
        entry = (btm_InternalEntry*)&(page->data[entryOffset]);
        memcpy((char*)entry, (char*)item, entryLen);
        page->hdr.nSlots++;
        page->hdr.free += entryLen;
        
        *h = FALSE;		/* it is not splitted */
    }
    
    return(eNOERROR);
    
} /* edubtm_InsertInternal() */

