picotool load bin/Colossus249.bin -o 0x10100000 -f
sleep 2
picotool load state/Core.bin -o 0x10200000 -f
sleep 2
picotool load resources/profile.bin -o 0x10300000 -f
sleep 2