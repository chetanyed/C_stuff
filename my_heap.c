#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#define HEAP_SIZE 4096

typedef struct chunk {
    uint32_t size;
    bool free;
    struct chunk *next_free;
} chunk;

typedef struct heap_info {
    void *start;
    uint32_t avail;
    chunk *free_list_head;
} heap;

void print_heap_state(heap *my_heap);

heap *heap_init() {
    heap *my_heap = (heap*)malloc(sizeof(heap));
    if (!my_heap) return NULL;

    my_heap->start = malloc(HEAP_SIZE);
    if (!my_heap->start) {
        free(my_heap);
        return NULL;
    }
    my_heap->avail = HEAP_SIZE;

    chunk *initial_chunk = (chunk*)my_heap->start;
    initial_chunk->size = HEAP_SIZE - sizeof(chunk);
    initial_chunk->free = true;
    initial_chunk->next_free = NULL;

    my_heap->free_list_head = initial_chunk;

    return my_heap;
}

void *heap_alloc(heap *my_heap, uint32_t size) {
    if (size == 0) return NULL;

    if (size % sizeof(void*) != 0) {
        size += sizeof(void*) - (size % sizeof(void*));
    }

    chunk *current = my_heap->free_list_head;
    chunk *prev = NULL;

    while (current != NULL) {
        if (current->size >= size) {
            uint32_t remaining_size = current->size - size;

            if (remaining_size > sizeof(chunk)) {
                chunk *new_free_chunk = (chunk*)((char*)current + sizeof(chunk) + size);
                new_free_chunk->size = remaining_size - sizeof(chunk);
                new_free_chunk->free = true;
                new_free_chunk->next_free = current->next_free;

                current->size = size;

                if (prev) {
                    prev->next_free = new_free_chunk;
                } else {
                    my_heap->free_list_head = new_free_chunk;
                }
            }
            else {
                if (prev) {
                    prev->next_free = current->next_free;
                } else {
                    my_heap->free_list_head = current->next_free;
                }
            }

            current->free = false;
            my_heap->avail -= (current->size + sizeof(chunk));
            
            return (void*)((char*)current + sizeof(chunk));
        }

        prev = current;
        current = current->next_free;
    }

    printf("No memory of sufficient size available.\n");
    return NULL;
}

void heap_free(heap *my_heap, void *ptr) {
    if (ptr == NULL) return;

    chunk *block_to_free = (chunk*)((char*)ptr - sizeof(chunk));
    
    if (block_to_free->free) {
        printf("Error: Block at %p is already free.\n", ptr);
        return;
    }
    
    block_to_free->free = true;
    my_heap->avail += block_to_free->size + sizeof(chunk);
    
    chunk *current = my_heap->free_list_head;
    chunk *prev = NULL;

    while (current != NULL && current < block_to_free) {
        prev = current;
        current = current->next_free;
    }

    if (prev != NULL && (char*)prev + sizeof(chunk) + prev->size == (char*)block_to_free) {
        prev->size += sizeof(chunk) + block_to_free->size;
        block_to_free = prev;
    }
    else {
        if (prev) {
            prev->next_free = block_to_free;
        } else {
            my_heap->free_list_head = block_to_free;
        }
        block_to_free->next_free = current;
    }
    
    if (current != NULL && (char*)block_to_free + sizeof(chunk) + block_to_free->size == (char*)current) {
        block_to_free->size += sizeof(chunk) + current->size;
        block_to_free->next_free = current->next_free;
    }
}

void print_heap_state(heap *my_heap) {
    printf("\n--- HEAP STATE ---\n");
    printf("Total Available: %u bytes\n", my_heap->avail);
    
    chunk *current = (chunk*)my_heap->start;
    int i = 0;
    while ((void*)current < my_heap->start + HEAP_SIZE) {
        printf("  Chunk %d at %p: Size=%-4u, State=%-10s\n",
               i,
               current,
               current->size,
               current->free ? "FREE" : "ALLOCATED");
        current = (chunk*)((char*)current + sizeof(chunk) + current->size);
        i++;
    }
    printf("------------------\n");
}

int main(void) {
    heap *my_heap = heap_init();
    printf("Heap initialized. Header size is %lu bytes.\n", sizeof(chunk));
    print_heap_state(my_heap);

    printf("\n1. Allocating three blocks: A(100), B(500), C(200)\n");
    void *block_a = heap_alloc(my_heap, 100);
    void *block_b = heap_alloc(my_heap, 500);
    void *block_c = heap_alloc(my_heap, 200);
    print_heap_state(my_heap);

    printf("\n2. Freeing middle block (B). This creates a hole.\n");
    heap_free(my_heap, block_b);
    print_heap_state(my_heap);
    
    printf("\n3. Freeing first block (A). This should coalesce with the hole left by B.\n");
    heap_free(my_heap, block_a);
    print_heap_state(my_heap);
    
    printf("\n4. Allocating a new, larger block (D) of 600 bytes. It should fit in the coalesced chunk.\n");
    void *block_d = heap_alloc(my_heap, 600);
    print_heap_state(my_heap);
    
    printf("\n5. Freeing last two blocks (C and D) to demonstrate final coalescing.\n");
    heap_free(my_heap, block_c);
    heap_free(my_heap, block_d);
    print_heap_state(my_heap);
    
    printf("\nFinal heap is one single free block again.\n");
    free(my_heap->start);
    free(my_heap);

    return 0;
}
