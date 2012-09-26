/*
 * (C) 1995-2001 Clemson University and Argonne National Laboratory.
 *
 * See COPYING in top-level directory.
 */


#include "llist.h"
#include "scheduler_SFQD.h"
#include "scheduler_DSFQ.h"
#include "scheduler_main.h"
#include "scheduler_2LSFQD.h"
/* PINT_llist_new() - returns a pointer to an empty list
 */
PINT_llist_p PINT_llist_new(
    void)
{
    PINT_llist_p l_p;

    if (!(l_p = (PINT_llist_p) malloc(sizeof(PINT_llist))))
	return (NULL);
    l_p->next = l_p->item = NULL;
    return (l_p);
}
int list_req_comp(void * a, void * b)
{
	return !(
			((long int)a)
			==((struct generic_queue_item *)b)->item_id
	);
}
int list_sfqd_sort_comp(PINT_llist * a, PINT_llist * b)//used for sfqd item only!
{
	//a is an int,
	//b is an item
	struct generic_queue_item* generic_item_a = (struct generic_queue_item *) (a->item);
	struct generic_queue_item* generic_item_b = (struct generic_queue_item *) (b->item);
	struct sfqd_queue_item* s_a = (struct sfqd_queue_item *) (generic_item_a->embedded_queue_item);
	struct sfqd_queue_item* s_b = (struct sfqd_queue_item *) (generic_item_b->embedded_queue_item);

	//ordered first by tag for inter-app barrier
	//then by queue_item_id by natural coming order

	return s_a->start_tag-s_b->start_tag;

}

int list_dsfq_sort_comp(PINT_llist * a, PINT_llist * b)//used for dsfq item only!
{
	//a is an int,
	//b is an item
	struct generic_queue_item* generic_item_a = (struct generic_queue_item *) (a->item);
	struct generic_queue_item* generic_item_b = (struct generic_queue_item *) (b->item);
	struct dsfq_queue_item* dsfq_item_a = (struct dsfq_queue_item *) (generic_item_a->embedded_queue_item);
	struct dsfq_queue_item* dsfq_item_b = (struct dsfq_queue_item *) (generic_item_b->embedded_queue_item);

	return dsfq_item_a->start_tag-dsfq_item_b->start_tag;
}

int list_vdsfq_sort_comp(PINT_llist * a, PINT_llist * b)//used for dsfq item only!
{
	//a is an int,
	//b is an item
	struct generic_queue_item* generic_item_a = (struct generic_queue_item *) (a->item);
	struct generic_queue_item* generic_item_b = (struct generic_queue_item *) (b->item);
	struct vdsfq_queue_item* vdsfq_item_a = (struct vdsfq_queue_item *) (generic_item_a->embedded_queue_item);
	struct vdsfq_queue_item* vdsfq_item_b = (struct vdsfq_queue_item *) (generic_item_b->embedded_queue_item);

	return vdsfq_item_a->start_tag-vdsfq_item_b->start_tag;
}

int list_edf_sort_comp(PINT_llist * a, PINT_llist * b)//used for edf item only!
{
	//a is an int,
	//b is an item
	struct generic_queue_item* generic_item_a = (struct generic_queue_item *) (a->item);
	struct generic_queue_item* generic_item_b = (struct generic_queue_item *) (b->item);
	struct twolsfqd_queue_item* edf_item_a = (struct twolsfqd_queue_item *) (generic_item_a->embedded_queue_item);
	struct twolsfqd_queue_item* edf_item_b = (struct twolsfqd_queue_item *) (generic_item_b->embedded_queue_item);

	if ((edf_item_a->deadline).tv_sec < (edf_item_b->deadline).tv_sec)
	{
		return -1;
	}
	else if ((edf_item_a->deadline).tv_sec > (edf_item_b->deadline).tv_sec)
	{
		return 1;
	}
	else
	{
		return (edf_item_a->deadline).tv_usec-(edf_item_b->deadline).tv_usec;
	}


}

int list_resp_sort_comp(PINT_llist * a, PINT_llist * b)//used for edf item only!
{
	//a is an int,
	//b is an item
	struct resp_time* resp_item_a = (struct resp_time *) (a->item);
	struct resp_time* resp_item_b = (struct resp_time *) (b->item);

	return resp_item_a->resp_time - resp_item_b->resp_time;

}

/*
 * This is the actual sort function. Notice that it returns the new
 * head of the list. (It has to, because the head will not
 * generally be the same element after the sort.) So unlike sorting
 * an array, where you can do
 *
 *     sort(myarray);
 *
 * you now have to do
 *
 *     list = listsort(mylist);
 */
