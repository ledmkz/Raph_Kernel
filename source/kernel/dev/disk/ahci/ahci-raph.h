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
 * Author: Liva
 * 
 */

#ifndef __RAPH_KERNEL_DEV_DISK_AHCI_AHCI_RAPH_H__
#define __RAPH_KERNEL_DEV_DISK_AHCI_AHCI_RAPH_H__

#include <sys/types-raph.h>
#include <sys/bus-raph.h>

class DevDisk {
public:
  DevDisk() {
  }
};

class BsdDevPciDisk : public DevDisk {
public:
  BsdDevPciDisk(uint8_t bus, uint8_t device, uint8_t function) : DevDisk(), _bsd_pci(bus, device, function) {
    _bsd.SetClass(&_bsd_pci);
  }
  virtual ~BsdDevPciDisk() {
  }
protected:
  BsdDevice _bsd;
private:
  BsdDevPci _bsd_pci;
};

class AhciCtrl : public BsdDevPciDisk {
public:
  AhciCtrl(uint8_t bus, uint8_t device, uint8_t function) : BsdDevPciDisk(bus, device, function) {
  }
  static DevPci *InitPci(uint8_t bus, uint8_t device, uint8_t function);
};

class AhciChannel {
public:
  AhciChannel(AhciCtrl *ctrl) : _ctrl(ctrl) {
  }
  static AhciChannel *Init(AhciCtrl *ctrl);
  device_t GetDevice() {
    return &_bsd;
  }
private:
  AhciCtrl *_ctrl;
  BsdDevice _bsd;
};

#endif // __RAPH_KERNEL_DEV_DISK_AHCI_AHCI_RAPH_H__