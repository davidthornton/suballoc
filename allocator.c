#include <stdio.h>
#include <math.h>
#include <stddef.h>

#include "allocator.h"

#define U_INT32_MAX 0xffffffff

typedef unsigned char byte;
static byte *memory = NULL;
static byte *free_list_ptr = NULL;


u_int32_t HEADER_SIZE;

typedef struct header* Link; //refactor this as HeaderLink ? When messing with memory, 
							 //it is desireable, but when "doing" linked lists it isn't...
struct header {
	u_int32_t magic;
	u_int32_t size;
	Link next;
	Link prev;

} Header;


// Helper functions
u_int32_t minimalSufficientPower(u_int32_t size);

// Internal abstraction
static void *searchForBlock(u_int32_t requestSize);
static void *splitAndAllocate(byte* offset, u_int32_t normalisedRequestSize);
static void mergeBlocks(void* currentBlockPtr);

void* memValidityCheck(Link ptr);
void* memFreeValidityCheck(Link ptr);

// Debugging functions
u_int32_t peekAtOffsetAsUInt32 (byte *offset);
void peekAtOffsetAsIntPtr (byte * offset);
void printFreeListAsc(Link list);
void printFreeListDesc(Link list);


int main() {

	u_int32_t size 		= 4096;
	
	byte *ptrArray[100];
	ptrArray[0] = NULL; //just in case

	
	allocator_init(size);
	
	printf("\n**\nInitialised with:\t%d bytes\n**\n\n",size);


	printf("Beginning block\n");
	printFreeListAsc((Link)free_list_ptr);



	int i;

	for(i=1; i<=8;i++){
	
		ptrArray[i] = allocator_malloc(240);
	
		if(!ptrArray[i]){
			printf("error allocating memory | allocator_malloc returned NULL\n\n");
		}
		
		printFreeListAsc((Link)free_list_ptr);
		
	}

	for(i=9; i<=13;i++){
	
		ptrArray[i] = allocator_malloc(490);
	
		if(!ptrArray[i]){
			printf("error allocating memory | allocator_malloc returned NULL\n\n");
		}
		
		printFreeListAsc((Link)free_list_ptr);
		
	}


/* DEALLOC DESCENDING */

/*	for(i=13; i>=0;i--){
	
	
		if( i>=0 && ptrArray[i] != NULL){
			allocator_free(ptrArray[i]);
			printFreeListAsc((Link)free_list_ptr);
		}
	}
*/

/* DEALLOC ASCENDING */

	for(i=1; i<=6;i++){
	
	
		if( i>=0 && ptrArray[i] != NULL){
			allocator_free(ptrArray[i]);
			printFreeListAsc((Link)free_list_ptr);
		}
	}

		
	allocator_end();

	return 0;
}

void allocator_end(void) {

	free(memory);
	memory = NULL;
	free_list_ptr = NULL;
}

void allocator_init(u_int32_t size) {

	u_int32_t valid_size;

	// As per allocator.h, this must do nothing if
	// it detects that the allocator has previously been initialised
	if(memory != NULL){
		return;
	}
	
	if(size > pow(2,31)){
		// There is no power of two greater than the requested
		// size that will also fit into the type u_int32_t
		valid_size = pow(2,31);
	} else {
		valid_size = minimalSufficientPower(size);
	}
	
	//printf("valid size: %Ld\n", (long long int)valid_size);

	// Request mem from os - I thought it would be clearer to cast the ptr to memory to 
	//		begin with... but the spec says struct header
	Link header = (Link) malloc(valid_size);
	if(header == NULL){
		fprintf(stderr,"There was an error calling malloc()... aborting\n");
		abort();
	}
			
	header->magic = 0xdeadbeef;
	header->size = valid_size;
	header->next = header;
	header->prev = header;
	
	// Set header size (pointers are 4 bytes long on CSE machines, so HEADER_SIZE
	//					should nearly always be 16)
	HEADER_SIZE = sizeof(Header);
	
	// Allocate the rest of the pointers
	memory 			= (byte *)header;
	free_list_ptr	= (byte *)header;

}

void *allocator_malloc(u_int32_t n){

	//Allocated block
	byte *allocatedBlock = NULL;
	byte *selectedOffset;


	// Convert the number requested into a power of two block
	u_int32_t requestSize = minimalSufficientPower(n + HEADER_SIZE);

	// Find a good block to subdivide and allocate
	selectedOffset = searchForBlock(requestSize);


	if(selectedOffset){
	
		// Allocate memory and subdivide to align
		allocatedBlock = splitAndAllocate(selectedOffset, requestSize);

	} else {
		//no block could be found
		allocatedBlock = selectedOffset;
	
	}

	return (void *)allocatedBlock;

}

