/*
 * transport.h
 *
 *  Created on: 2018-1-29
 *      Author: virusv
 */

#ifndef SRC_DEBUGGER_TRANSPORT_H_
#define SRC_DEBUGGER_TRANSPORT_H_

// 仿真方式
enum transportType {
	UNKNOW,	// 未知协议，由仿真器指定
	JTAG,
	SWD,
	SWIM
};

#endif /* SRC_DEBUGGER_TRANSPORT_H_ */
