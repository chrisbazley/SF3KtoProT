# SF3KtoProT
Star Fighter 3000 to ProTracker convertor

(C) Christopher Bazley, 2009

Version 0.08 (21 May 2024)

-----------------------------------------------------------------------------
 1   Introduction and Purpose
-----------------------------

  'Star Fighter 3000' is a three dimensional combat-dedicated flight action
shooter which was first published in 1994 for 32-bit Acorn RISC OS computers.
This program converts the music tracks from their original esoteric file
format (understood only by the game's own music player) into standard Amiga
ProTracker V1.1B song/module format. Most of the original features of the
music can be replicated fairly accurately in ProTracker format.

  The game's own music and sound effects player is a relocatable module named
'SFX_Handler'. For brevity, I have referred to it by that name throughout the
remainder of this manual.

-----------------------------------------------------------------------------
2   Requirements
----------------

  The supplied executable file will only work on RISC OS machines. It has
not been tested on any version of RISC OS earlier than 4, although it
should also work on earlier versions provided that a 'SharedCLibrary' module
is active. It is known to be compatible with 32 bit versions of RISC OS.

  You will obviously require a copy of the game 'Star Fighter 3000' from
which to rip the music. Until release 2.02 of the game, the sound sample data
and music tracks were incorporated into 'SFX_Handler'. You need a version
where this data is instead supplied in separate files. For example, they are
included in the free upgrade patch to version 3.08:
http://starfighter.acornarcade.com/download/uni308.zip

  Compressed music data files can be found inside the !Star3000 application,
in the sub-directory named "Music". Raw sound sample data files suitable for
used with this program are located in the sub-directory "Samples".

-----------------------------------------------------------------------------
3   Quick Guide
---------------

  Ensure that the !Star3000 application has been 'seen' by the Filer, so that
the system variable Star3000$Dir has been set.

  Hold down the Shift key and double-click on the !Star3000 application to
open it. Copy the supplied 'index' file into the sub-directory that contains
sound sample data files (as !Star3000.Samples.index). This file should
require no modification because it has been tailored for the game (which
configures 'SFX_Handler' using similar parameters).

  Set the current selected directory to that containing the 'SF3KtoProT'
program. On RISC OS 6, that can be done by opening the relevant directory
display and choosing 'Set work directory' from the menu (or giving the
directory display the input focus and then pressing F11).

  Press Ctrl-F12 to open a task window and then invoke the music conversion
program using the following command:
```
  *SF3KtoProT -v <Star3000$Dir>.Samples <Star3000$Dir>.Music.BonusLevel b/mod
```
  This should convert the Star Fighter 3000 music file 'BonusLevel' into a
ProTracker module, using sound samples taken from the 'Samples'
directory inside the !Star3000 application and storing the output in a file
named 'b/mod'. The '-v' switch enables verbose output, which shows what is
happening during the conversion process.

  By varying the third argument to the 'SF3KtoProT' program, any of the
other files in the !Star3000.Music directory can also be converted to
ProTracker format.

  If the Filer has already 'seen' an application which claims file type
&CC5 ('TeqMusic' - allocated to Krisalis) then you should simply be able to
double-click on the output files in order to play the music. If that causes
an error box saying "An application that loads a file of this type has not
been found by the Filer" then you must explicitly load a tracker player such
as DigitalCD or Digital Symphony, before dragging the output file to the
player's iconbar icon.

-----------------------------------------------------------------------------
4   Detailed guide
------------------

4.1 Command line syntax
-----------------------
```
  SF3KtoProT [switches] <samples-dir> [<input-file> [<output-file>]]
  SF3KtoProT -batch [switches] <samples-dir> <file1> [<file2> .. <fileN>]
```
Switches (names may be abbreviated):
```
  -allowsfx           Allow notes to be played using sound effect samples
  -batch              Process a batch of files (see above)
  -blankend           Append a blank pattern to the end of the song
  -channelglissando   Restrict glissando effects to the same channel
  -extraoctaves       Utilise non-standard ProTracker octaves 0 and 4
  -help               Display this text
  -indexfile <file>   Index file to use instead of looking in <samples-dir>
  -name <song-name>   Name to give the song (default is the input file name)
  -outfile <file>     Specify a name for the output file
  -raw                Input is uncompressed raw data
  -verbose or -debug  Emit debug output
```

4.2 Sound sample files
----------------------
  When invoking 'SF3KtoProT', you must specify the name of a directory
containing the sound sample files to be incorporated in the output ProTracker
module. Unlike ProTracker modules, SF3000 music files do not incorporate
sound samples (which makes sense given that each piece uses a subset of the
same instruments). Suitable sound samples are supplied with the game, inside
the !Star3000.Samples directory.

  Because the sound sample files consist of raw data in 16 bit little-endian
signed linear format, there is nowhere to keep meta data. Rather than hard-
wiring the tuning value, repeat offset and type of each sample into the
conversion program, it reads this data from an index file (the format of
which is described in section 7.3). By default, it looks for an index file in
the same directory as the sample files, but you can force it to look
elsewhere by using the '-indexfile' switch. That may be useful if the game is
on read-only media such as a CD-ROM.

4.3 Single file mode
--------------------
  Single file mode is the default mode of operation. Unlike batch mode, the
input and output file names can be specified separately (or not at all). An
output file name can be specified after the input file name, or, by means of
the '-outfile' switch, before the name of the directory containing the sound
samples.

  If no input file is specified, input is read from 'stdin' (the standard
input stream; keyboard unless redirected). If no output file is specified,
output is written to 'stdout' (the standard output stream; screen unless
redirected).

  All of the following examples read input from a file named 'foo' and
write output to a file named 'bar':
```
  *SF3KtoProT <Star3000$Dir>.Samples foo bar
  *SF3KtoProT -outfile bar <Star3000$Dir>.Samples foo
  *SF3KtoProT -outfile bar <Star3000$Dir>.Samples <foo
  *SF3KtoProT <Star3000$Dir>.Samples foo >bar
  *SF3KtoProT <Star3000$Dir>.Samples <foo >bar
```
  Under UNIX-like operating systems, output can be piped directly into
another program.

  Convert a SF3000 music file named 'foo' into a compressed ProTracker
module file named 'bar.gz':
```
  SF3KtoProT ~/star3000/samples foo | gzip > bar.gz
```

4.4 Batch processing mode
-------------------------
  Batch processing is enabled by the switch '-batch'. In this mode, multiple
files can be processed using a single command. The output is always saved to
a file with a name derived from the input file's name, which means that
programs cannot be chained using pipes. At least one file name must be
specified.

  Convert a SF3000 music file named 'foo' to a ProTracker module file
named 'foo/mod':
```
  *SF3KtoProT -batch <Star3000$Dir>.Samples foo
```
  Convert SF3000 music files named 'foo', 'bar' and 'baz' to ProTracker
module files named 'foo/mod', 'bar/mod' and 'baz/mod':
```
  *SF3KtoProT -batch <Star3000$Dir>.Samples foo bar baz
```

4.5 Song names
--------------
  SF3000 music files do not incorporate a song name.

  By default, the name written in the ProTracker module is the leaf of
the input file path (e.g. 'BonusLevel' if the input file path was
'<Star3000$Dir>.Music.BonusLevel'). If input was read from the standard input
stream then the default song name is instead 'Star Fighter 3000'.

  If the switch '-name' is used then the following argument specifies an
alternative song name, which is truncated to 19 characters if necessary.

4.6 Getting diagnostic information
----------------------------------
  If either of the switches '-verbose' and '-debug' is used then the
program emits information about its internal operation on the standard
output stream. However, this makes it slower and prevents output being piped
to another program.

  When debugging output is enabled, you must specify an output file name.
This is to prevent the ProTracker module being sent to the standard
output stream and becoming mixed up with the diagnostic information.

4.7 Sound effects
-----------------
  Early versions of the 'SFX_Handler' module automatically blocked sound
effects whilst playing music. That distinction was made on the basis of the
sample type; sample numbers 0 to 11 are classified as effects whereas 12 to
22 are classified as instruments (surprisingly, 'Radio' is included in the
latter category).

  This restriction was removed in version 2.10 of 'SFX_Handler' (distributed
with release 2.02 of the game), because it didn't make any sense in the
context of a configurable number of channels (only 4 of which are used for
music). Doing so revealed hitherto inaudible parts of the 'Intro1' music
- notes played using the 'Wind' and 'TextBeep' samples. The extra notes were
stripped out prior to release 2.04 of the game, to restore the track's
original ambience.

  The samples index file ('-indexfile' switch) specifies the type of each
sample. The command line switch '-allowsfx' has been provided to allow
inclusion in the output file of notes played using samples designated as
sound effects. By default, such notes are stripped out. This option is only
likely to be useful if you have access to music files from an older version
of SF3000.

4.8 End of song
---------------
  Like most tracker music players, 'SFX_Handler' stops abruptly upon reaching
the last division of the pattern that was played at the final song position.
It cuts off any notes that are still playing, instead of allowing the samples
to finish playing (and perhaps repeat).

  This policy means that notes triggered late in the final pattern won't
actually be audible! For example, the final division of the last pattern of
the 'GameOver' music contains commands to play the 'String' sample at low
pitch on channels 0 and 2. These notes would round the tune off nicely, but
they are never heard in the game.

  If the command line switch '-blankend' is specified then an additional
'blank' pattern will be created in the output file and the play order
modified so that this extra pattern is played last, thus allowing late notes
to be heard. If the pattern played immediately beforehand contains glissando
effects (not yet overridden by new notes on the same channel) then the
so-called 'blank' pattern may actually contain Tone Portamento commands to
ensure that these glissandos reach their target pitch.

4.9 Glissando effects
---------------------
  An idiosyncrasy of 'SFX_Handler' is that glissando effects are applied to
all channels playing the sample mapped to the voice number specified by bits
16-19 of the channel data. In other words, if two channels are playing the
same sample then a glissando effect for that sample number will cause their
pitches to converge on a common value. The reason is that glissandos are
implemented using the SFX_Pitch SWI, which doesn't take a channel number as a
parameter (unlike SFX_Force, which is used to play notes).

  The command line switch '-channelglissando' can be used to restrict
glissando effects to the channel on which the command was issued. The
difference between the two modes of conversion is most obvious with the
'Intro1' music, where the final high note played on channel 4 is either held
or else slides towards the same target pitch as a note on channel 3.

  In a ProTracker module, glissando effects must be explicitly
continued by a series of Tone Portamento commands throughout their expected
duration, whereas 'SFX_Handler' continues to update the pitch of a note
automatically until its target pitch is reached or it is usurped by another
note on the same channel. That makes conversion between the two formats more
awkward.

  Each Tone Portamento command incorporates a value to govern the pitch slide
speed. Even the minimum speed of '1' is noticeably faster than the rate of
change when playing music through the 'SFX_Handler' module. (In early
versions of 'SFX_Handler', the increment in the phase accumulator pitch
oscillator value was updated at the start of alternate buffer fills, which
meant the slide speed was dependent upon the configured buffer size!)

  Version 0.01 of this converter recognised a '-fastglissando' switch to
output contiguous Tone Portmento commands instead of interleaving Tone
Portamento commands with Normal Play commands. 'Fast' glissandos are now the
only output mode supported. A bug in the versions of 'SFX_Handler' supplied
with releases 2.02 to 3.08 (inclusive) meant that glissandos were slower
than the original sound player with the default DMA buffer size.

4.10 ProTracker octave range
----------------------------
  The standard ProTracker pitch range is 3 octaves, whereas Star Fighter
3000's music format allows a range of 8 octaves (because 3 bits of channel
data are allocated for the octave number).

  After applying the tuning values to the default set of SF3000 samples, it
