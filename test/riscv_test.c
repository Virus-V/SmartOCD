/**
 * test/riscv_test.c
 * Copyright (c) 2020 Virus.V <virusv@live.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <unistd.h>
#include "ctest.h"

#include "Adapter/ftdi/ftdi.h"
#include "Library/jtag/jtag.h"
#include "Library/log/log.h"

uint16_t vids[] = {0x0403, 0};
uint16_t pids[] = {0x6010, 0};
