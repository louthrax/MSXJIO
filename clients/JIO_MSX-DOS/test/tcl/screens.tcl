# Run headless and dump the screen (text) at the times of SCREEN_TIMES, then quit.
set renderer none
set throttle off
proc dump {n} {
    set f [open "screen_$n.txt" w]
    if {[catch {puts $f [get_screen]} e]} { puts $f "(screen: $e)" }
    close $f
}
set times [expr {[info exists ::env(SCREEN_TIMES)] ? $::env(SCREEN_TIMES) : "15 30 45 60"}]
foreach t $times { after time $t "dump $t" }
after time [expr {[lindex $times end] + 1}] { exit }
