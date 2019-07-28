/* To Professor YPCho!
 * This Code was writed in Virtual Studio
 * So when you look this code in Linux, there must be in collapsion in remark
 */

/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ChangPulMu",
    /* First member's full name */
    "ChangDuHyeok",
    /* First member's email address */
    "16906@naver.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WORDSIZE     4	
#define DOUBLEWORDSIZE     8		
#define LIST    20
#define MAX(x, y) ((x) > (y) ? (x) : (y)) 
#define MIN(x, y) ((x) < (y) ? (x) : (y)) 
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p)            (*(unsigned int *)(p))
#define PUT(p, val)       (*(unsigned int *)(p) = (val) | GET_TAG(p))
#define PUT_NOTAG(p, val) (*(unsigned int *)(p) = (val))
#define SET_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_TAG(p)   (GET(p) & 0x2)
#define SET_RATAG(p)   (GET(p) |= 0x2)
#define REMOVE_RATAG(p) (GET(p) &= ~0x2)
#define HDRP(ptr) ((char *)(ptr) - WORDSIZE)
#define FTRP(ptr) ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DOUBLEWORDSIZE)
#define NEXT_BLKP(ptr) ((char *)(ptr) + GET_SIZE((char *)(ptr) - WORDSIZE))
#define PREV_BLKP(ptr) ((char *)(ptr) - GET_SIZE((char *)(ptr) - DOUBLEWORDSIZE))
#define PRED_PTR(ptr) ((char *)(ptr))
#define SUCC_PTR(ptr) ((char *)(ptr) + WORDSIZE)
#define PRED(ptr) (*(char **)(ptr))
#define SUCC(ptr) (*(char **)(SUCC_PTR(ptr)))

void *free_lists[LIST];

static void *extend_heap(size_t size);
static void *coalesce(void *ptr);
static void *place(void *ptr, size_t asize);
static void insert_node(void *ptr, size_t size);
static void delete_node(void *ptr);


static void *extend_heap(size_t size)
{
	size_t temp = size;
	void * ptr = mem_sbrk(temp);

	if (ptr == (void *)-1)
		return NULL;

	PUT_NOTAG(HDRP(ptr), PACK(temp, 0));
	PUT_NOTAG(FTRP(ptr), PACK(temp, 0));
	PUT_NOTAG(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));

	insert_node(ptr, temp);

	return coalesce(ptr);
}

static void * coalesce(void * ptr)
{
	size_t prev = GET_ALLOC(HDRP(PREV_BLKP(ptr)));
	size_t next = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
	size_t size = GET_SIZE(HDRP(ptr));

	if (GET_TAG(HDRP(PREV_BLKP(ptr))) == 1)
		prev = 1;

	if (prev == 1 && next == 1)
		return ptr;

	if (prev == 1 && next == 0)
	{
		delete_node(ptr);
		delete_node(NEXT_BLKP(ptr));

		size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));

		PUT(HDRP(ptr), PACK(size, 0));
		PUT(FTRP(ptr), PACK(size, 0));
	}
	else if (prev == 0 && next == 1)
	{
		delete_node(ptr);
		delete_node(PREV_BLKP(ptr));
		size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
		ptr = PREV_BLKP(ptr);
		PUT(HDRP(ptr), PACK(size, 0));
		PUT(FTRP(ptr), PACK(size, 0));
	}
	else if (prev == 0 && next == 0)
	{
		delete_node(ptr);
		delete_node(PREV_BLKP(ptr));
		delete_node(NEXT_BLKP(ptr));

		size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(HDRP(NEXT_BLKP(ptr)));

		ptr = PREV_BLKP(ptr);
		PUT(HDRP(ptr), PACK(size, 0));
		PUT(FTRP(ptr), PACK(size, 0));
	}

	insert_node(ptr, size);
	return ptr;
}

static void * place(void * ptr, size_t asize)
{
	size_t size = GET_SIZE(HDRP(ptr));
	size_t remain = size - asize;

	delete_node(ptr);

	if (remain <= DOUBLEWORDSIZE * 2)
	{
		PUT(HDRP(ptr), PACK(size, 1));
		PUT(FTRP(ptr), PACK(size, 1));
	}

	else if (asize >= 100)
	{
		PUT(HDRP(ptr), PACK(remain, 0));
		PUT(FTRP(ptr), PACK(remain, 0));

		PUT_NOTAG(HDRP(NEXT_BLKP(ptr)), PACK(asize, 1));
		PUT_NOTAG(FTRP(NEXT_BLKP(ptr)), PACK(asize, 1));

		insert_node(ptr, remain);
		return NEXT_BLKP(ptr);
	}
	else
	{
		PUT(HDRP(ptr), PACK(asize, 1));
		PUT(FTRP(ptr), PACK(asize, 1));
		PUT_NOTAG(HDRP(NEXT_BLKP(ptr)), PACK(remain, 0));
		PUT_NOTAG(FTRP(NEXT_BLKP(ptr)), PACK(remain, 0));
		insert_node(NEXT_BLKP(ptr), remain);
	}
	return ptr;
}

