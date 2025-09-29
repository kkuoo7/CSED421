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
 * Module: EduBtM_Fetch.c
 *
 * Description :
 *  Find the first object satisfying the given condition.
 *  If there is no such object, then return with 'flag' field of cursor set
 *  to CURSOR_EOS. If there is an object satisfying the condition, then cursor
 *  points to the object position in the B+ tree and the object identifier
 *  is returned via 'cursor' parameter.
 *  The condition is given with a key value and a comparison operator;
 *  the comparison operator is one among SM_BOF, SM_EOF, SM_EQ, SM_LT, SM_LE, SM_GT, SM_GE.
 *
 * Exports:
 *  Four EduBtM_Fetch(PageID*, KeyDesc*, KeyValue*, Four, KeyValue*, Four, BtreeCursor*)
 */


#include <string.h>
#include "EduBtM_common.h"
#include "BfM.h"
#include "EduBtM_Internal.h"


/*@ Internal Function Prototypes */
Four edubtm_Fetch(PageID*, KeyDesc*, KeyValue*, Four, KeyValue*, Four, BtreeCursor*);



/*@================================
 * EduBtM_Fetch()
 *================================*/
/*
 * Function: Four EduBtM_Fetch(PageID*, KeyDesc*, KeyVlaue*, Four, KeyValue*, Four, BtreeCursor*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  Find the first object satisfying the given condition. See above for detail.
 *
 * Returns:
 *  error code
 *    eBADPARAMETER_BTM
 *    some errors caused by function calls
 *
 * Side effects:
 *  cursor  : The found ObjectID and its position in the Btree Leaf
 *            (it may indicate a ObjectID in an  overflow page).
 */
Four EduBtM_Fetch(
    PageID   *root,		/* IN The current root of the subtree */
    KeyDesc  *kdesc,		/* IN Btree key descriptor */
    KeyValue *startKval,	/* IN key value of start condition */
    Four     startCompOp,	/* IN comparison operator of start condition */
    KeyValue *stopKval,		/* IN key value of stop condition */
    Four     stopCompOp,	/* IN comparison operator of stop condition */
    BtreeCursor *cursor)	/* OUT Btree Cursor */
{
    int i;
    Four e;		   /* error number */

    
    if (root == NULL) ERR(eBADPARAMETER_BTM);

    /* Error check whether using not supported functionality by EduBtM */
    for(i=0; i<kdesc->nparts; i++)
    {
        if(kdesc->kpart[i].type!=SM_INT && kdesc->kpart[i].type!=SM_VARSTRING)
            ERR(eNOTSUPPORTED_EDUBTM);
    }

    if (startCompOp == SM_BOF) {
        /* Return the first object of the B+ tree. */
        e = edubtm_FirstObject(root, kdesc, stopKval, stopCompOp, cursor);
        if (e < 0) ERR(e);
	
    } else if (startCompOp == SM_EOF) {
        /* Return the last object of the B+ tree. */
        e = edubtm_LastObject(root, kdesc, stopKval, stopCompOp, cursor);
        if (e < 0) ERR(e);
	
    } else { /* SM_EQ, SM_LT, SM_LE, SM_GT, SM_GE */
        e = edubtm_Fetch(root, kdesc, startKval, startCompOp, stopKval, stopCompOp, cursor);
        if (e < 0) ERR(e);
    }

    return(eNOERROR);

} /* EduBtM_Fetch() */



/*@================================
 * edubtm_Fetch()
 *================================*/
/*
 * Function: Four edubtm_Fetch(PageID*, KeyDesc*, KeyVlaue*, Four, KeyValue*, Four, BtreeCursor*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  Find the first object satisfying the given condition.
 *  This function handles only the following conditions:
 *  SM_EQ, SM_LT, SM_LE, SM_GT, SM_GE.
 *
 * Returns:
 *  Error code *   
 *    eBADCOMPOP_BTM
 *    eBADBTREEPAGE_BTM
 *    some errors caused by function calls
 */
