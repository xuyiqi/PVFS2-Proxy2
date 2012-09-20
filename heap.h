/* heap.h -- Binomial Heaps
 *
 * Copyright (c) 2008, Bjoern B. Brandenburg <bbb [at] cs.unc.edu>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of North Carolina nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY  COPYRIGHT OWNER AND CONTRIBUTERS ``AS IS'' AND
 * ANY  EXPRESS OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT  LIMITED TO,  THE
 * IMPLIED WARRANTIES  OF MERCHANTABILITY AND FITNESS FOR  A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO  EVENT SHALL THE  COPYRIGHT OWNER OR  CONTRIBUTERS BE
 * LIABLE  FOR  ANY  DIRECT,   INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
 * CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT  LIMITED  TO,  PROCUREMENT  OF
 * SUBSTITUTE GOODS  OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
 * INTERRUPTION)  HOWEVER CAUSED  AND ON  ANY THEORY  OF LIABILITY,  WHETHER IN
 * CONTRACT,  STRICT LIABILITY,  OR  TORT (INCLUDING  NEGLIGENCE OR  OTHERWISE)
 * ARISING IN ANY WAY  OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HEAP_H
#define HEAP_H

#define NOT_IN_HEAP UINT_MAX

struct heap_node {
	struct heap_node* 	parent;
	struct heap_node* 	next;
	struct heap_node* 	child;

	unsigned int 		degree;
	void*			value;
	struct heap_node**	ref;
};

struct heap {
	struct heap_node* 	head;
	/* We cache the minimum of the heap.
	 * This speeds up repeated peek operations.
	 */
	struct heap_node*	min;
	int* count;
	int all_count;
};

/* item comparison function:
 * return 1 if a has higher prio than b, 0 otherwise
 */
typedef int (*heap_prio_t)(struct heap_node* a, struct heap_node* b);

void heap_init(struct heap* heap);

void heap_node_init_ref(struct heap_node** _h, void* value);

void heap_node_init(struct heap_node* h, void* value);
void* heap_node_value(struct heap_node* h);

int heap_node_in_heap(struct heap_node* h);

int heap_empty(struct heap* heap);


/* insert (and reinitialize) a node into the heap */
void heap_insert(heap_prio_t higher_prio, struct heap* heap,
			       struct heap_node* node);

/* merge addition into target */
void heap_union(heap_prio_t higher_prio,
			      struct heap* target, struct heap* addition);


struct heap_node* heap_peek(heap_prio_t higher_prio,
					  struct heap* heap);

struct heap_node* heap_take(heap_prio_t higher_prio,
					  struct heap* heap);


void heap_decrease(heap_prio_t higher_prio, struct heap* heap,
				 struct heap_node* node);


void heap_delete(heap_prio_t higher_prio, struct heap* heap,
			       struct heap_node* node);

#endif /* HEAP_H */