can be heard that octave numbers in a SF3000 music track are exactly one
higher than in a ProTracker module. However, some of the tuning values
are very large: 'String', 'BassHeavy', 'DrumHeavy' and 'HiHat' are tuned up
by at least one octave and 'Piano' is tuned down by nearly a whole octave!

  The following table shows the range of octave numbers actually used by each
of the music tracks supplied with Star Fighter 3000:

|  Track name | Untuned Min | Untuned Max | Tuned Min | Tuned Max | ProTracker Min | ProTracker Max |
|-------------|-------------|-------------|-----------|-----------|----------------|----------------|
|  BonusLevel | 2           | 5           | 2         | 4         | 1              | 3              |
|  FlightDem1 | 2           | 4           | 2         | 4         | 1              | 3              |
|  FlightDem2 | 2           | 6           | 3         | 5         | 2              | 4*             |
|  GameOver   | 1           | 3           | 1         | 3         | 0*             | 2              |
|  GameStart  | 2           | 3           | 2         | 4         | 1              | 3              |
|  HighScore  | 2           | 5           | 2         | 5         | 1              | 4*             |
|  Intro1     | 1           | 4           | 1         | 4         | 0*             | 3              |
|  MissionCom | 2           | 5           | 2         | 4         | 1              | 3              |
|  Toccatta   | 1           | 5           | 1         | 5         | 0*             | 4*             |
|  TopScore   | 1           | 6           | 1         | 5         | 0*             | 4*             |

  It can be seen that 'GameOver', 'Intro1', 'Toccatta' and 'TopScore' contain
