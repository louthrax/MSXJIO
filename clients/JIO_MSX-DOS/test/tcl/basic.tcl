# Disk BASIC test: each command is typed when BASIC shows "Ok" (keys typed during
# disk accesses are lost), the screen is dumped after RUN and at the end.
# BASIC_DRIVE (environment): drive used by the test (default A).
set renderer none
set throttle off
set drv [expr {[info exists ::env(BASIC_DRIVE)] ? $::env(BASIC_DRIVE) : "A"}]
proc dump {n} {
    set f [open "screen_$n.txt" w]
    if {[catch {puts $f [get_screen]} e]} { puts $f "(screen: $e)" }
    close $f
}
set cmds [list \
    "10 OPEN \"$drv:DATA.TXT\" FOR OUTPUT AS #1" \
    "20 PRINT #1,\"LINE ONE\"" \
    "30 CLOSE #1" \
    "40 OPEN \"$drv:DATA.TXT\" FOR INPUT AS #1" \
    "50 LINE INPUT #1,A\$:PRINT A\$" \
    "60 CLOSE" \
    "RUN" \
    "SAVE\"$drv:PROG.BAS\"" \
    "KILL\"$drv:DATA.TXT\"" \
    "NEW" \
    "LOAD\"$drv:PROG.BAS\"" \
    "LIST 10-20" \
    "FILES\"$drv:\"" ]
# Last non empty line of the screen
proc last_line {} {
    if {[catch {set s [get_screen]}]} { return "" }
    set last ""
    foreach l [split $s "\n"] { if {[string trim $l] ne ""} { set last [string trim $l] } }
    return $last
}
set ::step 0
set ::waiting 0
proc next {} {
    # program lines do not print "Ok": only wait after commands
    if {$::step >= [llength $::cmds]} { after time 3 { dump end; exit }; return }
    set c [lindex $::cmds $::step]
    set is_line [string is digit [string index $c 0]]
    if {$is_line || [last_line] eq "Ok" || [incr ::waiting] > 30} {
        set ::waiting 0
        if {$::step > 0 && [lindex $::cmds [expr {$::step - 1}]] eq "RUN"} { dump run }
        type "$c\r"
        incr ::step
        after time [expr {$is_line ? 1 : 2}] next
    } else {
        after time 1 next
    }
}
after time 16 next
# safety net
after time 170 { dump end; exit }
