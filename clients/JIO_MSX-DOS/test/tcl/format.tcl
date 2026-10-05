# Run headless, type FORMAT B: (FORMAT_DRIVE) and answer its questions (FORMAT_CHOICE: choice of the driver if it asks for one,
# then any key to confirm), then COPY A:HELLO.TXT B: and DIR B:, dump the screen (text) at the times of SCREEN_TIMES, then quit.
set renderer none
set throttle off
set choice [expr {[info exists ::env(FORMAT_CHOICE)] ? $::env(FORMAT_CHOICE) : ""}]
set drive [expr {[info exists ::env(FORMAT_DRIVE)] ? $::env(FORMAT_DRIVE) : "B"}]
after time 20 { type "FORMAT $::drive:\r" }
if {$choice ne ""} {
    after time 24 { type $::choice }
    after time 28 { type "y" }
} else {
    after time 24 { type "y" }
}
after time 110 { type "COPY A:HELLO.TXT B:\r" }
after time 120 { type "DIR B:\r" }
proc dump {n} {
    set f [open "screen_$n.txt" w]
    if {[catch {puts $f [get_screen]} e]} { puts $f "(screen: $e)" }
    close $f
}
set times [expr {[info exists ::env(SCREEN_TIMES)] ? $::env(SCREEN_TIMES) : "23 27 40 130"}]
foreach t $times { after time $t "dump $t" }
after time [expr {[lindex $times end] + 1}] { exit }