PINT_llist_p PINT_llist_sort(PINT_llist_p list, int (* list_comp) (PINT_llist * a, PINT_llist * b))
{
	PINT_llist_p p, q, e, tail, oldhead, temp_head;

    int insize, nmerges, psize, qsize, i;

    if (!list)
    {
    	//fprintf(stderr,"null\n");
    	return NULL;
    }
	temp_head=list;
	list=list->next;
    insize = 1;
    if (list==NULL)
    {
    	//fprintf(stderr,"null 2\n");
    	return NULL;
    }

    while (1) {
        p = list;
        oldhead = list;		       /* only used for circular linkage */
        list = NULL;
        tail = NULL;

        nmerges = 0;  /* count number of merges we do in this pass */

        while (p) {
            nmerges++;  /* there exists a merge to be done */
            /* step `insize' places along from p */
            q = p;
            psize = 0;
            for (i = 0; i < insize; i++) {
                psize++;
                q = q->next;
                if (!q) break;
            }

            /* if q hasn't fallen off end, we have two lists to merge */
            qsize = insize;

            /* now we have two lists; merge them */
            while (psize > 0 || (qsize > 0 && q)) {

                /* decide whether next element of merge comes from p or q */
                if (psize == 0) {
                	/* p is empty; e must come from q. */
                	e = q; q = q->next; qsize--;

                	} else if (qsize == 0 || !q) {
                		/* q is empty; e must come from p. */
                		e = p; p = p->next; psize--;

					} else if ((*list_comp)(p,q) <= 0) {
						/* First element of p is lower (or same);
						 * e must come from p. */
						e = p; p = p->next; psize--;

					} else {
						/* First element of q is lower; e must come from q. */
						e = q; q = q->next; qsize--;
					}

                /* add the next element to the merged list */
				if (tail) {
					tail->next = e;
				} else {
					list = e;
				}

				tail = e;
            }

		/* now p has stepped `insize' places along, and q has too */
            p = q;
        }

	    tail->next = NULL;

        /* If we have done only one merge, we're finished. */
        if (nmerges <= 1)   /* allow for nmerges==0, the empty list case */
        {
        	temp_head->next=list;
            return temp_head;
        }
        /* Otherwise repeat, merging lists twice the size */
        insize *= 2;
    }

}

/* PINT_llist_empty() - determines if a list is empty
 *
 * Returns 0 if not empty, 1 if empty
 */
int PINT_llist_empty(
    PINT_llist_p l_p)
{
    if (l_p->next == NULL)
	return (1);
    return (0);
}

/* PINT_llist_add_to_tail() - adds an item to a list
 *
 * Requires that a list have already been created
 * Puts item at tail of list
 * Returns 0 on success, -1 on failure
 */
int PINT_llist_add_to_tail(
    PINT_llist_p l_p,
    void *item)
{
    PINT_llist_p new_p;

    if (!l_p)	/* not a list */
    {
    	fprintf(stderr,"not a list\n");//"list:%#010x\n",l_p);
    	return (-1);

    }
    /* NOTE: first "item" pointer in list is _always_ NULL */

    if ((new_p = (PINT_llist_p) malloc(sizeof(PINT_llist))) == NULL)
	return -1;
    new_p->next = NULL;
    new_p->item = item;
    while (l_p->next)
	l_p = l_p->next;
    l_p->next = new_p;
    return (0);
}

/* PINT_llist_add_to_head() - adds an item to a list
 *
 * Requires that a list have already been created
 * Puts item at head of list
 * Returns 0 on success, -1 on failure
 */
int PINT_llist_add_to_head(
    PINT_llist_p l_p,
    void *item)
{
    PINT_llist_p new_p;

    if (!l_p)	/* not a list */
	return (-1);

    /* NOTE: first "item" pointer in list is _always_ NULL */

    if ((new_p = (PINT_llist_p) malloc(sizeof(PINT_llist))) == NULL)
	return -1;
    new_p->next = l_p->next;
    new_p->item = item;
    l_p->next = new_p;
    return (0);
}

/* PINT_llist_head() - returns a pointer to the item at the head of the
 * list
 *
 * Returns NULL on error or if no items are in list
 */
void *PINT_llist_head(
    PINT_llist_p l_p)
{
    if (!l_p || !l_p->next)
	return (NULL);
    return (l_p->next->item);
}

/* PINT_llist_tail() - returns pointer to the item at the tail of the list
 * 
 * Returns NULL on error or if no items are in list
 */