void allocator_free(void *object){

	Link freedPtr = (Link)((byte*)object - HEADER_SIZE);
	Link curr = (Link) free_list_ptr;
	int listCycle = 0, found = 0;

	Link lowestMemOffset = (Link)U_INT32_MAX, highestMemOffset = (Link)0x0;	
	
	memFreeValidityCheck(freedPtr);
	

printf("FREEING:\n");


	while(!listCycle && !found){
		
		//this feels so wrong but I am so so tired
		lowestMemOffset = curr < lowestMemOffset ? curr : lowestMemOffset;
		highestMemOffset = curr > highestMemOffset ? curr : highestMemOffset;
		
		if(curr < freedPtr && curr->next >= freedPtr){
		
		
			printf("found block in MIDDLE of list\n");
			
			found = 1;
		
			freedPtr->magic = 0xdeadbeef; // unallocated again
			freedPtr->prev = curr;
			freedPtr->next = curr->next;
			curr->next->prev = freedPtr;
			curr->next = freedPtr;
		
		}
	
		curr = curr->next;
		if((byte *)curr == free_list_ptr){
			listCycle++;
		}
	}
	
	
	
	if( !found && freedPtr <= lowestMemOffset){
	
		//printf("was lower than lowest\n");

		freedPtr->magic = 0xdeadbeef; // unallocated again
		freedPtr->next = lowestMemOffset;
		freedPtr->prev = lowestMemOffset->prev;
		lowestMemOffset->prev->next = freedPtr;
		lowestMemOffset->prev = freedPtr;
	
	} else if ( !found && freedPtr >= highestMemOffset){
			
		//printf("was higher than highest\n");

		freedPtr->magic = 0xdeadbeef; // unallocated again
		freedPtr->prev = highestMemOffset;
		freedPtr->next = highestMemOffset->next;
		highestMemOffset->next->prev = freedPtr;
		highestMemOffset->next = freedPtr;
			
	}
	
	
	// Merge blocks back together now
	
	printf("** merge candidate: offset %d | size %d\n", (int)freedPtr-(int)memory, freedPtr->size);
	
	mergeBlocks(freedPtr);

}

static void mergeBlocks(void* currentBlockPtr){

	Link currentBlock = (Link)currentBlockPtr;
	Link prevBlock = ((Link)currentBlockPtr)->prev;
	ptrdiff_t currentBlockOffset = (byte*)currentBlock - (byte*)memory;
	ptrdiff_t prevBlockOffset = (byte*)prevBlock - (byte*)memory;


	//base case - only one block left
	if(currentBlock->next == currentBlock && currentBlock->prev == currentBlock){
		
		printf("plz stop\n");
		
		return;
	}
	

	//we should merge if the neighbouring block is:
	//	1. the exact same size, and;
	//	2. offset % block size == 0 (i.e. they are exact integer multiples)
	//	3. AND THE BLOCKS ARE CONTIGUOUS
	

	int isContiguousOnRight = (byte*)currentBlock->next == ((byte*)currentBlock + currentBlock->size);
	int isContiguousOnLeft = ((byte *)currentBlock->prev + currentBlock->prev->size) == (byte*)currentBlock;
	
	if(currentBlock->size == currentBlock->next->size && isContiguousOnRight){
		// find same sized AND contiguous blocks
	
		if( (u_int32_t)currentBlockOffset % (currentBlock->size * 2) == 0) {

			//merge block!
			currentBlock->size = currentBlock->size * 2;
			currentBlock->next = currentBlock->next->next;
			currentBlock->next->prev = currentBlock;
			
			//update free_list_ptr
			free_list_ptr = (byte *)currentBlock;
			
			// RECURSIVE CALL
			mergeBlocks(currentBlock); // call recursively on the new block (same header)
		}
	
	} else if(currentBlock->size == currentBlock->prev->size && isContiguousOnLeft) {
		// find same sized AND contiguous blocks
		
		if( (u_int32_t)prevBlockOffset % (currentBlock->size * 2) == 0) {

			//merge block!
			currentBlock->prev->size = currentBlock->size * 2;
			currentBlock->next->prev = currentBlock->prev;
			currentBlock->prev->next = currentBlock->next;
			
			//update free_list_ptr
			free_list_ptr = (byte *)currentBlock->prev;
			
			// RECURSIVE CALL
			mergeBlocks(currentBlock->prev); // call recursively on the new block (header before)
		}
	
	}
	
	//otherwise, don't merge

}

static void *searchForBlock(u_int32_t requestSize) {

	u_int32_t minSuffBlockSize = U_INT32_MAX;
	Link minSuffBlockLink = NULL;
	Link curr = (Link) free_list_ptr;

	int listCycle = 0;

	while(!listCycle){


		// Determine if the block is a good size to use
		if( memValidityCheck(curr) && curr->size >= requestSize && curr->size < minSuffBlockSize){
			
			
			//printf("found one: %d addr: %x\n\n",curr->size, curr);
			
			minSuffBlockSize = curr->size;
			minSuffBlockLink = curr;
		
		}
		
		curr = curr->next;
		if((byte *)curr == free_list_ptr){
		
			listCycle++;
		}
	}

	return (void *)minSuffBlockLink;
}

