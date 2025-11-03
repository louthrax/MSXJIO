set renderer none
diskmanipulator create ./Tmp/disk.dsk 720
diska ./Tmp/disk.dsk
diskmanipulator import diska ./MSX-DOS2/ ./Tmp/main
diskmanipulator rename diska MAIN JIO.COM
exit
