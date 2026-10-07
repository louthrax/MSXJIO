# Run headless, type TYPE_TEXT at TYPE_TIME (e.g. a command at the prompt after a warm restart),
# dump the screen (text) at the times of SCREEN_TIMES, then quit.
set renderer none
set throttle off
after time $::env(TYPE_TIME) { type $::env(TYPE_TEXT) }
proc dump {n} {
    set f [open "screen_$n.txt" w]
    if {[catch {puts $f [get_screen]} e]} { puts $f "(screen: $e)" }
    close $f
}
set times [expr {[info exists ::env(SCREEN_TIMES)] ? $::env(SCREEN_TIMES) : "15 30 45 60"}]
foreach t $times { after time $t "dump $t" }
after time [expr {[lindex $times end] + 1}] { exit }
