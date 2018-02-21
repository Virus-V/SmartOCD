/*
 * misc.c
 *
 *  Created on: 2018-2-15
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>
/*
 * 打印数据块
 * data：数据内容
 * length：数据长度
 * rowLen：每一行打印多少个byte
 * 返回：打印的行数
 */
int print_bulk(char *data, int length, int rowLen){
	// 计算要打印多少行
	int lineCnt, placeCnt, tmp;	// 数据总行数，行数的数字位数（用来计算前导0）
	int currRow, printedCnt = 0;	// 当前输出的行和列
	if(length < rowLen) rowLen = length;	// 如果总长度小于一行的长度，则把一行的长度设置为总长度输出在一行
	lineCnt = (length / rowLen) + 1;	// 计算行数
	for(tmp = length - (length % rowLen), placeCnt = 1; tmp >= 16; placeCnt++){
		tmp /= 16;
	}
    // 输出一行数据：前导位置信息：数据...
	for(currRow = 0; currRow < lineCnt && rowLen > 0; currRow ++){
		int currPlaceCnt;	// 已输出的数据数字位数和已输出的数据
		// 计算位数
		for(tmp = printedCnt, currPlaceCnt = 1; tmp >= 16; currPlaceCnt++){
			tmp /= 16;
		}
		// 输出前导信息
		for(tmp = placeCnt - currPlaceCnt; tmp > 0; tmp--){
			putchar('0');
		}
		printf("%X: ", printedCnt);
		// 输出数据
		for(tmp = 0; tmp < rowLen ; tmp++){
			printf("%02hhX ", *(data + printedCnt + tmp));
		}
		// 加入输出总长度
		printedCnt += rowLen;
		printf("\n");
		// 判断是否是最后一行
		if(currRow == lineCnt-2){
			rowLen = length % rowLen;
		}
	}
	return currRow;
}
