inkscape=/Applications/Inkscape.app/Contents/MacOS/inkscape
insvg=$1

outdir=svg2icns.iconset
mkdir $outdir
for sz in 16 32 128 256 512
do
    echo "[+] Generate ${sz}x${sz} png..."
    $inkscape --export-filename ${outdir}/icon_${sz}x${sz}.png -w $sz -h $sz $insvg
    $inkscape --export-filename ${outdir}/icon_${sz}x${sz}@2x.png -w $((sz*2)) -h $((sz*2)) $insvg
done
iconutil --convert icns --output $2 ${outdir}
rm -rf ${outdir}
