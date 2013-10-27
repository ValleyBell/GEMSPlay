GEMS Music Player
-----------------
by Valley Bell


There are 3 ways to play music without the commandline:
- double-click GemsPlay (uses files set in config.ini)
- drop a sequence file on GemsPlay (uses files set in config.ini, excepct for sequence data)
- drop the 4 GEMS data files on GemsPlay. They must be sorted so that their order is Instrument, Envelope, Sequences, Samples. Then you need to start dragging on the Instrument file.

If you feel comfortable with the commandline (or .bat files), the possible calls are:
GEMSPlay.exe
GEMSPlay.exe SeqData.bin
GEMSPlay.exe InsData.bin EnvData.bin SeqData.bin SmpData.bin
GEMSPlay.exe ROM.bin SeqPtr
GEMSPlay.exe ROM.bin InsPtr EnvPtr SeqPtr SmpPtr

Hexadecimal pointers need to start with "0x".



Keys:
Cursor Up/Down - select song
Return - play song
S - stop selected sequence
L - stop all sequences
T - auto-stop all (before starting new sequence)
F - follow sequences (changes selected song when new sequence is started by the song)
P/Space - Pause
A - auto-progess to next song after song end
V - enable VGM logging



Notes:
- GEMS 2.8 uses a slightly different pointer format in sequence files. If a sequence bank uses this format, GemsPlay will show it.
  If the format autodetection comes up with an invalid result, please report it.
- In GEMS you can do loops in various ways, so there is no loop detection.
- Some games (like Aladdin) have sequences that change the master volume (e.g. for fading). This can mute the music until you play a sequence that resets the master volume.