notes which are too low to be represented within the standard ProTracker
octave range 1 to 3. Conversely, 'FlightDem2', 'HighScore', 'Tocatta', and
'TopScore' contain notes that are too high - even after certain samples have
been tuned down.

  The default solution adopted by 'SF3KtoProT' is to pre-tune certain
ProTracker samples to a higher or lower octave than the original audio data.
Hence, the output file may contain several different samples generated from
the same source (e.g. 'TopScore/mod' contains samples named 'BassLight-R0-O0'
and 'BassLight-R0-O-1' - the latter specifically for low notes).

  Andrew Scott gives a table of period values for a range of five octaves
rather than the standard three. However, this comes with the caveat that
octaves 0 and 4 are non-standard, unlike octaves 1 to 3. Some trackers may be
unable to play notes which use those periods, or might even crash! However,
Andre Timmermans's 'TimPlayer' module (for RISC OS) seems to cope.

  The command line switch '-extraoctaves' has been provided to enable use
of these non-standard period values in preference to generating alternative
versions of samples pre-tuned to a higher or lower octave. (The conversion
routine falls back to generating pre-tuned samples for notes that would be
more than one octave outside the standard range.)

-----------------------------------------------------------------------------
5   How it works
----------------

  Firstly, the sample definitions are read from the index file. Every sound
