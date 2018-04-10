#!/bin/bash
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --log-file=./leak_check.log $1
