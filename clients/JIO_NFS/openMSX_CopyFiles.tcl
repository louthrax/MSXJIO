set renderer none
diskmanipulator create ./Tmp/disk.dsk 720
diska ./Tmp/disk.dsk
diskmanipulator import diska ./MSX-DOS2/AUTOEXEC.BAT ./MSX-DOS2/COMMAND2.COM ./MSX-DOS2/MSXDOS2.SYS ./Tmp/main
diskmanipulator rename diska MAIN JIO.COM
exit
