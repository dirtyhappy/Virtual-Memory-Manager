#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define PAGE_NUMBER_MASK 65280
#define OFFSET_MASK		255

#define TLB_SIZE 16

#define BUFFER_SIZE 10

FILE * address_list;
FILE * backing_store;

//////////////////////////////////////////////////////////////////////////
//function declarations

/**
 * Function to retrieve the page number from a logical address
 *
 * @param unsigned int address - a logical address
 * @retval int - the page number of the logical address
 */
int get_page_number (unsigned int address);

/**
 * Function to retrieve the offset from a logical address
 *
 * @param unsigned int address - a logical address
 * @retval int - the offset of the logical address
 */
int get_offset (unsigned int address);

/** Function to initialize the page table
 *
 * Sets each entry in the page table to -1
 */
void init_page_table(void);

/**
 * Function to initialize the physical memory 
 *
 * sets all frames as free.
 */
void init_physical_memory(void);

/**
 * Function to initialize the tlb
 *
 * sets all member variables of each entry to -1
 */
void init_tlb(void);
/**
 * Function to retrieve the next free frame in physical memory
 *
 * @retval int - next free frame
 */
int get_next_free_frame(void);

/**
 * Function to check the tlb
 *
 * @param int page number - a page number obtained by a logical address
 * @retval int - returns -1 if tlb-miss, or frame that the page is stored in.
 */
int check_tlb(int page_number);

/**
 * Function to update the tlb for a new page number to frame mapping
 *
 * @param int page_number :		a page number
 * @param int frame_number :	the frame the page is mapped to
 */
void update_tlb(int page_number, int frame_number);

/**
 * Function to check the page table for the page number
 *
 * @retval int :	returns -1 if page fault, otherwise the frame number
 */
int check_page_table(int page_number);
//////////////////////////////////////////////////////////////////////////

//entry in the tlb
struct tlb_entry{
	int page_number;
	int frame_number;
	int valid;
};


//buffer for reading logical addresses from the address file
char buffer[BUFFER_SIZE];

//physical memory
signed char physical_memory[256][256];

//page table for 256 pages
int page_table[256];

//paging variables
int page_number;
int frame_number;
int offset;

//logical address read from address file
int logical_address;

//translated address
int physical_address;

//value stored at physical address
signed char value;

//statistics
int page_faults = 0;
int tlb_hits = 0;

//list of free frames
int free_frames[256];

//number of free frames
int number_of_free_frames;

//index of next free tlb entry if not full
int next_free_tlb_index;

//tlb
struct tlb_entry tlb[TLB_SIZE];

//temporary frame read from backing store
signed char temp_frame[256];


//////////////////////////////////////////////////////////////////////////
//function implementations

//-----------------------------------------------------------------------
// get_page_number
//
int get_page_number (unsigned int address){

	return (address & PAGE_NUMBER_MASK) >> 8;
}

//----------------------------------------------------------------------
// get_offset
//
int get_offset (unsigned int address){

	return (address & OFFSET_MASK);
		
}

//----------------------------------------------------------------------
//
// init_page_table
//
void init_page_table(void){
	int i;
	//set all entries to -1
	for (i = 0; i < 256; ++i){
		page_table[i] = -1;
	}
}

//---------------------------------------------------------------------
//
// init_physical_memory
//
void init_physical_memory(void){
	int i;
	
	for (i = 0; i < 256; ++i){
		free_frames[i] = 1;
	}
	
	number_of_free_frames = 256;
}

//----------------------------------------------------------------------
//
// init_tlb
//
void init_tlb (void){
	int i;
	for(i = 0; i < TLB_SIZE; ++i){
		tlb[i].valid = -1;
		tlb[i].page_number = -1;
		tlb[i].frame_number = -1;
	}
	
	next_free_tlb_index = 0;
}
//----------------------------------------------------------------------
//
// get_next_free_frame
//
int get_next_free_frame(void){
	
	int next_free_frame = -1;
	int i = 0;
	
	if(number_of_free_frames > 0){
		while (free_frames[i] == -1){
			i++;
		}
		next_free_frame = i; //set to next free frame
		free_frames[i] = -1; // set frame to occupied
		number_of_free_frames--; //decrement the number of free frames
	}
	
	return next_free_frame;	
}

