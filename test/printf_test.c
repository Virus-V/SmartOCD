/*
 * printf_test.c
 *
 *  Created on: 2018-2-20
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>

int main(){
	printf("asdsad %X\n", 0x01000000 | 0x20000000 | 0x02000000 | 0x00000040 | 0x00000010);
	exit(0);
}
