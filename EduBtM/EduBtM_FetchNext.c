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
 * Module: EduBtM_FetchNext.c
 *
 * Description:
 *  Find the next ObjectID satisfying the given condition. The current ObjectID
 *  is specified by the 'current'.
 *
 * Exports:
 *  Four EduBtM_FetchNext(PageID*, KeyDesc*, KeyValue*, Four, BtreeCursor*, BtreeCursor*)
 */


#include <string.h>
#include "EduBtM_common.h"
#include "BfM.h"
#include "EduBtM_Internal.h"


/*@ Internal Function Prototypes */
Four edubtm_FetchNext(KeyDesc*, KeyValue*, Four, BtreeCursor*, BtreeCursor*);

/*@================================
 * EduBtM_FetchNext()
 *================================*/
/*
 * Function: Four EduBtM_FetchNext(PageID*, KeyDesc*, KeyValue*,
 *                              Four, BtreeCursor*, BtreeCursor*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  Fetch the next ObjectID satisfying the given condition.
 * By the B+ tree structure modification resulted from the splitting or merging
 * the current cursor may point to the invalid position. So we should adjust
 * the B+ tree cursor before using the cursor.
 *
 * Returns:
 *  error code
 *    eBADPARAMETER_BTM
 *    eBADCURSOR
 *    some errors caused by function calls
 */
