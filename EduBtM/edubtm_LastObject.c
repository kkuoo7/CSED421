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
 * Module: edubtm_LastObject.c
 *
 * Description : 
 *  Find the last ObjectID of the given Btree.
 *
 * Exports:
 *  Four edubtm_LastObject(PageID*, KeyDesc*, KeyValue*, Four, BtreeCursor*) 
 */


#include <string.h>
#include "EduBtM_common.h"
#include "BfM.h"
#include "EduBtM_Internal.h"



/*@================================
 * edubtm_LastObject()
 *================================*/
/*
 * Function:  Four edubtm_LastObject(PageID*, KeyDesc*, KeyValue*, Four, BtreeCursor*) 
 *
 * Description : 
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  Find the last ObjectID of the given Btree. The 'cursor' will indicate
 *  the last ObjectID in the Btree, and it will be used as successive access
 *  by using the Btree.
 *
 * Returns:
 *  error code
 *    eBADPAGE_BTM
 *    some errors caused by function calls
 *
 * Side effects:
 *  cursor : the last ObjectID and its position in the Btree
 */
Four edubtm_LastObject(
    PageID   		*root,		/* IN the root of Btree */
    KeyDesc  		*kdesc,		/* IN key descriptor */
    KeyValue 		*stopKval,	/* IN key value of stop condition */
    Four     		stopCompOp,	/* IN comparison operator of stop condition */
    BtreeCursor 	*cursor)	/* OUT the last BtreeCursor to be returned */
{
    int			i;
    Four 		e;		/* error number */
    Four 		cmp;		/* result of comparison */
    BtreePage 		*apage;		/* pointer to the buffer holding current page */
    BtreeOverflow 	*opage;		/* pointer to the buffer holding overflow page */
    PageID 		curPid;		/* PageID of the current page */
    PageID 		child;		/* PageID of the child page */
    PageID 		ovPid;		/* PageID of the current overflow page */
    PageID 		nextOvPid;	/* PageID of the next overflow page */
    Two 		lEntryOffset;	/* starting offset of a leaf entry */
    Two 		iEntryOffset;	/* starting offset of an internal entry */
    btm_LeafEntry 	*lEntry;	/* a leaf entry */
    btm_InternalEntry 	*iEntry;	/* an internal entry */
    Four 		alignedKlen;	/* aligned length of the key length */
        

    if (root == NULL) ERR(eBADPAGE_BTM);

    /* Error check whether using not supported functionality by EduBtM */
    for(i=0; i<kdesc->nparts; i++)
    {
        if(kdesc->kpart[i].type!=SM_INT && kdesc->kpart[i].type!=SM_VARSTRING)
            ERR(eNOTSUPPORTED_EDUBTM);
    }

    curPid = *root;
    
    e = BfM_GetTrain(&curPid, (char **)&apage, PAGE_BUF);
    if (e < 0)  ERR(e);

    /*@ Traverse the B+ tree until we reach the last leaf page. */
    while (apage->any.hdr.type & INTERNAL) {
	
        /* Get the last(right most) child of the given root page */
        iEntryOffset = apage->bi.slot[-(apage->bi.hdr.nSlots-1)];
        iEntry = (btm_InternalEntry*)&(apage->bi.data[iEntryOffset]);
            
        /* Recusively call itself to get the last ObjectID. */
        MAKE_PAGEID(child, curPid.volNo, iEntry->spid);

        /*@ Free the current page. */
        e = BfM_FreeTrain(&curPid, PAGE_BUF);
        if (e < 0) ERR(e);

        curPid = child;
        
        e = BfM_GetTrain(&curPid, (char **)&apage, PAGE_BUF);
        if (e < 0)  ERR(e);
        
    }

    /* From now, curPid is the PageID of the last leaf page. */
	
    if (apage->bl.hdr.nSlots == 0) {
     /* Assert that the page is the root page. */
    
        cursor->flag = CURSOR_EOS;
	
    } else {	    
        /* Get the last entry */
        lEntryOffset = apage->bl.slot[-(apage->bl.hdr.nSlots-1)];
        lEntry = (btm_LeafEntry*)&(apage->bl.data[lEntryOffset]);
        alignedKlen = ALIGNED_LENGTH(lEntry->klen);
        
        /*@ Construct 'cursor' */
        cursor->key.len = lEntry->klen;
        memcpy(&(cursor->key.val[0]), &(lEntry->kval[0]), cursor->key.len);
        
        if (stopCompOp != SM_BOF) { /* stopCompOp is one of SM_GE and SM_GT. */
            
            cmp = btm_KeyCompare(kdesc, &cursor->key, stopKval);

            if (cmp == LESS || (cmp == EQUAL && stopCompOp == SM_GT)) {
                cursor->flag = CURSOR_EOS;
                    
                e = BfM_FreeTrain(&curPid, PAGE_BUF);
                if (e < 0) ERR(e);

                return(eNOERROR);
            }
        }
        
        cursor->flag = CURSOR_ON;
        cursor->leaf = curPid;
        cursor->slotNo = apage->bl.hdr.nSlots - 1;
        
        if(lEntry->nObjects > 0) {  /* a normal leaf item */ /* 'less than' == 'greater than' */
            /* Get the last ObjectID of the leaf item */
            cursor->oidArrayElemNo = lEntry->nObjects - 1;
            MAKE_PAGEID(cursor->overflow, curPid.volNo, NIL);
            btm_get_objectid_from_leaf(cursor);
                   
        } 
    }
    
    e = BfM_FreeTrain(&curPid, PAGE_BUF);
    if (e < 0) ERR(e);
    

    return(eNOERROR);
    
} /* edubtm_LastObject() */