sample file named in the index is opened temporarily to determine its length
(which relies on being able to seek the end of the input stream) and then
closed again.

  The SF3000 music track to be converted to ProTracker format is loaded into
memory and then converted in two passes:

  The purpose of the first pass is merely to determine which samples (and
variants thereof) need to be included in the ProTracker module. Only
commands to play notes are examined; glissandos are ignored, as are any
notes played using samples designated as sound effects (unless '-allowsfx'
was specified).

  For each play note command, the sample's tuning value is converted to the
nearest whole semitone and used to adjust the octave and note numbers. At
this stage it becomes clear whether or not the final octave number will be
within the standard ProTracker range (or extended range, if '-extraoctaves'
was specified), and hence whether a version of the sample pre-tuned to a
higher or lower octave will be required.

  Because the ProTracker format does not provide any facility to loop a
sample a specific number of times (unlike 'SFX_Handler'), a separate version
of each sample will be needed for every distinct number of repeats.

  If creation of a ProTracker sample with the required number of repeats and
level of pre-tuning has not already been scheduled then a new entry is added
to the list of ProTracker samples to be created. This list is later used to
write the sample information table at the head of the output file.

  The purpose of second pass is to transcode the pattern data from SF3000 to
ProTracker module format.

  To generate glissando effects, the transcode routine must keep track of
which sample (if any) is believed to be playing on each channel, and the
target pitch if a glissando is affecting that channel. This record is reset
before transcoding each pattern - which isn't foolproof, but is the best that
can be done given that patterns may be played in any order.

  Each individual division of every pattern is also examined in two passes in
order to ensure that glissando effects are applied consistently across all
four channels (unless '-channelglissando' was specified):

  The first pass discovers any glissando effects and updates the virtual
channel state accordingly. The octave and note numbers which specify the
target pitch are converted to their ProTracker equivalents using the same
technique as before. However, this time the ProTracker sample has already
been chosen for each channel and the transcode routine must compensate for
the fact that different variants of the same basic sample (all affected by a
glissando effect) may have been pre-tuned to different octaves. The final
target octave for some channels may be outside the valid range, but nothing
can be done except outputting a warning and substituting a nearer octave.

  The second pass actually writes the appropriate ProTracker commands for
the division being transcoded: a Tone Portamento or Normal Play command if
there is no note play command in the SF3000 music data (or the sample number
is undefined); otherwise a Set Volume command to play the specified note. In
the former case, the virtual channels state is consulted to determine whether
a glissando is in progress, and to ensure that only the first Tone Portamento
command in the sequence specifies the sample number and target pitch.

  Immediately after transcoding the pattern to be played last (as specified
by the play order), a snapshot of the virtual channels state is taken to
allow continuation of any glissando effects on the additional 'blank' pattern
(if one is to be appended).

  Finally, the list of ProTracker samples awaiting creation is used to copy
