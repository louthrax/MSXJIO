                                INCLUDE "../JIO_MSX-DOS/drv_jio.inc"

Hook:
                                di
                                ld                              (SPSave),sp
                                ld                              sp,g_aoRegisters+10
                                push                            ix
                                push                            de
                                push                            hl
                                push                            af
                                push                            bc
                                ld                              sp,(SPSave)

                                call                            bDoCommand

                                or                              a
                                jr                              z,OriginalCode

                                ld                              sp,g_aoRegisters
                                pop                             bc
                                pop                             af
                                pop                             hl
                                pop                             de
                                pop                             ix
                                ld                              sp,(SPSave)
                                ret

OriginalCode:
                                ld                              sp,g_aoRegisters
                                pop                             bc
                                pop                             af
                                pop                             hl
                                pop                             de
                                pop                             ix
                                ld                              sp,(SPSave)

                                defb                            0xC3
Hook_OriginalCode:              nop
                                nop

SPSave:                         defw                            0

g_aoRegisters:                  defw                            0,0,0,0,0
g_oCommonHeader:                defb                            'J', 'I', 'O', 0, COMMAND_BDOS, 0