Four EduBtM_FetchNext(
    PageID                      *root,          /* IN root page's PageID */
    KeyDesc                     *kdesc,         /* IN key descriptor */
    KeyValue                    *kval,          /* IN key value of stop condition */
    Four                        compOp,         /* IN comparison operator of stop condition */
    BtreeCursor                 *current,       /* IN current B+ tree cursor */
    BtreeCursor                 *next)          /* OUT next B+ tree cursor */
{
    int							i;
    Four                        e;              /* error number */
    Four                        cmp;            /* comparison result */
    Two                         slotNo;         /* slot no. of a leaf page */
    Two                         oidArrayElemNo; /* element no. of the array of ObjectIDs */
    Two                         alignedKlen;    /* aligned length of key length */
    PageID                      overflow;       /* temporary PageID of an overflow page */
    Boolean                     found;          /* search result */
    ObjectID                    *oidArray;      /* array of ObjectIDs */
    BtreeLeaf                   *apage;         /* pointer to a buffer holding a leaf page */
    BtreeOverflow               *opage;         /* pointer to a buffer holding an overflow page */
    btm_LeafEntry               *entry;         /* pointer to a leaf entry */
    BtreeCursor                 tCursor;        /* a temporary Btree cursor */
  
    
    /*@ chesck parameter */
    if (root == NULL || kdesc == NULL || kval == NULL || current == NULL || next == NULL)
	ERR(eBADPARAMETER_BTM);
    
    /* Is the current cursor valid? */
    if (current->flag != CURSOR_ON && current->flag != CURSOR_EOS)
		ERR(eBADCURSOR);
    
    if (current->flag == CURSOR_EOS) return(eNOERROR);
    
    /* Error check whether using not supported functionality by EduBtM */
    for(i=0; i<kdesc->nparts; i++)
    {
        if(kdesc->kpart[i].type!=SM_INT && kdesc->kpart[i].type!=SM_VARSTRING)
            ERR(eNOTSUPPORTED_EDUBTM);
    }

    /*@ Copy the current cursor to the next cursor. */
    *next = *current;
    
    e = BfM_GetTrain(&next->leaf, (char**)&apage, PAGE_BUF);
    if (e < 0) ERR(e);

    
    /*@ Adjust the slot no. */
    found = FALSE;
    if (next->slotNo >= 0 && next->slotNo < apage->hdr.nSlots) {
        entry = (btm_LeafEntry*)&apage->data[apage->slot[-next->slotNo]];
            
        if (btm_KeyCompare(kdesc, (KeyValue*)&entry->klen, &current->key) == EQUAL)
            found = TRUE;
    }


    if (!found) {		/*@ cannot find the current cursor's key */
        /* The current cursor's key has been deleted. */
        /* 'next->slotNo' points to the slot with a key less than the current cursor's key. */
        
        switch(compOp) {
        case SM_EQ:
            next->flag = CURSOR_EOS;
            break;
            
        case SM_LT:		/* forward scan */
        case SM_LE:
        case SM_EOF:
            next->slotNo++;
            entry = (btm_LeafEntry*)&(apage->data[apage->slot[-next->slotNo]]);
            alignedKlen = ALIGNED_LENGTH(entry->klen);
            
            if (compOp != SM_EOF) { 
                /* Check the boundary condition. */
                cmp = btm_KeyCompare(kdesc, (KeyValue*)&entry->klen, kval);
                if (cmp == GREAT || (cmp == EQUAL && compOp == SM_LT)) {
                    next->flag = CURSOR_EOS;
                    break;
                }
            }
            
            if (entry->nObjects < 0) { /* overflow page */
                MAKE_PAGEID(next->overflow, next->leaf.volNo,
                        *((ShortPageID*)&entry->kval[alignedKlen]));
                
                e = BfM_GetTrain(&next->overflow, (char**)&opage, PAGE_BUF);
                if (e < 0) ERRB1(e, &next->leaf, PAGE_BUF);
                
                next->oidArrayElemNo = 0; /*@ set oidArrayElemNo to 0.  */
                next->oid = opage->oid[next->oidArrayElemNo];
                
                e = BfM_FreeTrain(&next->overflow, PAGE_BUF);
                if (e < 0) ERRB1(e, &next->leaf, PAGE_BUF);
            
            } else {		       /* normal entry */
                oidArray = (ObjectID*)&(entry->kval[alignedKlen]);
                
                next->oidArrayElemNo = 0; /*@ set oidArrayElemNo to 0. */
                next->oid = oidArray[0];
                MAKE_PAGEID(next->overflow, next->leaf.volNo, NIL);
            }
            
            next->flag = CURSOR_ON;
            break;
            
        case SM_GT:		/* backward scan */
        case SM_GE:
        case SM_BOF:
            entry = (btm_LeafEntry*)&(apage->data[apage->slot[-next->slotNo]]);
            alignedKlen = ALIGNED_LENGTH(entry->klen);
            
            if (compOp != SM_BOF) {
                /* Check the boundary condition. */
                cmp = btm_KeyCompare(kdesc, (KeyValue*)&entry->klen, kval);
                if (cmp == LESS || (cmp == EQUAL && compOp == SM_GT)) {
                    next->flag = CURSOR_EOS;
                    break;
                }
            }
            
            /* normal entry */
            oidArray = (ObjectID*)&(entry->kval[alignedKlen]);
            
            next->oidArrayElemNo = entry->nObjects - 1;
            next->oid = oidArray[next->oidArrayElemNo];
            MAKE_PAGEID(next->overflow, next->leaf.volNo, NIL);
            
            next->flag = CURSOR_ON;
            break;
        } /* end of switch */

        if (next->flag == CURSOR_ON)
            memcpy((char*)&next->key, (char*)&entry->klen, entry->klen+sizeof(Two));
        
        e = BfM_FreeTrain(&next->leaf, PAGE_BUF);
        if (e < 0) ERR(e);
        return(eNOERROR);
    }
    
    /* At this point, the slot no. is correct. */
    entry = (btm_LeafEntry*)&(apage->data[apage->slot[-next->slotNo]]);
    alignedKlen = ALIGNED_LENGTH(entry->klen);
    

    /* normal entry */
    oidArray = (ObjectID*)&entry->kval[alignedKlen];
    found = btm_BinarySearchOidArray(oidArray, &current->oid, entry->nObjects, &oidArrayElemNo);
    
    if (compOp == SM_EQ || compOp == SM_LT || compOp == SM_LE || compOp == SM_EOF) /* forward scan */
        next->oidArrayElemNo = oidArrayElemNo;
    else			/* SM_GT or SM_GE: backward scan */
        next->oidArrayElemNo = (found) ? oidArrayElemNo:(oidArrayElemNo+1);
    
    MAKE_PAGEID(next->overflow, next->leaf.volNo, NIL);

    /*@ free the page */
    /* Notice: next->leaf may be changed in btm_FetchNext(). */
    e = BfM_FreeTrain(&next->leaf, PAGE_BUF);
    if (e < 0) ERR(e);
    
    tCursor = *next;
    e = btm_FetchNext(kdesc, kval, compOp, &tCursor, next);
    if (e < 0) ERR(e);
    
    
    return(eNOERROR);
    
    
} /* EduBtM_FetchNext() */