audio data from the relevant files into the output file. Because the format
of the source data is 16 bit, whereas it is 8 bit in a ProTracker module,
the least significant bytes are discarded (the first of each pair, because
the data is little-endian).

  If the number of repeats for a sample is greater than 0 and less than 15
then the audio data between the repeat offset and the end of the file will be
duplicated the requisite number of times. It is expedient to treat 15 as
'loop forever', because that is the only kind of repeats supported by the
ProTracker format, and this keeps the output file size reasonable.

  Some of the ProTracker samples may need to be pre-tuned to a higher or
lower octave than the original audio data. That is done crudely by doubling
(or quadrupling) each sample value to lower the pitch by one (or two)
octaves. Alternatively it skips one of every two (or three out of four)
sample values to raise the pitch by one (or two) octaves.

-----------------------------------------------------------------------------
6   File formats
----------------

6.1 Compression format
----------------------
  Until release 2.02 of Star Fighter 3000, the music tracks were incorporated
directly into the 'SFX_Handler' module. In later releases, they were instead
supplied as individual files containing the same data but encoded using
Gordon Key's esoteric lossless compression algorithm (with file type &154).

  The first 4 bytes of a compressed file give the expected size of the data
when decompressed, as a 32 bit signed little-endian integer. Gordon Key's
file decompression module 'FDComp', which is presumably normative, rejects
input files where the top bit of the fourth byte is set (i.e. negative
values).

  Thereafter, the compressed data consists of tightly packed groups of 1, 8
or 9 bits without any padding between them or alignment with byte boundaries.
A decompressor must deal with two main types of directive: The first (store a
byte) consists of 1+8=9 bits and the second (copy previously-decompressed
data) consists of 1+9+8=18 or 1+9+9=19 bits.

The type of each directive is determined by whether its first bit is set:

0.   The next 8 bits of the input file give a literal byte value (0-255) to
   be written at the current output position.

     When attempting to compress input that contains few repeating patterns,
   the output may actually be larger than the input because each byte value
   is encoded using 9 rather than 8 bits.

1.   The next 9 bits of the input file give an offset (0-511) within the data
   already decompressed, relative to a point 512 bytes behind the current
   output position.

     If the offset is greater than or equal to 256 (i.e. within the last 256
   bytes written) then the next 8 bits give the number of bytes (0-255) to be
   copied to the current output position. Otherwise, the number of bytes
   (0-511) to be copied is encoded using 9 bits.

    If the read pointer is before the start of the output buffer then zeros
  should be written at the output position until it becomes valid again. This
  is a legitimate method of initialising areas of memory with zeros.

    It isn't possible to replicate the whole of the preceding 512 bytes in
  one operation.

  The decompressors written by the Fourth Dimension and David O'Shea always
copy at least 1 byte from the source offset, even if the compressed bitstream
specified 0 as the number of bytes to be copied. A well-written compressor
should not insert directives to copy 0 bytes and no instances are known in
the wild. CBLibrary's new decompressor will treat directives to copy 0 bytes
as invalid input.


6.2 Music data format
---------------------
  The table below shows the Star Fighter 3000 music data format, after the
contents of the file have been decompressed:

Header:

|  Offset    | Data
|------------|------------------------------------------------------
|  +0        | Tempo (metronome value in centiseconds, 0-255)
|  +1...     | Unused
|  +16...    | 16 byte table mapping voice numbers to sample numbers
|    +16     |   Sample number to use for voice 0
|    +17     |   Sample number to use for voice 1
|    +18     |   Sample number to use for voice 2 ...etc
|  +32       | Number of patterns in file - 1
|  +36...    | Unused
|  +40...    | 64 byte play order for patterns (terminated by 255)
|  +104...   | Patterns data (see format below)
|    +104... | 1024 bytes of data for pattern 0
|    +1128.. | 1024 bytes of data for pattern 1
|    +2152.. | 1024 bytes of data for pattern 2 ...etc

  The value at offset +32 is supposedly a 32 bit word, and probably signed
little-endian. However, there is no evidence to corroborate that because
'SFX_Handler' doesn't actually use this value. Patterns numbered higher than
254 would be inaccessible anyway because of the format of the play order.

Pattern:
|  Offset  | Data
|----------|---------------------------------
|  +0...   | 16 bytes of data for division 0
|  +16...  | 16 bytes of data for division 1
|  +32...  | 16 bytes of data for division 2
|  ...     |
|  +1008.. | 16 bytes of data for division 63

