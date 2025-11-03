diskmanipulator create ./Tmp/disk.dsk 720
diska ./Tmp/disk.dsk
diskmanipulator import diska ./MSX-DOS2/ ./Tmp/main
diskmanipulator rename diska MAIN JIO.COM

diskmanipulator create ./Tmp/diskb.dsk 720
diskb ./Tmp/diskb.dsk
diskmanipulator import diskb ./MSX-DOS2/ ./Tmp/main


set auto_enable_reverse off

debug set_watchpoint write_io 0x2D

ext debugdevice
ext msxdos2

set throttle off
set fullspeedwhenloading on

proc regsdump {} {
    puts [format "A=%02X BC=%04X DE=%04X HL=%04X  IX=%04X IY=%04X  SP=%04X" \
        [reg a] [reg bc] [reg de] [reg hl] [reg ix] [reg iy] [reg sp]]
}

proc hexdump {start size {cols 16}} {
    set end   [expr {$start + $size}]
    for {set base $start} {$base < $end} {incr base $cols} {
        set lineCount [expr {($end - $base) < $cols ? ($end - $base) : $cols}]
        set hexstr ""
        set ascii  ""

        for {set i 0} {$i < $cols} {incr i} {
            if {$i < $lineCount} {
                set b [peek [expr {$base + $i}]]

                append hexstr [format "%02X " $b]
                if {$b >= 32 && $b < 127} {
                    append ascii [format "%c" $b]
                } else {
                    append ascii "."
                }
            } else {
                append hexstr "   "
            }
            # Extra gap between the two 8-byte halves
            if {$i == 7} { append hexstr " " }
        }

        puts [format "%04X  %s |%s|" $base $hexstr $ascii]
    }
}

proc read_asciiz {addr {max 256}} {
    set s ""
    for {set i 0} {$i < $max} {incr i} {
        set b [peek [expr {$addr + $i}]]
        if {$b == 0} break
        append s [format %c $b]
    }
    return $s
}

proc show_attributes {value} {
    set attrs {
        1   ATTRIBUTE_READ_ONLY
        2   ATTRIBUTE_HIDDEN_FILE
        4   ATTRIBUTE_SYSTEM_FILE
        8   ATTRIBUTE_VOLUME_NAME
        16  ATTRIBUTE_DIRECTORY
        32  ATTRIBUTE_ARCHIVE_BIT
        64  ATTRIBUTE_RESERVED
        128 ATTRIBUTE_DEVICE_BIT
    }

    set result {}
    foreach {mask name} $attrs {
        if {$value & $mask} {
            lappend result $name
        }
    }

    if {[llength $result] == 0} {
        puts "No attributes set"
    } else {
        puts "Attributes: [join $result {, }]"
    }
}


set msxDos2Errors {
    0xFF "NCOMP"
    0xFE "WRERR"
    0xFD "DISK"
    0xFC "NRDY"
    0xFB "VERFY"
    0xFA "DATA"
    0xF9 "RNF"
    0xF8 "WPROT"
    0xF7 "UFORM"
    0xF6 "NDOS"
    0xF5 "WDISK"
    0xF4 "WFILE"
    0xF3 "SEEK"
    0xF2 "IFAT"
    0xF1 "NOUPB"
    0xF0 "IFORM"
    0xDF "INTER"
    0xDE "NORAM"
    0xDC "IBDOS"
    0xDB "IDRV"
    0xDA "IFNM"
    0xD9 "IPATH"
    0xD8 "PLONG"
    0xD7 "NOFIL"
    0xD6 "NODIR"
    0xD5 "DRFUL"
    0xD4 "DKFUL"
    0xD3 "DUPF"
    0xD2 "DIRE"
    0xD1 "FILRO"
    0xD0 "DIRNE"
    0xCF "IATTR"
    0xCE "DOT"
    0xCD "SYSX"
    0xCC "DIRX"
    0xCB "FILEX"
    0xCA "FOPEN"
    0xC9 "OV64K"
    0xC8 "FILE"
    0xC7 "EOF"
    0xC6 "ACCV"
    0xC5 "IPROC"
    0xC4 "NHAND"
    0xC3 "IHAND"
    0xC2 "NOPEN"
    0xC1 "IDEV"
    0xC0 "IENV"
    0xBF "ELONG"
    0xBE "IDATE"
    0xBD "ITIME"
    0xBC "RAMDX"
    0xBB "NRAMD"
    0xBA "HDEAD"
    0xB9 "EOL"
    0xB8 "ISBFN"
    0x9F "STOP"
    0x9E "CTRL_C"
    0x9D "ABORT"
    0x9C "OUTERR"
    0x9B "INERR"
    0x8F "BADCOM"
    0x8E "BADCM"
    0x8D "BUFUL"
    0x8C "OKCMD"
    0x8B "IPARM"
    0x8A "INP"
    0x89 "NOPAR"
    0x88 "IOPT"
    0x87 "BADNO"
    0x86 "NOHELP"
    0x85 "BADVER"
    0x84 "NOCAT"
    0x83 "BADEST"
    0x82 "COPY"
    0x81 "OVDEST"
    0x00 "OK"
}

