/*
 *
 * Copyright (c) 2016 Raphine Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Author: Yuchiki
 * 
 */

#ifndef __RAPH_KERNEL_DEV_KEYBOARD_H__
#define __RAPH_KERNEL_DEV_KEYBOARD_H__

#include <global.h>
#include <apic.h>

class Keyboard {
 public:
  void Setup(int cpuid);
  void Write(uint8_t code);
  uint8_t Read();
  char GetCh();
  bool Overflow();
  bool Underflow();
  int Count();
  void Reset();
 private:
  static void Handler (Regs *reg, void *arg);
  static const int kbufSize = 100;
  static const char kScanCode[256];
  static const int kDataPort = 0x60;
  int _count = 0; 
  uint8_t _buf[kbufSize];
  bool _overflow = false;
  bool _underflow = false;
  int _next_w = 0;
  int _next_r = 0;
};

#endif // __RAPH_KERNEL_DEV_KEYBOARD_H__
