# Run headless, dump the screen at FIRST_TIME, replace drive/AUTOEXEC.BAT by REBOOT_AUTOEXEC,
# reset the MSX, dump the screen again SECOND_TIME seconds later, then quit.
set renderer none
set throttle off
proc dump {n} {
    set f [open "screen_$n.txt" w]
    if {[catch {puts $f [get_screen]} e]} { puts $f "(screen: $e)" }
    close $f
}
proc reboot {} {
    set f [open "drive/AUTOEXEC.BAT" w]
    fconfigure $f -translation binary
    puts -nonewline $f [subst -nocommands -novariables $::env(REBOOT_AUTOEXEC)]
    close $f
    reset
}
set t1 $::env(FIRST_TIME)
set t2 [expr {$t1 + $::env(SECOND_TIME)}]
after time $t1 "dump $t1; reboot"
after time $t2 "dump $t2"
after time [expr {$t2 + 1}] { exit }