//----------------------------------------------------------------------
//
// check_tlb
// 
int check_tlb (int page_number){
	//this is a slow implementation. A dictionary lookup would be faster.
	int i;
	int frame_number = -1;
	
	for (i = 0 ; i < TLB_SIZE; ++i){
		if ( (tlb[i].page_number == page_number) && (tlb[i].valid == 1) ){
			frame_number = tlb[i].frame_number;
			++tlb_hits;
			break;
		}
	}
	return frame_number;
}

//----------------------------------------------------------------------
//
// update_tlb
//
void update_tlb (int page_number, int frame_number){
	tlb[next_free_tlb_index].page_number = page_number;
	tlb[next_free_tlb_index].frame_number = frame_number;
	tlb[next_free_tlb_index].valid = 1;

	next_free_tlb_index = (next_free_tlb_index + 1) % TLB_SIZE;
}

//---------------------------------------------------------------------
//
// check_page_table
//
int check_page_table (int page_number){
	int frame_number = -1;
	
	frame_number = page_table[page_number];
	
	//if page fault
	if (frame_number == -1){
		++page_faults;

		if (fseek(backing_store, (page_number * 256), SEEK_SET) != 0){
			fprintf(stderr, "Error seeking in backing store\n");
		}		
	
		if (fread(temp_frame, sizeof(signed char), 256, backing_store) == 0) {
			fprintf(stderr, "Error reading from backing store\n");
			return -1;
		}
	
		//update page_table
		frame_number = get_next_free_frame();
		page_table[page_number] = frame_number;
	
		//copy temporary frame into physical memory
		if(frame_number >= 0){
			memcpy(&physical_memory[frame_number], temp_frame, 256);
		}
	
		//update the tlb 
		update_tlb(page_number, frame_number);
		
	}
	return frame_number;
}
////////////////////////////////////////////////////////////////////////
//driver
int main (int argc, char * argv[]){
	//error check
	if (argc != 3){
		fprintf(stderr, "Usage : ./a.out [backing store] [address file]\n");
		return -1;
	}	
	
	//open backing store
	backing_store = fopen(argv[1], "rb");
	
	//if backing store doesn't open print an error.
	if (backing_store == NULL){
		fprintf(stderr, "Error opening %s\n", argv[1]);
	}

	//open address file
	address_list = fopen(argv[2] , "r");
	
	//if address list doesn't open print an error.
	if (address_list == NULL){
		fprintf(stderr, "Error openeing %s\n", argv[2]);
	}

	//intialize physical memory
	init_physical_memory();

	//initialize the page table	
	init_page_table();

	//initialize the tlb
	init_tlb();
	
	while (fgets(buffer, BUFFER_SIZE, address_list) !=NULL) {
		//read logical address
		logical_address = atoi(buffer);
	
		//extract page number and offset
		page_number = get_page_number(logical_address);
		offset = get_offset(logical_address);
		
		//check tlb
		frame_number = check_tlb(page_number);
			
		//if tlb miss
		if (frame_number == -1) {
			//check page table
			frame_number = check_page_table(page_number);
		} 
		
		//set physical address
		physical_address = (frame_number * 256) + offset;		
		//get the value from the frame 
		value = physical_memory[frame_number][offset];
		
		printf("logical address : %d  physical address : %d  value : %d\n",logical_address, physical_address, value);
		
	}
	printf("Page Faults : %d\n", page_faults);
	printf("TLB hits : %d\n",tlb_hits);
	
	//close files
	fclose(address_list);
	fclose(backing_store);
	
	return 0;
}