Division:
|  Offset | Data
|---------|-----------------------------
|  +0...  | Channel 1 data (0 for none)
|  +4...  | Channel 2 data (0 for none)
|  +8...  | Channel 3 data (0 for none)
|  +12... | Channel 4 data (0 for none)

Channel data:
|  Offset | Data
|---------|---------------------------------------------------
|  +0     | Bits 0-3: Note number (in semitones, 0 to 11)
|         | Bits 4-7: Unused
|  +1     | Bits 0-3: Octave number (0 to 7)
|         | Bits 4-7: Linear volume (0 to 15)
|  +2     | Bits 0-3: Voice number (0 to 15)
|         | Bits 4-7: Action (0 none, 1 play, or 2 glissando)
|  +3     | Bits 0-3: Unused
|         | Bits 4-7: Number of repeats (0 to 15)

  The sample number to be used is looked up from the specified voice number
using the voice table in the header.

  If action > 1 then a glissando effect is triggered, sliding all instances
of the specified sample towards the specified pitch. The volume and repeats
parameters of such commands are ignored.

  Otherwise a note is forced on the channel, using the specified sample,
volume, pitch and repeats parameters.

6.3 Sound sample file
---------------------
  Until release 2.02 of Star Fighter 3000, all of the instruments and sound
effects required by the game were incorporated directly into the 'SFX_Handler'
module in mono 8 bit Archimedes VIDC format (similar to mu-law).

  In later releases, the sound samples were instead supplied as individual
files containing raw audio data in mono 16 bit little-endian signed linear
format. This is better suited to the sound hardware in modern RISC OS machines
and would make it possible to substitute equivalent sound samples with a
greater dynamic range.

6.4 Sound samples index file
----------------------------
  The set of sound samples to be used when transcoding music to ProTracker
format is defined by an index file in plain ASCII text format. This file
also defines items of metadata related to each sample.

  Each line of text is either a comment (preceded by '#', which MUST be the
first character on that line), a blank line (which consists of nothing but
whitespace), or a sample definition (which may have leading and/or trailing
whitespace). Sample definitions consist of a numeric ID to be assigned to
the sample, the name of a file from which to read raw sample data, an offset
from which to repeat the sample (for sustained notes), the type of sample
(music or effect), and a tuning value to correct the pitch when playing
music.

  Sample numbers must lie within the range 0-255, because of the voice table
format in a SF3000 music track. The range of sample numbers assigned by a
sample index file needn't be contiguous, nor do the samples need to be
defined to be ascending numeric order. Any sample numbers which are still
undefined when transcoding music will merely cause warning messages. However,
the sample index file parser does fault attempts to assign the same sample
number more than once.

  The repeat offset must be less than the length of the sample data, when