proc show_msxdos_error {} {
    set code [format "0x%02X" [reg a]]

    if {$code in $::msxDos2Errors} {
        puts ".[dict get $::msxDos2Errors $code]"
    } else {
        puts [format "Error %02Xh: (unknown)" $code]
    }
}

set msxdosFuncs {
    0x00 "Program terminate"
    0x01 "Console input"
    0x02 "Console output"
    0x03 "Aux input"
    0x04 "Aux output"
    0x05 "Printer output"
    0x06 "Direct console I/O"
    0x07 "Direct console input"
    0x08 "Console input without echo"
    0x09 "String output"
    0x0A "Buffered line input"
    0x0B "Console status"
    0x0C "Return version number"
    0x0D "Disk reset"
    0x0E "Select disk"
    0x0F "Open file (FCB)"
    0x10 "Close file (FCB)"
    0x11 "Search first (FCB)"
    0x12 "Search next (FCB)"
    0x13 "Delete file (FCB)"
    0x14 "Sequential read (FCB)"
    0x15 "Sequential write (FCB)"
    0x16 "Create file (FCB)"
    0x17 "Rename file (FCB)"
    0x18 "Get login vector"
    0x19 "Get current drive"
    0x1A "Set DMA address"
    0x1B "Get allocation info"
    0x21 "Random read (FCB)"
    0x22 "Random write (FCB)"
    0x23 "Get file size (FCB)"
    0x24 "Set random record (FCB)"
    0x26 "Random block write (FCB)"
    0x27 "Random block read (FCB)"
    0x28 "Random write w/ zero fill (FCB)"
    0x2A "Get date"
    0x2B "Set date"
    0x2C "Get time"
    0x2D "Set time"
    0x2E "Set/Reset verify flag"
    0x2F "Absolute sector read"
    0x30 "Absolute sector write"
    0x31 "Get disk parameters"
    0x40 "Find first entry"
    0x41 "Find next entry"
    0x42 "Find new entry"
    0x43 "Open file handle"
    0x44 "Create file handle"
    0x45 "Close file handle"
    0x46 "Ensure file handle"
    0x47 "Duplicate file handle"
    0x48 "Read from file handle"
    0x49 "Write to file handle"
    0x4A "Move file handle pointer"
    0x4B "I/O control for devices"
    0x4C "Test file handle"
    0x4D "Delete file or subdir"
    0x4E "Rename file or subdir"
    0x4F "Move file or subdir"
    0x50 "Get/set file attributes"
    0x51 "Get/set file date/time"
    0x57 "Get DMA address"
    0x58 "Get verify flag"
    0x59 "Get current directory"
    0x5A "Change current directory"
    0x5B "Parse pathname"
    0x5C "Parse filename"
    0x5D "Check character"
    0x5E "Get whole path"
    0x5F "Flush disk buffers"
    0x60 "Fork child process"
    0x61 "Rejoin parent process"
    0x62 "Terminate with error"
    0x63 "Define abort routine"
    0x64 "Define disk error handler"
    0x65 "Get previous error"
    0x66 "Explain error"
    0x67 "Format disk"
    0x68 "Create/destroy RAM disk"
    0x69 "Allocate sector buffers"
    0x6A "Logical drive assignment"
    0x6B "Get environment item"
    0x6C "Set environment item"
    0x6D "Find environment item"
    0x6E "Get/set disk check status"
    0x6F "Get DOS version number"
    0x70 "Get/set redirection status"
}

set funcsToDiscard {  }


debug set_bp 0xEC10 {} {

    set ::command [format "0x%02X" [reg c]]

    if {![expr {$::command in $::funcsToDiscard}]} {
        puts "------------------------------------------------"
        puts [format "%s: %s" $::command [dict get $::msxdosFuncs $::command]]

        switch $::command {
            0x43 { hexdump [reg de] 64; regsdump }
        }
    }    
}

debug set_bp 0xEC13 {} {
    if {![expr {$::command in $::funcsToDiscard}]} {
        puts "->"
        switch $::command {
            0x43 { hexdump [reg ix] 64; regsdump; show_msxdos_error }
        }
    }
}    
