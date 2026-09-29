# Create floppy.dsk (FLOPPY_SIZE KB, default 720) with the files of the directory FLOPPY_FILES.
set renderer none
set size  [expr {[info exists ::env(FLOPPY_SIZE)] ? $::env(FLOPPY_SIZE) : 720}]
diskmanipulator create floppy.dsk $size
diska floppy.dsk
if {[info exists ::env(FLOPPY_FILES)]} { diskmanipulator import diska $::env(FLOPPY_FILES)/ }
after time 1 { diska eject; exit }