static void insert_node(void * ptr, size_t size)
{
	int i;
	void *next = ptr;
	void *before = NULL;

	for (i = 0; i < LIST - 1; i++)
	{
		if (size > 1)
		{
			size = size >> 1;
		}
		else break;
	}
	next = free_lists[i];
	
	while (next != NULL && size < GET_SIZE(HDRP(next)))
	{
		before = next;
		next = PRED(next);
	}
	if (next != NULL)
	{
		if (before != NULL)
		{
			SET_PTR(PRED_PTR(ptr), next);
			SET_PTR(SUCC_PTR(next), ptr);
			SET_PTR(PRED_PTR(before), ptr);
			SET_PTR(SUCC_PTR(ptr), before);
		}
		else
		{
			SET_PTR(PRED_PTR(ptr), next);
			SET_PTR(SUCC_PTR(next), ptr);
			SET_PTR(SUCC_PTR(ptr), NULL);
			free_lists[i] = ptr;
		}
	}
	else
	{
		if (before != NULL)
		{
			SET_PTR(PRED_PTR(ptr), NULL);
			SET_PTR(SUCC_PTR(ptr), before);
			SET_PTR(PRED_PTR(before), ptr);
		}
		else
		{
			SET_PTR(PRED_PTR(ptr), NULL);
			SET_PTR(SUCC_PTR(ptr), NULL);
			free_lists[i] = ptr;
		}
	}
	return;

}

static void delete_node(void * ptr)
{
	int i;
	int size = GET_SIZE(HDRP(ptr));

	while ((i < LIST - 1) && (size >1))
	{
		size = size >> 1;
		i++;
	}

	if (PRED(ptr) != NULL)
	{
		if (SUCC(ptr) != NULL)
		{
			SET_PTR(SUCC_PTR(PRED(ptr)), SUCC(ptr));
			SET_PTR(PRED_PTR(SUCC(ptr)), PRED(ptr));
		}
		else
		{
			SET_PTR(SUCC_PTR(PRED(ptr)), NULL);
			free_lists[i] = PRED(ptr);
		}
	}
	else
	{
		if (SUCC(ptr) != NULL)
		{
			SET_PTR(PRED_PTR(SUCC(ptr)), NULL);
		}
		else
		{
			free_lists[i] = NULL;
		}
	}
	return;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	int i;
	char * heap;
	
	for (i = 0; i< LIST; i++)
		free_lists[i] = NULL;
	
	if ((long)(heap = mem_sbrk(4 * WORDSIZE)) == -1)
		return  -1;

	PUT_NOTAG(heap, 0);
	PUT_NOTAG(heap + 1 * WORDSIZE, PACK(DOUBLEWORDSIZE, 1));
	PUT_NOTAG(heap + 2 * WORDSIZE, PACK(DOUBLEWORDSIZE, 1));
	PUT_NOTAG(heap + 3 * WORDSIZE, PACK(0, 1));

	if (extend_heap(1<<6) == NULL)
		return -1;

	return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	if (size == 0)
		return NULL;

	size_t asize;
	size_t extend;
	void * ptr = NULL;

	if (size <= DOUBLEWORDSIZE)
	{
		asize = 2 * DOUBLEWORDSIZE;
	}
	else
	{
		asize = ALIGN(size + DOUBLEWORDSIZE);
	}

	int i = 0;
	size_t search = asize;

	while (i < LIST)
	{
		if ((i == LIST - 1) || (search <= 1 && free_lists[i] != NULL))
		{
			ptr = free_lists[i];
			
			while (ptr != NULL && ((asize > GET_SIZE(HDRP(ptr)) || GET_TAG(ptr))))
			{
				ptr = PRED(ptr);
			}

			if (ptr != NULL)
				break;
		}
		search = search >> 1;
		i++;
	}

	if (ptr == NULL)
	{
		extend = MAX(asize, 1<<12);
	
		ptr = extend_heap(extend);
		if (ptr == NULL)
			return NULL;
	}
	
	ptr = place(ptr, asize);

	return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	size_t size = GET_SIZE(HDRP(ptr));
	
	REMOVE_RATAG(HDRP(NEXT_BLKP(ptr)));

	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));

	insert_node(ptr, size);

	coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
	if (size == 0)
		return NULL;
	void * oldptr = ptr;
	size_t newsize = size;
	int remain;
	int extend;
	int blockbuff;

	if (size <= DOUBLEWORDSIZE)
	{
		newsize = 2 * DOUBLEWORDSIZE;
	}
	else
	{
		newsize = ALIGN(size + DOUBLEWORDSIZE);
	}

	newsize += (1<<7);

	blockbuff = GET_SIZE(HDRP(ptr)) - newsize;

	if (blockbuff < 0)
	{
		if (GET_ALLOC(HDRP(NEXT_BLKP(ptr))) == 0 || GET_SIZE(HDRP(NEXT_BLKP(ptr))) == 0)
		{
			remain = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - newsize;

			if (remain < 0)
			{
				extend = MAX(-remain, 1<<12);
				
				if (extend_heap(extend) == NULL)
					return NULL;
				remain += extend;
			}

			delete_node(NEXT_BLKP(ptr));

			PUT_NOTAG(HDRP(ptr), PACK(newsize + remain, 1));
			PUT_NOTAG(FTRP(ptr), PACK(newsize + remain, 1));

		}
		else
		{
			oldptr = mm_malloc(newsize - DOUBLEWORDSIZE);
			memcpy(oldptr, ptr, MIN(size, newsize));
			mm_free(ptr);
		}
		blockbuff = GET_SIZE(HDRP(oldptr)) - newsize;
	}

	if (blockbuff < 2 * (1<<7))
	{
		SET_RATAG(HDRP(NEXT_BLKP(oldptr)));
	}

	return oldptr;
}