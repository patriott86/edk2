/** @file
  UEFI NVM Express Pass Thru Protocol.
  
  This protocol allows interaction with NVMe devices in a UEFI environment.

  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __NVM_EXPRESS_PASS_THRU_H__
#define __NVM_EXPRESS_PASS_THRU_H__

#include <Uefi/UefiBaseType.h>
#include <Protocol/DevicePath.h>

#define EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL_GUID \
  { 0x52c78312, 0x8edc, 0x4233, { 0x98, 0xf2, 0x1a, 0x1f, 0x3c, 0x34, 0x51, 0x96 } }

typedef struct _EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL;

// NVMe Command Set Opcodes
#define NVME_ADMIN_IDENTIFY                 0x06
#define NVME_ADMIN_SET_FEATURES             0x09
#define NVME_ADMIN_GET_FEATURES             0x0A

// Define the PassThru function
typedef
EFI_STATUS
(EFIAPI *EFI_NVM_EXPRESS_PASS_THRU)(
  IN EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL    *This,
  IN UINT32                                 NamespaceId,
  IN OUT VOID                               *CommandPacket,
  IN VOID                                   *Buffer,
  IN OUT UINTN                              *BufferSize,
  IN UINT64                                 Timeout
  );

struct _EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL {
  EFI_NVM_EXPRESS_PASS_THRU PassThru;
};

extern EFI_GUID gEfiNvmExpressPassThruProtocolGuid;

#endif

