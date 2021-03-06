/*
 *
 * Copyright (c) 2015 Raphine Project
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
 * Author: Liva
 * 
 */

#include "physmem.h"
#include "paging.h"

extern int *phys_memory_end;

#ifdef __UNIT_TEST__

#include <stdlib.h>

PhysmemCtrl::PhysmemCtrl() {
  // for UNIT TEST
  _allocated_area = _allocated_area_buffer.Alloc();
  _allocated_area->start_addr = ptr2physaddr(malloc(0xFFF000));
  _allocated_area->end_addr = _allocated_area->start_addr + 0xFFF000;
  _allocated_area->start_addr = PagingCtrl::RoundUpAddrOnPageBoundary(_allocated_area->start_addr);
  _allocated_area->next = nullptr;
}

#else
#include <raph.h>
PhysmemCtrl::PhysmemCtrl() {
  _allocated_area = _allocated_area_buffer.Alloc();
  _allocated_area->start_addr = 0;
  kassert(ptr2physaddr(&phys_memory_end) % PagingCtrl::kPageSize == 0);
  // 2MB allocated by boot.S
  kassert(reinterpret_cast<phys_addr>(&phys_memory_end) < 0x200000);
  _allocated_area->end_addr = 0x200000;
  _allocated_area->next = nullptr;
}

#endif // __UNIT_TEST__

void PhysmemCtrl::Alloc(PhysAddr &paddr, size_t size) {
  kassert(size > 0);
  kassert(size % PagingCtrl::kPageSize == 0);
  _alloc_lock = false;
  phys_addr allocated_addr = 0;
  AllocatedArea *allocated_area = nullptr;
  AllocatedArea *fraged_area = nullptr;
  {
    Locker lock(_lock);
    allocated_area = _allocated_area;
    while(allocated_area->next) {
      if (fraged_area == nullptr &&
          allocated_area->next->start_addr == allocated_area->end_addr) {
        fraged_area = allocated_area;
      }
      if (allocated_area->next->start_addr - allocated_area->end_addr
          >= static_cast<phys_addr>(size)) {
        allocated_addr = allocated_area->end_addr;
        break;
      }
      allocated_area = allocated_area->next;
    }
    allocated_addr = allocated_area->end_addr;
    //TODO 物理メモリチェック
    allocated_area->end_addr += size;
    
    // 断片化された領域を結合する
    if (fraged_area != nullptr) {
      AllocatedArea *remove_area = fraged_area->next;
      fraged_area->end_addr = remove_area->end_addr;
      fraged_area->next = remove_area->next;
      fraged_area = remove_area;
    }
  }
  if (fraged_area != nullptr) {
    _allocated_area_buffer.Free(fraged_area);
  }
  paddr.SetAddr(allocated_addr);
  return;
}

void PhysmemCtrl::Free(PhysAddr &paddr, size_t size) {
  kassert(size > 0);
  kassert(size % PagingCtrl::kPageSize == 0);
  AllocatedArea *newarea = _allocated_area_buffer.Alloc();
  bool newarea_notused = false;
  phys_addr addr = paddr.GetAddr();
  paddr.Reset();
  AllocatedArea *allocated_area = nullptr;
  {
    Locker locker(_lock);
    allocated_area = _allocated_area;
    while(allocated_area) {
      if (allocated_area->start_addr <= addr &&
          addr + size <= allocated_area->end_addr) {
        break;
      }
      allocated_area = allocated_area->next;
    }
    if (allocated_area) {
      if (allocated_area->start_addr == addr) {
        newarea_notused = true;
        allocated_area->start_addr += size;
      } else if (allocated_area->end_addr == addr + size) {
        newarea_notused = true;
        allocated_area->end_addr = addr;
      } else {
        newarea_notused = false;
        newarea->start_addr = addr + size;
        newarea->end_addr = allocated_area->end_addr;
        newarea->next = allocated_area->next;
        allocated_area->end_addr = addr;
        // allocated_area->nextの書き込みは最後にする事
        // 他のプロセスのReadで事故る可能性があるので
        allocated_area->next = newarea;
      }
    }
  }
  if (newarea_notused) {
    _allocated_area_buffer.Free(newarea);
  }
}

