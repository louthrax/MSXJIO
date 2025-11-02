diska ./Tmp/disk.dsk

set auto_enable_reverse on

debug set_watchpoint write_io 0x2D

ext debugdevice
ext msxdos2

set throttle off
set fullspeedwhenloading on
