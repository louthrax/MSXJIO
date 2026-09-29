# Export the files of floppy.dsk to the directory floppy_out.
set renderer none
diska floppy.dsk
diskmanipulator export diska floppy_out/
after time 1 { exit }