both have been rounded down to the nearest even number (the ProTracker
module format isn't capable of representing odd sample lengths). Remember
that the repeat offset is the number of sample values to skip, whereas the
file size is in bytes (each 16 bit sample occupies two bytes).

  The sample type is represented by a single letter which may be either upper
or lower case: "M" for music or "E" for effect. The significance of the
sample type is explained in the section on sound effects.

  The magnitude of the tuning value must be small enough that the calculation
to convert it to ProTracker tuning units does not overflow a 'long' integer.
On RISC OS, the largest value representable by a 'long' integer is one less
than 2 to the power of 31, which means that tuning values must lie within the
range -22369599, 22369599 (1048575 octaves).

  The grammar of a samples index file is as follows (in Backus-Naur Form):
```
  <file>       ::= { <line> }*
  <line>       ::= <comment> | <noncomment>
  <comment>    ::= '#' { <comchar> }* <eol>
  <noncomment> ::= <whitespace>
                   [ <integer> <whitespace> <filename> <whitespace> <integer>
                     <whitespace> <type> <whitespace> <tuning> <whitespace> ]
                   <eol>
  <filename>   ::= <fnchar> { <fnchar> }*
  <type>       ::= "E" | "e" | "M" | "m"
  <tuning>     ::= [ "-" ] <integer>
  <comchar>    ::= any character except <eol>
  <fnchar>     ::= any character except <eol> or <schar>
  <whitespace> ::= <schar> { <schar> }*
  <schar>      ::= " " | <tab> | <vtab> | <formfeed>
  <tab>        ::= character 9
  <eol>        ::= character 13
  <vtab>       ::= character 11
  <formfeed>   ::= character 12
  <integer>    ::= <digit> { <digit> }*
  <digit>      ::= "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
```
  Note that <fnchar> must also be a valid character for use in a file name
on the host platform. On RISC OS, FileCore-based filing systems such as ADFS
reserve the following characters for wildcards and other special uses: '*',
'#', '$', '@', '^', '%' and '\'.

-----------------------------------------------------------------------------
7   Program history
-------------------

0.01 (02 Jun 2009)

- First public release.

0.02 (23 May 2010)

- Removed support for the command line switch "-fastglissando" and increased
  the glissando speed from 1 to 2. The converter now always outputs contiguous
  Tone Portamento commands.

- Specified the appropriate switches to prevent the Norcroft RISC OS ARM C
  compiler from generating unaligned memory accesses incompatible with ARM v6
  and later (for Beagleboard).

- Fixed a couple of warnings previously generated when compiling using GCC
  in Cygwin on Windows.

- Created three wrappers to allow common implementations of most platform-
  specific functions to be shared between Windows, Unix and RISC OS by defining
  appropriate macro values. Hitherto, porting to a new platform required
  editing c.platform (which has been renamed as c.platformc).

- Added hooks for debugging using Simon Bullen's fortified memory allocator.

- Moved the read_int32le function definition from c.platform to c.decomp
  because the implementation is platform-agnostic.

- Substituted anonymous enumerations for macro definitions of numeric
  constant values to avoid changing bypassing the language's syntax-checking.

- Used the standard library function 'strerror' to add detail to file access
  error messages.

- Used the standard library function 'isspace' for better recognition of
  lines consisting only of whitespace in a sound samples index file.

- Removed list of dynamic dependencies from the supplied Makefile (can be
  regenerated by the C compiler).

0.03 (09 Jan 2011)

- Replaced an integral implementation of Gordon Key's decompression algorithm
  with a dependency on release 40 of CBLibrary. There's no sense maintaining
  several versions in parallel.

- Removed file paths from the preamble to many error messages. On RISC OS,
  the actual error message tends to include the file path, so it was needlessly
  repetitious. Path names are still shown when opening files in verbose mode.

- Stopped unnecessary use of type aliases defined by <stdint.h> where 'long
  int' will do just as well.

0.04 (28 Jan 2011)

- Linked against release 42 of CBLibrary, in which the speed of compression
  and error detection during decompression have been significantly improved.

- The -help switch no longer displays the version number and copyright
  message (use -verbose for that).

0.05 (11 Dec 2016)

- Lots of code refactoring to improve maintainability. The compressed SF3000
  music data is now accessed via a buffered stream interface instead of being
  decompressed into memory in its entirety.

- Changed the command-line syntax to allow the program to read from the
  standard input stream if no input file is specified instead of reporting an
  error. Previously, the sound samples directory name was specified after the
  input file name which prevented the latter from being optional.

- The program now writes to the standard output stream if no output file is
  specified, instead of inventing an output file name.

- Added a batch processing mode to allow multiple files to be processed with
  one invocation of the program. This is similar to the original behaviour if
  no output file name was specified.

- Optimised multi-byte reads and writes where possible to use fread and
  fwrite instead of fgetc and fputc.

- The supplied executable file was compiled with GCCSDK GCC 4.7.4 Release 2)
  instead of the Norcroft RISC OS ARM C compiler.

0.06 (07 Nov 2018)

- Added an option to treat the input as uncompressed music data.

- Adapted to use CBUtilLib, StreamLib and GKeyLib instead of the monolithic
  CBLibrary previously required. The is_switch function can be found in
  CBUtilLib therefore a local version of it was deleted.

- The output file is now deleted if its type cannot be set.

- The version string definition was moved to a header file for convenience.

0.07 (02 May 2020)

- Failure to close the output stream is now detected and treated like any
  other error since data may have been lost.

0.08 (21 May 2024)

- Added a new makefile for use on Linux.
- Improved the README.md file for Linux users.
- Elimated a benign switch case fallthrough that caused a compiler warning.
- Some code is now conditionally compiled only if the macro ACORN_C is defined.

-----------------------------------------------------------------------------
8  Compiling the software
-------------------------

  Source code is only supplied for the command-line program. To compile
and link the code you will also require an ISO 9899:1999 standard 'C'
library and four of my own libraries: CBDebugLib, CBUtilLib, StreamLib
and GKeyLib. These are available separately from
https://github.com/chrisbazley/

  The dependency on CBDebugLib isn't very strong: it can be eliminated
by modifying the make file so that the macro USE_CBDEBUG is no longer
predefined.

  Three make files are supplied:

