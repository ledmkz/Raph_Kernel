/******************************************************************************

  Copyright (c) 2001-2015, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
/*$FreeBSD$*/

#include "e1000_api.h"
#include <dev/pci.h>

/*
 * NOTE: the following routines using the e1000 
 * 	naming style are provided to the shared
 *	code but are OS specific
 */

void
e1000_write_pci_cfg(struct e1000_hw *hw, u32 reg, u16 *value)
{
  ((struct e1000_osdep *)hw->back)->dev->GetPciClass()->WriteReg<uint16_t>(static_cast<uint16_t>(reg), *value);
}

void
e1000_read_pci_cfg(struct e1000_hw *hw, u32 reg, u16 *value)
{
  *value = ((struct e1000_osdep *)hw->back)->dev->GetPciClass()->ReadReg<uint16_t>(static_cast<uint16_t>(reg));
}

void
e1000_pci_set_mwi(struct e1000_hw *hw)
{
  ((struct e1000_osdep *)hw->back)->dev->GetPciClass()->WriteReg<uint16_t>(PciCtrl::kCommandReg, (hw->bus.pci_cmd_word | PciCtrl::kCommandRegMemWriteInvalidateFlag));
}

void
e1000_pci_clear_mwi(struct e1000_hw *hw)
{
  ((struct e1000_osdep *)hw->back)->dev->GetPciClass()->WriteReg<uint16_t>(PciCtrl::kCommandReg, (hw->bus.pci_cmd_word & ~PciCtrl::kCommandRegMemWriteInvalidateFlag));
}

/*
 * Read the Pci Express capabilities
 */
int32_t
e1000_read_pcie_cap_reg(struct e1000_hw *hw, u32 reg, u16 *value)
{
  DevPci *dev = ((struct e1000_osdep *)hw->back)->dev->GetPciClass();
  uint16_t offset = dev->FindCapability(PciCtrl::CapabilityId::kPcie);

  *value = dev->ReadReg<uint16_t>(static_cast<uint16_t>(offset + reg));
  return (E1000_SUCCESS);
}

/*
 * Write the Pci Express capabilities
 */
int32_t
e1000_write_pcie_cap_reg(struct e1000_hw *hw, u32 reg, u16 *value)
{
  DevPci *dev = ((struct e1000_osdep *)hw->back)->dev->GetPciClass();
  uint16_t offset = dev->FindCapability(PciCtrl::CapabilityId::kPcie);

  dev->WriteReg<uint16_t>(static_cast<uint16_t>(offset + reg), *value);
  return (E1000_SUCCESS);
}