// TODO: テスト書く
void PhysmemCtrl::Reserve(phys_addr addr, size_t size) {
  kassert(size > 0);
  kassert(size % PagingCtrl::kPageSize == 0);
  kassert(addr % PagingCtrl::kPageSize == 0);
  AllocatedArea *allocated_area = _allocated_area;
  AllocatedArea *fraged_area = nullptr;
  while(allocated_area->next) {
    if (fraged_area == nullptr &&
        allocated_area->next->start_addr == allocated_area->end_addr) {
      fraged_area = allocated_area;
    }
    kassert(allocated_area->start_addr <= addr);
    if (allocated_area->end_addr > addr) {
      if (allocated_area->end_addr > addr + size) {
        break;
      }
      if (addr + size <= allocated_area->next->start_addr) {
        allocated_area->end_addr = addr + size;
        break;
      }
      allocated_area->end_addr = allocated_area->next->start_addr;
      Reserve(allocated_area->next->start_addr, addr + size - allocated_area->next->start_addr);
      return;
    }
    if (allocated_area->next->start_addr >= addr) {
      if (addr + size <= allocated_area->next->start_addr) {
        AllocatedArea *newarea = _allocated_area_buffer.Alloc();
        newarea->next = allocated_area->next;
        allocated_area->next = newarea;
        newarea->start_addr = addr;
        newarea->end_addr = addr + size;
        break;
      }
      phys_addr tmp = allocated_area->next->start_addr;
      allocated_area->next->start_addr = addr;
      Reserve(tmp, addr + size - tmp);
      return;
    }
    allocated_area = allocated_area->next;
  }
    
  // 断片化された領域を結合する
  if (fraged_area != nullptr) {
    AllocatedArea *remove_area = fraged_area->next;
    fraged_area->end_addr = remove_area->end_addr;
    fraged_area->next = remove_area->next;
    fraged_area = remove_area;
    _allocated_area_buffer.Free(fraged_area);
  }
}

#ifdef __UNIT_TEST__

#include "physmem.h"
#include <future>
#include <random>
#include <iostream>

static void test1() {
  PhysmemCtrl _physmem_ctrl;
  physmem_ctrl = &_physmem_ctrl;
  auto th1 = std::thread([&_physmem_ctrl]{
      try {
        PhysAddr paddr1, paddr2;
        _physmem_ctrl.Alloc(paddr1, 0x1000);
        _physmem_ctrl.Alloc(paddr2, 0x1000);
        assert(paddr1.GetAddr() + 0x1000 == paddr2.GetAddr());
      } catch(const char *str) {
        std::cout<<str<<std::endl;
        assert(false);
      }
  });
  th1.join();
}

static void test2() {
  PhysmemCtrl _physmem_ctrl;
  physmem_ctrl = &_physmem_ctrl;
  auto th1 = std::thread([&_physmem_ctrl]{
      try {
        phys_addr addr1, addr2;
        PhysAddr paddr1, paddr2, paddr3;
        _physmem_ctrl.Alloc(paddr1, 0x1000);
        _physmem_ctrl.Alloc(paddr2, 0x1000);
        addr1 = paddr1.GetAddr();
        addr2 = paddr2.GetAddr();
        assert(addr1 + 0x1000 == addr2);
        _physmem_ctrl.Free(paddr1, 0x1000);
        _physmem_ctrl.Alloc(paddr1, 0x1000);
        assert(addr1 == paddr1.GetAddr());
        _physmem_ctrl.Alloc(paddr3, 0x1000);
        assert(addr2 + 0x1000 == paddr3.GetAddr());
      } catch(const char *str) {
        std::cout<<str<<std::endl;
        assert(false);
      }
  });
  th1.join();
}

static void mem_check(StdMap<phys_addr, int> mmap) {
  virt_addr addr = 0;
  for(auto it = mmap.Map().begin(); it != mmap.Map().end(); it++) {
    assert(addr <= it->first);
    assert(it->second > 0);
    addr += it->second;
  }
}
#include <iostream>
static void test3() {
  PhysmemCtrl _physmem_ctrl;
  physmem_ctrl = &_physmem_ctrl;

  StdMap<phys_addr, int> mmap;

  std::random_device seed1;
  std::mt19937 rnd(seed1());
  std::uniform_int_distribution<int> rnd1(9000, 10000);
  int th_num = rnd1(rnd);
  std::thread ths[th_num];

  for(int i = 0; i < th_num; i++) {
    ths[i] = std::thread([&_physmem_ctrl, &mmap]{
        try {
          std::random_device seed2;
          std::mt19937 random(seed2());
          std::uniform_int_distribution<int> rnd2(15, 20);
          std::uniform_int_distribution<int> rnd3(1, 2);
          for(int j = 0; j < rnd2(random); j++) {
            int size = rnd3(random);
            PhysAddr paddr;
            _physmem_ctrl.Alloc(paddr, size * 0x1000);
            mmap.Set(paddr.GetAddr(), size * 0x1000);
          }
        } catch(const char *str) {
          std::cout<<str<<std::endl;
          assert(false);
        }
      });
  }
  for(int i = 0; i < th_num; i++) {
    ths[i].join();
  }

  mem_check(mmap);
}

void physmem_test() {
  test1();
  test2();
  test3();
}

#endif // __UNIT_TEST__
