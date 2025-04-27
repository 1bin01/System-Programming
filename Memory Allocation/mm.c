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
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20201644",
    /* Your full name*/
    "Wonbin Cho",
    /* Your email address */
    "wonbin0504@sogang.ac.kr",
};

/* constant value && global varibales*/
#define MAX_LOG 32
static char** segregated_list;
static char *heap_listp, **heap_ptr;;
static int prev_size;

/* 강의 자료에 나오는 macros*/
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ //line:vm:mm:beginconst
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  //line:vm:mm:endconst 

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size << 3) | (alloc)) //line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            //line:vm:mm:get
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    //line:vm:mm:put
#define PUTCHAR(p, val) (*(char **)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_MASK(size) ((1 << size) - 1)
#define GET_SIZE(p) ((GET(p) & ~GET_MASK(3)) >> 3)
#define GET_ALLOC(p) (GET(p) & 0x1)                    //line:vm:mm:getalloc
#define GET_TSIZE(p) (GET_SIZE(p) + 2)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      //line:vm:mm:hdrp
#define FTRP(bp)       ((char **)(bp) + GET_SIZE(bp) + 1) //line:vm:mm:ftrp

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  (FTRP(bp) + 1) //line:vm:mm:nextblkp
#define PREV_BLKP(bp)  ((char **)(bp)-GET_TSIZE((char **)(bp)-1)) //line:vm:mm:prevblkp

#define PREV_NODE(bp) (*((char **)(bp + 1)))
#define NEXT_NODE(bp) (*((char **)(bp) + 2))

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


static void add_node(char **bp);
static void remove_node(char **bp);
void *go(void *ptr, size_t size, int buffer_size);
static void *extend_heap(size_t words);
static void *find_fit(size_t words);
static void place(void *bp, size_t words);
static void *coalesce(void *bp);


int mm_init(void)
{
    /* segregated list 시작 포인터를 위한 메모리 할당*/
    if((segregated_list = mem_sbrk(MAX_LOG * sizeof(char *) + WSIZE)) == (void *)-1) return -1;
    for (int i = 0; i < MAX_LOG; i++) segregated_list[i] = NULL;

    /* heap 메모리 할당*/
    if ((heap_ptr = mem_sbrk(DSIZE + DSIZE)) == (void *)-1) return -1;
    PUT(heap_ptr, PACK(0, 1)); /* Prologue header */ 
    PUT(heap_ptr + WSIZE, PACK(0, 1)); /* Prologue footer */ 
    PUT(heap_ptr + DSIZE, PACK(0, 1));     /* Epilogue header */
    PUT(heap_ptr + DSIZE + WSIZE, PACK(0, 1));     /* Epilogue footer */
    heap_ptr += DSIZE;
    return 0;
}

void *mm_malloc(size_t sz)
{
    char **bp;
    if (!sz) return NULL;
    size_t size = 1, words, E;
    /* c++의 vector도 크기가 2배씩 증가하는데 malloc할 때도 이렇게 할당하는게 효율적인 경우가 많다고 함*/
    /* 여기서 CHUNKSIZE가 넘어가는 데도 2배를 하면 5번에서 메모리 터짐 ㅜ*/
    if (sz <= CHUNKSIZE){
        while(size < sz) size <<= 1;
    }
    else size = sz;
    words = ALIGN(size) >> 2;

    /* 메모리 공간이 부족하면 heap 새로 할당*/
    if (!(bp = find_fit(words))){
        if (!(bp = extend_heap(MAX(words, CHUNKSIZE >> 2)))) return NULL;
        place(bp, words);
    }
    else remove_node(bp);
    /* 공간을 할당하고 block을 segregated list에 추가*/
    place(bp, words);
    return ++bp;
}

void mm_free(void *bp)
{
    if(!bp) return;
    bp -= WSIZE;

    size_t size = GET_SIZE(bp);
    PUTCHAR(bp, PACK(size, 0));
    PUTCHAR(FTRP(bp), PACK(size, 0));
    /* free한 블럭을 앞뒤 free block과 연결해줌*/
    add_node(coalesce(bp));
    return;
}

void *mm_realloc(void *ptr, size_t size)
{
    int buf, d = abs(size - prev_size);
    size_t t = 1;
    if(d < CHUNKSIZE){
        while(t < d) t <<= 1;
    }

    if (d < CHUNKSIZE && d % t) buf = t;
    else buf = size % (1<<10) >= (1<<9) ? size + (1<<10) - size % (1<<10) : size - size % (1<<10);
    prev_size = size;
    return go(ptr, size, buf);
}


static void *extend_heap(size_t size)
{
    char **bp;
    /* 8-byte alignment이므로 2word씩 할당*/
    if(size & 1) size++;

    if ((long) (bp = mem_sbrk((size + 2) << 2)) == -1) return NULL;
    bp -= 2;

    PUTCHAR(bp, PACK(size, 0));
    PUTCHAR(FTRP(bp), PACK(size, 0));
    PUTCHAR(bp + size + 2, PACK(0, 1));
    PUTCHAR(FTRP(bp + size + 2), PACK(0, 1));
    return bp;
}


