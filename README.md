# SmartOCD

## 简介

SmartOCD(Smart On-Chip Debugger)是适用多架构（RISC-V、ARM等）的Debug Server。它通过JTAG、SWD等接口与芯片通信，适用Lua脚本与芯片的调试组件（RISC-V DM、ARM Coresight）进行交互，实现对SoC行为的控制。

SmartOCD可以作为GDB Server，可以实现GDB对固件的调试。

## Support Platform

FreeBSD、Linux、MacOS、Windows
