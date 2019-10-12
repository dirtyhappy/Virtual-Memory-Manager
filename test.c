#include <stdio.h>

int get_page_number (unsigned int address){
//	int res = address & 65280;
	int res = 0;
	res = address >> 8;
	return res;
}

int get_offset (unsigned int address){
	int res = 0;
	res = address & 255;
}
int main (void){
	unsigned int test = 16916;
	unsigned int page_number = get_page_number(test);
	unsigned int offset = get_offset(test);
	printf("%d and %d\n" , page_number, offset);	
}