/*@================================
 * edubtm_FetchNext()
 *================================*/
/*
 * Function: Four edubtm_FetchNext(KeyDesc*, KeyValue*, Four,
 *                              BtreeCursor*, BtreeCursor*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  Get the next item. We assume that the current cursor is valid; that is.
 *  'current' rightly points to an existing ObjectID.
 *
 * Returns:
 *  Error code
 *    eBADCOMPOP_BTM
 *    some errors caused by function calls
 */
Four edubtm_FetchNext(
    KeyDesc  		*kdesc,		/* IN key descriptor */
    KeyValue 		*kval,		/* IN key value of stop condition */
    Four     		compOp,		/* IN comparison operator of stop condition */
    BtreeCursor 	*current,	/* IN current cursor */
    BtreeCursor 	*next)		/* OUT next cursor */
{
    Four 		e;		/* error number */
    Four 		cmp;		/* comparison result */
    Two 		alignedKlen;	/* aligned length of a key length */
    PageID 		leaf;		/* temporary PageID of a leaf page */
    PageID 		overflow;	/* temporary PageID of an overflow page */
    ObjectID 		*oidArray;	/* array of ObjectIDs */
    BtreeLeaf 		*apage;		/* pointer to a buffer holding a leaf page */
    BtreeOverflow 	*opage;		/* pointer to a buffer holding an overflow page */
    btm_LeafEntry 	*entry;		/* pointer to a leaf entry */    
    
    
    /* Error check whether using not supported functionality by EduBtM */
    int i;
    for(i=0; i<kdesc->nparts; i++)
    {
        if(kdesc->kpart[i].type!=SM_INT && kdesc->kpart[i].type!=SM_VARSTRING)
            ERR(eNOTSUPPORTED_EDUBTM);
    }

    /*@ Copy the current cursor to the next cursor. */
    *next = *current;

    if (compOp == SM_EQ || compOp == SM_LT || compOp == SM_LE || compOp == SM_EOF) { /* forward scan */

        if (IS_NILPAGEID(next->overflow)) { /* normal entry */
            /*@ get the page */
            e = BfM_GetTrain(&next->leaf, (char**)&apage, PAGE_BUF);
            if (e < 0) ERR(e);

            entry = (btm_LeafEntry*)&(apage->data[apage->slot[-next->slotNo]]);

            /* Go to the right ObjectID. */
            next->oidArrayElemNo++;

            if (next->oidArrayElemNo < entry->nObjects) {
                oidArray = (ObjectID*)&entry->kval[ALIGNED_LENGTH(entry->klen)];
                next->oid = oidArray[next->oidArrayElemNo];
                next->flag = CURSOR_ON;

                /*@ free the page */
                e = BfM_FreeTrain(&next->leaf, PAGE_BUF);
                if (e < 0) ERR(e);

                return(eNOERROR);
            }
        } 

        /* At this point, notice that we should move to the right slot. */

        if (compOp == SM_EQ) {
            next->flag = CURSOR_EOS;

            /*@ free the page */
            e = BfM_FreeTrain(&next->leaf, PAGE_BUF);
            if (e < 0) ERR(e);

            return(eNOERROR);
        }

        /* Go to the right leaf entry. */
        next->slotNo++;

        if (next->slotNo >= apage->hdr.nSlots && apage->hdr.nextPage != NIL) {
            /* Go to the right leaf page. */
            MAKE_PAGEID(leaf, next->leaf.volNo, apage->hdr.nextPage);
            next->leaf = leaf;
            next->slotNo = 0;

            /*@ free the page */
            e = BfM_FreeTrain(&next->leaf, PAGE_BUF);
            if (e < 0) ERR(e);

            /*@ get the page */
            e = BfM_GetTrain(&next->leaf, (char**)&apage, PAGE_BUF);
            if (e < 0) ERR(e);
        }

        if (next->slotNo < apage->hdr.nSlots) {
            entry = (btm_LeafEntry*)&(apage->data[apage->slot[-next->slotNo]]);
            alignedKlen = ALIGNED_LENGTH(entry->klen);

            if (compOp != SM_EOF) {
                /* Check the boundary condition. */
                cmp = btm_KeyCompare(kdesc, (KeyValue*)&entry->klen, kval);
                if (cmp == GREAT || (compOp == SM_LT && cmp == EQUAL)) {
                    /*@ free the page */
                    e = BfM_FreeTrain(&next->leaf, PAGE_BUF);
                    if (e < 0) ERR(e);

                    next->flag = CURSOR_EOS;
                    return(eNOERROR);
                }
            }

            /* normal entry */
            MAKE_PAGEID(next->overflow, next->leaf.volNo, NIL);
            next->oidArrayElemNo = 0;
            next->oid = *((ObjectID*)&entry->kval[alignedKlen]);
            

            memcpy((char*)&next->key, (char*)&entry->klen, entry->klen + sizeof(Two));
            next->flag = CURSOR_ON;

        } else {
            next->flag = CURSOR_EOS;
        }

        next->flag = CURSOR_EOS;

        /*@ free the page */
        e = BfM_FreeTrain(&next->leaf, PAGE_BUF);
        if (e < 0) ERR(e);

    } 
    else { /* SM_GT, SM_GE, or SM_BOF : backward scan */

        if (IS_NILPAGEID(next->overflow)) { /* normal entry */
            /*@ get the page */
            e = BfM_GetTrain(&next->leaf, (char**)&apage, PAGE_BUF);
            if (e < 0) ERR(e);

            entry = (btm_LeafEntry*)&apage->data[apage->slot[-next->slotNo]];

            /* Go to the left ObjectID. */
            next->oidArrayElemNo--;

            if (next->oidArrayElemNo >= 0) {
                oidArray = (ObjectID*)&entry->kval[ALIGNED_LENGTH(entry->klen)];
                next->oid = oidArray[next->oidArrayElemNo];
                next->flag = CURSOR_ON;

                /*@ free the page */
                e = BfM_FreeTrain(&next->leaf, PAGE_BUF);
                if (e < 0) ERR(e);

                return(eNOERROR);
            }
        } 

        /* At this point, notice that we should move to the left slot. */

        /* Go to the left leaf entry. */
        next->slotNo--;

        if (next->slotNo < 0 && apage->hdr.prevPage != NIL) {
            /* Go to the left leaf page. */
            MAKE_PAGEID(leaf, next->leaf.volNo, apage->hdr.prevPage);
            next->leaf = leaf;
            next->slotNo = apage->hdr.nSlots - 1; /* last slot */

            /*@ free the page */
            e = BfM_FreeTrain(&next->leaf, PAGE_BUF);
            if (e < 0) ERR(e);

            /*@ get the page */
            e = BfM_GetTrain(&next->leaf, (char**)&apage, PAGE_BUF);
            if (e < 0) ERR(e);
        }

        if (next->slotNo >= 0) {
            entry = (btm_LeafEntry*)&(apage->data[apage->slot[-next->slotNo]]);
            alignedKlen = ALIGNED_LENGTH(entry->klen);

            if (compOp != SM_BOF) {
                /* Check the boundary condition. */
                cmp = btm_KeyCompare(kdesc, (KeyValue*)&entry->klen, kval);
                if (cmp == LESS || (compOp == SM_GT && cmp == EQUAL)) {
                    /*@ free the page */
                    e = BfM_FreeTrain(&next->leaf, PAGE_BUF);
                    if (e < 0) ERR(e);

                    next->flag = CURSOR_EOS;
                    return(eNOERROR);
                }
            }
            
            /* normal entry */
            MAKE_PAGEID(next->overflow, next->leaf.volNo, NIL);
            next->oidArrayElemNo = entry->nObjects - 1;
            oidArray =(ObjectID*)&entry->kval[alignedKlen];
            next->oid = oidArray[next->oidArrayElemNo];
            
            memcpy((char*)&next->key, (char*)&entry->klen, entry->klen + sizeof(Two));	    
            next->flag = CURSOR_ON;

        } else {
            next->flag = CURSOR_EOS;
        }

        /*@ free the page */
        e = BfM_FreeTrain(&next->leaf, PAGE_BUF);
        if (e < 0) ERR(e);
    }

    return(eNOERROR);

    
} /* edubtm_FetchNext() */


