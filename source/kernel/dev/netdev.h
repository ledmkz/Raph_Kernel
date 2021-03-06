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

#ifndef __RAPH_KERNEL_NETDEV_H__
#define __RAPH_KERNEL_NETDEV_H__

#include <stdint.h>
#include <string.h>
#include <buf.h>
#include <function.h>
#include <spinlock.h>
#include <polling.h>
#include <freebsd/sys/param.h>

class ProtocolStack;
class DevEthernet;

class NetDev {
public:
  struct Packet {
    size_t len;
    uint8_t buf[MCLBYTES];
  };
  enum class LinkStatus {
    kUp,
    kDown,
  };
  enum class HandleMethod {
    kInt,
    kPolling,
  };

  typedef RingBuffer<Packet *, 300> NetDevRingBuffer;
  typedef FunctionalRingBuffer<Packet *, 300> NetDevFunctionalRingBuffer;

  // rxパケットの処理の流れ
  // 0. rx_reservedを初期化、バッファを満タンにしておく
  // 1. Receiveハンドラがパケットを受信すると、rx_reservedから一つ取り出し、
  //    memcpyの上、rx_bufferedに詰む
  // 2. プロトコル・スタックはReceivePacket関数を呼ぶ
  // 3. ReceivePacket関数はrx_bufferedからパケットを取得する
  // 4. プロトコル・スタックは取得したパケットを処理した上でReuseRxBufferを呼ぶ
  // 5. ReuseRxBufferはrx_reservedにバッファを返す
  // 6. 1に戻る
  //
  // プロトコル・スタックがReuseRxBufferを呼ばないと
  // そのうちrx_reservedが枯渇して、一切のパケットの受信ができなくなるるよ♪
  NetDevRingBuffer _rx_reserved;
  NetDevFunctionalRingBuffer _rx_buffered;
  NetDevFunctionalRingBuffer _rx_filtered;

  // txパケットの処理の流れ
  // 0. tx_reservedを初期化、バッファを満タンにしておく
  // 1. プロトコル・スタックはGetTxBufferを呼び出す
  // 2. GetTxBufferはtx_reservedからバッファを取得する
  // 3. プロトコル・スタックはバッファにmemcpyして、TransmitPacket関数を呼ぶ
  // 4. TransmitPacket関数はtx_bufferedにパケットを詰む
  // 5. Transmitハンドラがパケットを処理した上でtx_reservedに返す
  // 6. 1に戻る
  //
  // プロトコル・スタックはGetTxBufferで確保したバッファを必ずTransmitPacketするか
  // ReuseTxBufferで開放しなければならない。サボるとそのうちtx_reservedが枯渇
  // して、一切のパケットの送信ができなくなるよ♪
  NetDevRingBuffer _tx_reserved;
  NetDevFunctionalRingBuffer _tx_buffered;

  void ReuseRxBuffer(Packet *packet) {
    kassert(_rx_reserved.Push(packet));
  }
  void ReuseTxBuffer(Packet *packet) {
    kassert(_tx_reserved.Push(packet));
  }
  // 戻り値がfalseの時はバッファが枯渇しているので、要リトライ
  bool GetTxPacket(Packet *&packet) {
    if (_tx_reserved.Pop(packet)) {
      packet->len = 0;
      return true;
    } else {
      return false;
    }
  }
  bool TransmitPacket(Packet *packet) {
    PrepareTxPacket(packet);
    return _tx_buffered.Push(packet);
  }
  bool ReceivePacket(Packet *&packet) {
    return _rx_filtered.Pop(packet);
  }
  void SetReceiveCallback(int cpuid, const GenericFunction &func) {
    _rx_filtered.SetFunction(cpuid, func);
  }

  void InitTxPacketBuffer() {
    while(!_tx_reserved.IsFull()) {
      Packet *packet = reinterpret_cast<Packet *>(virtmem_ctrl->Alloc(sizeof(Packet)));
      kassert(_tx_reserved.Push(packet));
    }
  }
  void InitRxPacketBuffer() {
    while(!_rx_reserved.IsFull()) {
      Packet *packet = reinterpret_cast<Packet *>(virtmem_ctrl->Alloc(sizeof(Packet)));
      kassert(_rx_reserved.Push(packet));
    }
  }

  virtual void UpdateLinkStatus() = 0;
  void SetStatus(LinkStatus status) {
    _status = status;
  }
  volatile LinkStatus GetStatus() {
    return _status;
  }

  void SetName(const char *name) {
    strncpy(_name, name, kNetworkInterfaceNameLen);
  }
  const char *GetName() { return _name; }
  void SetHandleMethod(HandleMethod method) {
    switch(method) {
    case HandleMethod::kInt: {
      ChangeHandleMethodToInt();
      break;
    }
    case HandleMethod::kPolling: {
      ChangeHandleMethodToPolling();
      break;
    }
    default: {
      kassert(false);
    }
    }
    _method = method;
  }
  HandleMethod GetHandleMethod() {
    return _method;
  }
  void SetupNetInterface();
  void SetProtocolStack(ProtocolStack *stack) { _ptcl_stack = stack; }
protected:
  NetDev() {
    ClassFunction<NetDev> packet_filter;
    packet_filter.Init(this, &NetDev::FilterRxPacket, nullptr);
    // TODO cpuid
    _rx_buffered.SetFunction(2, packet_filter);

    // TODO cpuid
    ClassFunction<NetDev> func;
    func.Init(this, &NetDev::Transmit, nullptr);
    _tx_buffered.SetFunction(0, func);
  }
  virtual void ChangeHandleMethodToPolling() = 0;
  virtual void ChangeHandleMethodToInt() = 0;
  virtual void Transmit(void *) = 0;
  SpinLock _lock;
  PollingFunc _polling;
  volatile LinkStatus _status = LinkStatus::kDown;
  HandleMethod _method = HandleMethod::kInt;

  // filter received packet
  virtual void FilterRxPacket(void *p) = 0;

  // preprocess on packet before transmit
  virtual void PrepareTxPacket(NetDev::Packet *packet) = 0;

private:
  static const uint32_t kNetworkInterfaceNameLen = 8;
  // network interface name
  char _name[kNetworkInterfaceNameLen];

  // reference to protocol stack
  ProtocolStack *_ptcl_stack;
};

class NetDevCtrl {
public:
  struct NetDevInfo {
    NetDev *device;
    ProtocolStack *ptcl_stack;
  };

  NetDevCtrl() {}

  bool RegisterDevice(NetDev *dev, const char *name = kDefaultNetworkInterfaceName);
  NetDevInfo *GetDeviceInfo(const char *name = kDefaultNetworkInterfaceName);

protected:
  static const uint32_t kMaxDevNumber = 32;

private:
  static const uint32_t kNetworkInterfaceNameLen = 8;
  static const char *kDefaultNetworkInterfaceName;
  uint32_t _current_device_number = 0;
  NetDevInfo _dev_table[kMaxDevNumber];
};

#endif /* __RAPH_KERNEL_NETDEV_H__ */
