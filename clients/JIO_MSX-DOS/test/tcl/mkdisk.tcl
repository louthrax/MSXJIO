# Create floppy.dsk (or DISK_FILE) (FLOPPY_SIZE KB, default 720) with the files of the directory FLOPPY_FILES.
set renderer none
set size  [expr {[info exists ::env(FLOPPY_SIZE)] ? $::env(FLOPPY_SIZE) : 720}]
set file  [expr {[info exists ::env(DISK_FILE)] ? $::env(DISK_FILE) : "floppy.dsk"}]
diskmanipulator create $file $size
diska $file
if {[info exists ::env(FLOPPY_FILES)]} { diskmanipulator import diska $::env(FLOPPY_FILES)/ }
after time 1 { diska eject; exit }
