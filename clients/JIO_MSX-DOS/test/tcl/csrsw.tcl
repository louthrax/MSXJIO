# CSRSW (FCA9H, cursor shown by CHPUT) written to csrsw.txt at 30, 40, 60 and 80 s (MSX-DOS 2 started)
foreach t {30 40 60 80} {
    after time $t [list apply {{t} {
        set f [open "csrsw.txt" a]
        puts $f [format "%d CSRSW=%02X" $t [peek 0xFCA9]]
        close $f
    }} $t]
}
