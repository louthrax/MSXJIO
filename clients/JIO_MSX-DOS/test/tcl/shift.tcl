# TIME_SHIFT (environment, seconds): the timed actions of the scenario script (after time, at its top level) are
# delayed, e.g. for the boot of MSX-DOS 1 and JIO-ROM.COM before the JIO ROM (the delays set in procs are kept)
rename after _after
proc after {args} {
    if {[info level] == 1 && [lindex $args 0] eq "time"} {
        lset args 1 [expr {[lindex $args 1] + $::env(TIME_SHIFT)}]
    }
    uplevel 1 [list _after {*}$args]
}
