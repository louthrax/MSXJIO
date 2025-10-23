
_ENT_AUTO_DIRECT_L09:
                                pop                             hl
                                push                            bc
                                push                            de
                                push                            ix
                                ld                              ix,0
                                add                             ix,sp
                                ld                              e,(hl)
                                inc                             hl
                                ld                              d,(hl)
                                inc                             hl
                                ex                              de,hl
                                add                             hl,sp
                                ld                              sp,hl
                                ex                              de,hl
                                jp                              (hl)

_LEAVE_DIRECT_L09:
                                ld                              sp,ix
                                pop                             ix
                                pop                             de
                                pop                             bc
                                ret

_ENT_PARM_DIRECT_L09:
                                pop                             hl
                                push                            bc
                                push                            de
                                push                            ix
                                ld                              ix,0
                                add                             ix,sp
                                jp                              (hl)

_CALL_IND_L09:
                                jp                              (hl)


_STRLEN_L11:
                                push                            bc
                                push                            af

                                xor                             a
                                ld                              b,a
                                ld                              c,a
                                cpir
                                ld                              hl,0xFFFF
                                sbc                             hl,bc

                                pop                             af
                                pop                             bc
                                ret
