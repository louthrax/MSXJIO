#ifndef STUB_H
#define STUB_H

#define STUB_TRANSMIT       0x00
#define STUB_RECEIVE        0x03
#define STUB_XFER           0x06
#define STUB_FROM_CALLER    0x09
#define STUB_TO_CALLER      0x0C
#define STUB_ORIGINAL       0x0F
#define STUB_ENTRY          0x12
#define STUB_GET_P2         0x15
#define STUB_HOOK_ORIGINAL  0x18
#define STUB_HOOK           0x1B
#define STUB_REGISTERS      0x1E
#define STUB_DRIVES         0x28
#define STUB_HAS_TURBO      0x30
#define STUB_SEGMENT        0x31
#define STUB_AUTO_RETRY     0x32
#define STUB_PORT           0x33
#define STUB_TX_BLOCKS      0x34
#define STUB_DOS1           0x35
#define STUB_BOUNCE         0x36
#define STUB_BOUNCE_SIZE    64
#define STUB_DPB            0x76

#define DRIVER_BASE         0x8000
#define DRIVER_STACK        0xC000

#endif
