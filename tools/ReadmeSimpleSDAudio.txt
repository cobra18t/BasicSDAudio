Windows users:
- Download SoX from http://sox.sourceforge.net (choose ZIP distribution) 
  and unzip everything to this folder.
- Drag and drop your .WAV-files to the appropriate .bat files for conversion
- Converted files will go to folder "converted"

Linux users:
- Install SoX by compiling it from source or using your favourite
  package manager
- use the following line for conversion:

sox inputfile.wav --norm=-1 -e unsigned-integer -b 8 -r 31250 -c 1 -t raw outputfile.raw

- For stereo change -c 1 to -c 2
- For full rate use -r 62500 @ 16MHz, -r 31250 @ 8 MHz
- For half rate use -r 31250 @ 16MHz, -r 15625 @ 8 MHz