//
//	Expects a power of two as input for n
//		Undefined results otherwise
//
//	normalisedRequestSize is pads the user requested size to the nearest power of two and includes a HEADER_SIZE length header

static void *splitAndAllocate(byte* offset, u_int32_t normalisedRequestSize){

	u_int32_t currentSize = ((Link)offset)->size;
	u_int32_t halfSize = currentSize / 2;			//size guaranteed to be even
	
	byte *halfwayPtr = offset + currentSize/2;
	
		
	if(((Link)offset)->size >= normalisedRequestSize && ((Link)offset)->size < 2*normalisedRequestSize ){
		// base case
	
		
		if( ((Link)offset)->next == ((Link)offset) && ((Link)offset)->prev == ((Link)offset) ){
		// Always keep a block free at the end!
				
			return NULL;
		
		}
	
	
		//take it out of the free list
		((Link)offset)->next->prev = ((Link)offset)->prev;
		((Link)offset)->prev->next = ((Link)offset)->next;
		
		free_list_ptr = (byte *)((Link)offset)->next;
		
		//allocate it!
		((Link)offset)->magic = 0xdeafbead;
		((Link)offset)->next = NULL;
		((Link)offset)->prev = NULL;


		return (void *)(offset+HEADER_SIZE);
	}
	
	// the IF is not true here, so we should split (if it were true, we would have returned by now)

	
	// the right block must point EITHER to the next block in ascending mem order, or to offset if it is the last one
	((Link)(halfwayPtr))->magic = 0xdeadbeef;
	((Link)(halfwayPtr))->size = halfSize;
	((Link)(halfwayPtr))->next = ((Link)offset)->next;
	((Link)(halfwayPtr))->prev = (Link)offset;

	//update the block AFTER the newly created block in ascending mem order. If there is none, this will be a nop
	((Link)offset)->next->prev = ((Link)(halfwayPtr));

	// let the leftmost block just point to the newly created halfway pt
	((Link)offset)->size = halfSize;
	((Link)offset)->next = ((Link)(halfwayPtr));
	
	
	
	return (void *)splitAndAllocate(offset,normalisedRequestSize);

}


void printFreeListAsc(Link list){

	int listCycle = 0;



	while(!listCycle){
	

		printf("[off: %d | size: %d]->", (int)list-(int)memory,list->size);	
		list = list->next;
		
			
			
			//printf("free_list_ptr: 0x%08x\n\n", (unsigned int)list);
		
		
		
		if((byte *)list == free_list_ptr){
		
			printf("[<-loop]\n\n");
		
			listCycle++;
		}
	}
}

void *memValidityCheck(Link ptr) {

	if (ptr->magic != 0xdeafbead && ptr->magic != 0xdeadbeef){
		fprintf(stderr, "Memory integrity error\n");
		abort();	
	}

	return (void *)ptr;
}

void *memFreeValidityCheck(Link ptr) {

	if (ptr->magic != 0xdeafbead && ptr->magic != 0xdeadbeef){
		fprintf(stderr, "Pointer given was not immediately proceeding header\n");
		abort();	
	}

	return (void *)ptr;
}

//
// (Very useful) Debugging function
//
//	Input:  byte* (unsigned char *)
//	Output: u_int32_t
//
u_int32_t peekAtOffsetAsUInt32 (byte *offset){

	printf("\n*** Single ***\nOffset:\t\t0x%08x\nDeref Offset:\t0x%08x\n******\n\n", (unsigned int) offset, (unsigned int) (*(u_int32_t *)offset));

	return *(u_int32_t *)offset;
}


void peekAtOffsetAsIntPtr (byte * offset){ 
	printf("\n*** Single ***\nOffset:\t\t0x%08x\nPointer:\t0x%08x\nDeref Pointer:\t0x%08x\n******\n\n", (int) offset, *((int *)(offset)), **((int **)(offset)) );
}

// Not content with a logarithmic solution, attributed to:
//  http://graphics.stanford.edu/~seander/bithacks.html

u_int32_t minimalSufficientPower(u_int32_t size){

	// calculate the smallest power of two that is larger than the requested block size
	//
	//	Largest block requestable: 2^31 bytes = 2147483648 (dec) = 0x80000000
	//	(this is because our type is 32 bit, to store 2^32 would require 33 bits)

	size--;
	size |= size >> 1;
	size |= size >> 2;
	size |= size >> 4;
	size |= size >> 8;
	size |= size >> 16;
	size++;

	return size;
}
