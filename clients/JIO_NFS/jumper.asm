#include "../../common/msx.inc"

                                di

                                ; Patch ENASLT stub
                                ld                              hl,ENASLT
                                ld                              de,my_enaslt
                                ld                              bc,4
                                ldir

                                ; Load main BIOS in page 0
                                ld                              a,(EXPTBL)
                                ld                              h,0x00
                                call                            my_enaslt

                                ; Load BASIC in page 1
                                ld                              a,(EXPTBL)
                                ld                              h,0x40
                                call                            0x24

sethook:
                                ld                              hl,call_system
                                push                            hl
                                xor                             a
                                ld                              hl,0xF41F
                                ld                              (0xF860),hl
                                ld                              hl,0xF423
                                ld                              (0xF41F),hl
                                ld                              (hl),a
                                ld                              hl,0xF52C
                                ld                              (0xF421),hl
                                ld                              (hl),a
                                ld                              hl,0xF42C
                                ld                              (0xF862),hl
                                pop                             hl
                                jp                              NEWSTT

my_enaslt:
                                defb                            0,0,0,0

call_system:
                                defm                            ":_SYSTEM"
                                defb                            0
