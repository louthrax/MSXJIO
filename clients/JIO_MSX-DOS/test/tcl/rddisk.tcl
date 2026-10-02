# Export the files of floppy.dsk (or DISK_FILE) to the directory floppy_out (or DISK_OUT).
set renderer none
diska [expr {[info exists ::env(DISK_FILE)] ? $::env(DISK_FILE) : "floppy.dsk"}]
diskmanipulator export diska [expr {[info exists ::env(DISK_OUT)] ? $::env(DISK_OUT) : "floppy_out"}]/
after time 1 { exit }