1. 'Makefile' is intended for use with GNU Make and the GNU C Compiler
   on Linux.

2. 'NMakefile' is intended for use with Acorn Make Utility (AMU) and the
   Norcroft C compiler supplied with the Acorn C/C++ Development Suite.

3. 'GMakefile' is intended for use with GNU Make and the GNU C Compiler
   on RISC OS.

  The APCS variant specified for the Norcroft compiler is 32 bit for
compatibility with ARMv5 and fpe2 for compatibility with older versions of
the floating point emulator. Generation of unaligned data loads/stores is
disabled for compatibility with ARMv6. When building the code for release,
it is linked with RISCOS Ltd's generic C library stubs ('StubsG').

  The suffix rules generate output files with different suffixes (or in
different subdirectories, if using the supplied make files on RISC OS),
depending on the compiler options used to compile the source code:

o: Assertions and debugging output are disabled. The code is optimised for
   execution speed.

debug: Assertions and debugging output are enabled. The code includes
       symbolic debugging data (e.g. for use with DDT). The macro FORTIFY
       is pre-defined to enable Simon P. Bullen's fortified shell for memory
       allocations.

d: 'GMakefile' passes '-MMD' when invoking gcc so that dynamic dependencies
   are generated from the #include commands in each source file and output
   to a temporary file in the directory named 'd'. GNU Make cannot
   understand rules that contain RISC OS paths such as /C:Macros.h as
   prerequisites, so 'sed', a stream editor, is used to strip those rules
   when copying the temporary file to the final dependencies file.

  The above suffixes must be specified in various system variables which
control filename suffix translation on RISC OS, including at least
UnixEnv$ar$sfix, UnixEnv$gcc$sfix and UnixEnv$make$sfix.
Unfortunately GNU Make doesn't apply suffix rules to make object files in
subdirectories referenced by path even if the directory name is in
UnixEnv$make$sfix, which is why 'GMakefile' uses the built-in function
addsuffix instead of addprefix to construct lists of the objects to be
built (e.g. foo.o instead of o.foo).

  Before compiling the library for RISC OS, move the C source and header
files with .c and .h suffixes into subdirectories named 'c' and 'h' and
remove those suffixes from their names. You probably also need to create
'o', 'd' and 'debug' subdirectories for compiler output.

The only platform-specific code is the EXT_SEPARATOR and PATH_SEPARATOR
macro definitions in misc.h. These must be defined according to the file
name convention on the target platform (e.g. '.' and '\' for DOS or Windows).

-----------------------------------------------------------------------------
9  Licence and Disclaimer
-------------------------

  This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public Licence as published by the Free
Software Foundation; either version 2 of the Licence, or (at your option)
any later version.

  This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public Licence for
more details.

  You should have received a copy of the GNU General Public Licence along
with this program; if not, write to the Free Software Foundation, Inc.,
675 Mass Ave, Cambridge, MA 02139, USA.

-----------------------------------------------------------------------------
10  Credits
-----------

  SF3KtoProT was designed and programmed by Christopher Bazley with
additional ideas, testing and suggestions by Martin Bazley.

  Credit goes to David O'Shea and Keith McKillop for working out the Fednet
compression algorithm, although this program no longer incorporates code
derived from the DeComp module that David wrote for the Stunt Racer 2000
track designer.

  Most of my information on the Amiga ProTracker module format came
from a document relating to Digital Symphony, a multi-tasking sound tracker
editor for RISC OS which was marketed by Oregan Developments. That
description is (C) 1992/1996 Bernard Jungen and Gil Damoiseaux (members of
BASS). See http://users.skynet.be/Andre.Timmermans/digitalcd/player/docs.zip
(in sub-directory Docs/DSymPlay within the archive).

  I gleaned additional information from Andrew Scott's Noisetracker/
Soundtracker/Protracker module format specification (with credit to Lars
Hamre, Norman Lin, Mark Cox, Peter Hanning, Steinar Midtskogen, Marc Espie
and Thomas Meyer). That document is available here:
http://www.aes.id.au/modformat.html

  The game Star Fighter 3000 is (C) FEDNET Software 1994, 1995. Sound &
music for the game was by A. M. Perrins.

-----------------------------------------------------------------------------
11  Contact details
-------------------

  Feel free to contact me with any bug reports, suggestions or anything else.

  Email: mailto:cs99cjb@gmail.com

  WWW:   http://starfighter.acornarcade.com