void *PINT_llist_tail(
    PINT_llist_p l_p)
{
    if (!l_p || !l_p->next)
	return (NULL);
    while (l_p->next)
	l_p = l_p->next;
    return (l_p->item);
}

/* PINT_llist_search() - finds first match from list and returns pointer
 *
 * Returns NULL on error or if no match made
 * Returns pointer to item if found
 */
void *PINT_llist_search(
    PINT_llist_p l_p,
    void *key,
    int (*comp) (void *,
		 void *))
{
    if (!l_p || !l_p->next || !comp)	/* no or empty list */
	return (NULL);

    for (l_p = l_p->next; l_p; l_p = l_p->next)
    {
	/* NOTE: "comp" function must return _0_ if a match is made */
	if (!(*comp) (key, l_p->item))
	    return (l_p->item);
    }
    return (NULL);
}

/* PINT_llist_rem() - removes first match from list
 *
 * Returns NULL on error or not found, or a pointer to item if found
 * Removes item from list, but does not attempt to free memory
 *   allocated for item
 */
void *PINT_llist_rem(
    PINT_llist_p l_p,
    void *key,
    int (*comp) (void *,
		 void *))
{
    if (!l_p || !l_p->next || !comp)	/* no or empty list */
	return (NULL);

    for (; l_p->next; l_p = l_p->next)
    {
	/* NOTE: "comp" function must return _0_ if a match is made */
	if (!(*comp) (key, l_p->next->item))
	{
	    void *i_p = l_p->next->item;
	    PINT_llist_p rem_p = l_p->next;

	    l_p->next = l_p->next->next;
	    free(rem_p);
	    return (i_p);
	}
    }
    return (NULL);
}

/* PINT_llist_count()
 *
 * counts items in the list
 * NOTE: this is a slow count- it works by iterating through the whole
 * list
 *
 * returns count on success, -errno on failure
 */
int PINT_llist_count(
    PINT_llist_p l_p)
{
    int count = 0;

    if (!l_p)
	return (-1);

    for (l_p = l_p->next; l_p; l_p = l_p->next)
    {
	count++;
    }

    return (count);
}

/* PINT_llist_doall() - passes through list calling function "fn" on all 
 *    items in the list
 *
 * Returns -1 on error, 0 on success
 */
int PINT_llist_doall(
    PINT_llist_p l_p,
    int (*fn) (void *))
{
    PINT_llist_p tmp_p;

    if (!l_p || !l_p->next || !fn)
	return (-1);
    for (l_p = l_p->next; l_p;)
    {
	tmp_p = l_p->next;	/* save pointer to next element in case the
				 * function destroys the element pointed to
				 * by l_p...
				 */
	(*fn) (l_p->item);
	l_p = tmp_p;
    }
    return (0);
}

/* PINT_llist_doall_arg() - passes through list calling function "fn" on all 
 *    items in the list; passes through an argument to the function
 *
 * Returns -1 on error, 0 on success
 */
int PINT_llist_doall_arg(
    PINT_llist_p l_p,
    int (*fn) (void *item,
	       void *arg),
    void *arg)
{
    PINT_llist_p tmp_p;

    if (!l_p || !l_p->next || !fn)
	return (-1);
    for (l_p = l_p->next; l_p;)
    {
	tmp_p = l_p->next;	/* save pointer to next element in case the
				 * function destroys the element pointed to
				 * by l_p...
				 */
	(*fn) (l_p->item, arg);
	l_p = tmp_p;
    }
    return (0);
}

/* PINT_llist_free() - frees all memory associated with a list
 *
 * Relies on passed function to free memory for an item
 */
void PINT_llist_free(
    PINT_llist_p l_p,
    void (*fn) (void *))
{
    PINT_llist_p tmp_p;

    if (!l_p || !fn)
	return;

    /* There is never an item in first entry */
    tmp_p = l_p;
    l_p = l_p->next;
    free(tmp_p);
    while (l_p)
    {
	(*fn) (l_p->item);
	tmp_p = l_p;
	l_p = l_p->next;
	free(tmp_p);
    }
}

/*
 * PINT_llist_next()
 * 
 * returns the next list entry in the list.  WARNING- use this function
 * carefully- it is sortof a hack around the interface.
 *
 * returns a pointer to the next entry on success, NULL on end or
 * failure.
 */
PINT_llist_p PINT_llist_next(
    PINT_llist_p entry)
{

    if (!entry)
    {
	return (NULL);
    }

    return (entry->next);
}


/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 * End:
 */