static void add_node(char **bp)
{
    if(!bp) return;
    size_t size = GET_SIZE(bp), t = 1;
    if (!size) return;
    int i = 0;
    while((t<<1) <= size) i++, t <<= 1;

    *(bp + 1) = NULL;     // next pointer
    *(bp + 2) = NULL;     // prev pointer

    if (!segregated_list[i]) segregated_list[i] = bp;
    else if (size >= GET_SIZE(segregated_list[i])){
        /* list의 맨 앞에 삽입 */
        *(bp + 2) = segregated_list[i];
        *((char**) segregated_list[i] + 1) = bp;
        segregated_list[i] = bp;
    }
    else{
        char **prev = segregated_list[i];
        while(NEXT_NODE(prev) != NULL && GET_SIZE(NEXT_NODE(prev)) > size) prev = NEXT_NODE(prev);
        char **nxt = NEXT_NODE(prev);
        /* 뒤에 노드랑 연결*/
        if(nxt){
            *((char **) bp + 2) = nxt;
            *((char **) nxt + 1) = bp;    
        }    
        /* 앞에 노드랑 연결*/
        *((char **) prev + 2) = bp;
        *((char **) bp + 1) = prev;
    }
}

static void *find_fit(size_t size)
{
    char **bp;
    int i = 0;
    size_t t = 1;
    while((t<<1) <= size) i++, t <<= 1;
    while(i < MAX_LOG){
        if((bp = segregated_list[i]) == NULL || GET_SIZE(bp) < size) {
            i++; continue;
        }

        while(1){
            /* 같을 때 멈춰줘야 퍼포먼스가 높게 나옴,,*/
            if (GET_SIZE(bp) == size) return bp;
            if (NEXT_NODE(bp) == NULL || GET_SIZE(NEXT_NODE(bp)) < size) return bp;
            bp = NEXT_NODE(bp);
        }
        i++;
    }
    return NULL; /* No fit */
}

static void place(void *bp, size_t words)
{
    size_t asize = words + 2, csize = GET_SIZE(bp) + 2;
    /* 할당하고도 블록이 남는 경우 (2 word이상 남는 경우에만 새로운 free block을 만들어줌)*/
    if (csize - asize >= 2){   
        PUTCHAR(bp, PACK(words, 1));
        PUTCHAR(FTRP(bp), PACK(words, 1));
        bp = (char**)bp + asize;
        asize = csize - asize - 2;
        PUTCHAR(bp, PACK(asize, 0));
        PUTCHAR(FTRP(bp), PACK(asize, 0));
        bp = coalesce(bp);
        add_node(bp);
    }
    else{
        PUT(bp, PACK(csize - 2, 1));
        PUT(FTRP(bp), PACK(csize - 2, 1));
    }
}

static void remove_node(char **bp)
{
    if(!bp) return;
    size_t size = GET_SIZE(bp);
    if(!size) return;

    char** p = PREV_NODE(bp);
    char** nx = NEXT_NODE(bp);
    // 앞 노드 수정
    if(!p){
        int i = 0;
        size_t t = 1;
        while((t<<1) <= size) i++, t <<= 1;
        segregated_list[i] = nx; 
    }
    else *(p + 2) = nx;
    // 뒷 노드 수정
    if (nx) *(nx + 1) = p;
    return;
}

static void *coalesce(void *bp)
{
    char **p = PREV_BLKP(bp), **nx = NEXT_BLKP(bp);
    size_t pa = GET_ALLOC(p), na = GET_ALLOC(nx);
    size_t size = GET_SIZE(bp);

    if(pa && na) return bp;         /* Case 1*/
    else if (pa && !na){            /* Case 2*/
        remove_node(nx);    
        size += GET_TSIZE(nx);
        PUTCHAR(bp, PACK(size, 0));
        PUTCHAR(FTRP(nx), PACK(size, 0));
    }
    else if (!pa && na) {           /* Case 3*/
        remove_node(p);
        size += GET_TSIZE(p);
        PUTCHAR(p, PACK(size, 0));
        PUTCHAR(FTRP(bp), PACK(size, 0));
        bp = p;
    }
    else{                           /* Case 4*/
        remove_node(p);
        remove_node(nx);
        size += GET_TSIZE(p) + GET_TSIZE(nx);
        PUTCHAR(p, PACK(size, 0));
        PUTCHAR(FTRP(nx), PACK(size, 0));
        bp = p;
    }
    return bp;
}


void *go(void *ptr, size_t size, int buf_size){
    char** old, **bp;
    size_t cur = ALIGN(size) / WSIZE, buf = cur + buf_size, bf;
    if (!ptr) return mm_malloc(ptr);
    old = bp = ptr;
    old--; bp--;
    bf = GET_SIZE(bp);

    if (bf == buf && cur <= buf) return bp + 1;
    if (!cur){
        mm_free(ptr); return NULL;
    }
    else if (cur > bf){
        size_t prev = GET_SIZE(PREV_BLKP(bp));
        int pa = GET_ALLOC(PREV_BLKP(bp));
        size_t next = GET_SIZE(NEXT_BLKP(bp));
        int na = GET_ALLOC(NEXT_BLKP(bp));

        if (pa && !na && next + bf + 2 >= buf){
            PUTCHAR(bp, PACK(bf, 0));
            PUTCHAR(FTRP(bp), PACK(bf, 0));
            bp = coalesce(bp);
            place(bp, buf);
        }
        else if (!pa && na && prev + bf + 2 >= buf){
            PUTCHAR(bp, PACK(bf, 0));
            PUTCHAR(FTRP(bp), PACK(bf, 0));
            bp = coalesce(bp);
            memmove(bp + 1, old + 1, bf << 2);
            place(bp, buf);
        }
        else if (!pa && !na && prev + next + bf + 4 >= buf){
            PUTCHAR(bp, PACK(bf, 0));
            PUTCHAR(FTRP(bp), PACK(bf, 0));
            bp = coalesce(bp);
            memmove(bp + 1, old + 1, bf << 2);
            place(bp, buf);
        }
        else{
            if (!(bp =(char **)mm_malloc((buf << 2) + WSIZE) - 1)) return NULL;
            memcpy(bp + 1, old + 1, bf << 2);
            mm_free(old + 1);
        }
    }
    return bp + 1;
}