Four edubtm_Fetch(
    PageID              *root,          /* IN The current root of the subtree */
    KeyDesc             *kdesc,         /* IN Btree key descriptor */
    KeyValue            *startKval,     /* IN key value of start condition */
    Four                startCompOp,    /* IN comparison operator of start condition */
    KeyValue            *stopKval,      /* IN key value of stop condition */
    Four                stopCompOp,     /* IN comparison operator of stop condition */
    BtreeCursor         *cursor)        /* OUT Btree Cursor */
{
    Four                e;              /* error number */
    Four                cmp;            /* result of comparison */
    Two                 idx;            /* index */
    PageID              child;          /* child page when the root is an internla page */
    Two                 alignedKlen;    /* aligned size of the key length */
    BtreePage           *apage;         /* a Page Pointer to the given root */
    BtreeOverflow       *opage;         /* a page pointer if it necessary to access an overflow page */
    Boolean             found;          /* search result */
    PageID              *leafPid;       /* leaf page pointed by the cursor */
    Two                 slotNo;         /* slot pointed by the slot */
    PageID              ovPid;          /* PageID of the overflow page */
    PageNo              ovPageNo;       /* PageNo of the overflow page */
    PageID              prevPid;        /* PageID of the previous page */
    PageID              nextPid;        /* PageID of the next page */
    ObjectID            *oidArray;      /* array of the ObjectIDs */
    Two                 iEntryOffset;   /* starting offset of an internal entry */
    btm_InternalEntry   *iEntry;        /* an internal entry */
    Two                 lEntryOffset;   /* starting offset of a leaf entry */
    btm_LeafEntry       *lEntry;        /* a leaf entry */


    /* Error check whether using not supported functionality by EduBtM */
    int i;
    for(i=0; i<kdesc->nparts; i++)
    {
        if(kdesc->kpart[i].type!=SM_INT && kdesc->kpart[i].type!=SM_VARSTRING)
            ERR(eNOTSUPPORTED_EDUBTM);
    }

    /*@ get the page */
    e = BfM_GetTrain(root, (char **)&apage, PAGE_BUF);
    if (e < 0)  ERR(e);
   
    if (apage->any.hdr.type & INTERNAL) {
        /*@ Find the child page by using binary search routine. */
        (Boolean) btm_BinarySearchInternal(&(apage->bi), kdesc, startKval, &idx);
        
        /*@ Get the child page */
        if (idx >= 0) {
            iEntryOffset = apage->bi.slot[-idx];
            iEntry = (btm_InternalEntry*)&(apage->bi.data[iEntryOffset]);
            MAKE_PAGEID(child, root->volNo, iEntry->spid);
        } else
            MAKE_PAGEID(child, root->volNo, apage->bi.hdr.p0);

        /*@ recursive call */
        /* Recursively call itself by using the child */
        e = BtM_Fetch(&child, kdesc, startKval, startCompOp, stopKval, stopCompOp, cursor);
        if (e < 0) ERRB1(e, root, PAGE_BUF);

        /*@ free the page */
        e = BfM_FreeTrain(root, PAGE_BUF);
        if (e < 0) ERR(e);
	
    } else if (apage->any.hdr.type & LEAF) {
        /* Search the leaf item which has the given key value. */
        found = btm_BinarySearchLeaf(&(apage->bl), kdesc, startKval, &idx);

        leafPid = root;		/* set the current leaf page to root */
        slotNo = idx;		/* set the current slotNo to idx */
        
        switch(startCompOp) {
        case SM_EQ:
            if (!found) {
                e = BfM_FreeTrain(root, PAGE_BUF);
                if (e < 0) ERR(e);

                cursor->flag = CURSOR_EOS;
                return(eNOERROR);
            }
            break;

        case SM_LT:
        case SM_LE:
            if (startCompOp == SM_LT && found) /* use the left slot */
                slotNo--;
            
            if (slotNo < 0) {
                /* use the left page */
                /*@ get the page id */
                MAKE_PAGEID(prevPid, root->volNo, apage->bl.hdr.prevPage);

                /*@ free the page */
                e = BfM_FreeTrain(root, PAGE_BUF);
                if (e < 0) ERR(e);
                
                if (prevPid.pageNo == NIL) {
                    cursor->flag = CURSOR_EOS;
                    return(eNOERROR);
                }
                
                /* read the left page */
                /*@ get the page */
                e = BfM_GetTrain(&prevPid, (char **)&apage, PAGE_BUF);
                if (e < 0) ERR(e);
                
                slotNo = apage->bl.hdr.nSlots - 1;
                leafPid = &prevPid;
            }

            break;

        case SM_GT:
        case SM_GE:
            if (startCompOp == SM_GT || !found) {
                /* use the right slot */
                slotNo++;

                if (slotNo >= apage->bl.hdr.nSlots) {		
                    /* use the right page */
                    
                    MAKE_PAGEID(nextPid, root->volNo, apage->bl.hdr.nextPage);
                    
                    e = BfM_FreeTrain(root, PAGE_BUF);
                    if (e < 0) ERR(e);
                    
                    if (nextPid.pageNo == NIL) {
                        cursor->flag = CURSOR_EOS;
                        return(eNOERROR);
                    }
                    
                    /* read the right page */
                    /*@ get the page */
                    e = BfM_GetTrain(&nextPid, (char**)&apage, PAGE_BUF);
                    if (e < 0) ERR(e);
                    
                    slotNo = 0;
                    leafPid = &nextPid;
                }
            }
            
            break;

        default:
            ERRB1(eBADCOMPOP_BTM, root, PAGE_BUF);
        }

        /* Construct a cursor for successive access */
        cursor->leaf = *leafPid;
        cursor->slotNo = slotNo;

        lEntryOffset = apage->bl.slot[-(cursor->slotNo)];
        lEntry = (btm_LeafEntry*)&(apage->bl.data[lEntryOffset]);
        alignedKlen = ALIGNED_LENGTH(lEntry->klen);

        cursor->key.len = lEntry->klen;
        memcpy(&(cursor->key.val[0]), &(lEntry->kval[0]), cursor->key.len);

        if (stopCompOp != SM_BOF && stopCompOp != SM_EOF) {
            cmp = btm_KeyCompare(kdesc, &cursor->key, stopKval);

            if (cmp == EQUAL && (stopCompOp == SM_LT || stopCompOp == SM_GT) ||
            cmp == LESS && (stopCompOp == SM_GT || stopCompOp == SM_GE) ||
            cmp == GREAT && (stopCompOp == SM_LT || stopCompOp == SM_LE)) {

                cursor->flag = CURSOR_EOS;

                e = BfM_FreeTrain(leafPid, PAGE_BUF);
                if (e < 0) ERR(e);
                
                return(eNOERROR);
            }	    
        }
        
        /* a normal entry */
        oidArray = (ObjectID*)&(lEntry->kval[alignedKlen]);

        if (startCompOp == SM_LT || startCompOp == SM_LE)
            cursor->oidArrayElemNo = lEntry->nObjects - 1;
        else
            cursor->oidArrayElemNo = 0;

        MAKE_PAGEID(cursor->overflow, root->volNo, NIL);
        cursor->oid = oidArray[cursor->oidArrayElemNo];
            
        /*@ free the page */
        e = BfM_FreeTrain(leafPid, PAGE_BUF);
        if (e < 0) ERR(e);
        
        /* The B+ tree cursor is valid. */
        cursor->flag = CURSOR_ON;
    } else {

	    ERRB1(eBADBTREEPAGE_BTM, root, PAGE_BUF); 
    }


    return(eNOERROR);
    
} /* edubtm_Fetch() */

