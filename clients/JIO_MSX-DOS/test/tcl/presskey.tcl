# "Press STOP on the tape player, then press a key" (JIO-ROM.CAS loader, CAS2TAPE.COM): screen dumped in screen_key.txt, then a key
# typed, each time it is shown
set ::keyseen 0
proc presskey {} {
    if {![catch {set s [get_screen]}] && [string first "then press a key" $s] >= 0} {
        incr ::keyseen
        set f [open "screen_key$::keyseen.txt" w]
        puts $f $s
        close $f
        type " "
        after time 3 presskey
        return
    }
    after time 1 presskey
}
after time 5 presskey
