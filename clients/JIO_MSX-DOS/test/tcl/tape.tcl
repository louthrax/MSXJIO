# JIO-ROM.CAS (in the cassette player): SHIFT held at boot if SHIFT_BOOT is set (disk ROMs disabled), BLOAD"CAS:",R typed
# at TAPE_TIME (default 10 s), screen (text) dumped at the times of SCREEN_TIMES, then quit.
set renderer none
set throttle off
if {[info exists ::env(SHIFT_BOOT)] && $::env(SHIFT_BOOT) ne ""} {
    keymatrixdown 6 0x01
    after time 6 { keymatrixup 6 0x01 }
}
set tt [expr {[info exists ::env(TAPE_TIME)] ? $::env(TAPE_TIME) : 10}]
after time $tt { type "BLOAD\"CAS:\",R\r" }
proc dump {n} {
    set f [open "screen_$n.txt" w]
    if {[catch {puts $f [get_screen]} e]} { puts $f "(screen: $e)" }
    close $f
}
set times [expr {[info exists ::env(SCREEN_TIMES)] ? $::env(SCREEN_TIMES) : "20 200 240"}]
foreach t $times { after time $t "dump $t" }
after time [expr {[lindex $times end] + 1}] { exit }
