
Normalize

   This is release 0.7.7 of Normalize, an audio file volume normalizer.

   Copyright (c) 1999--2005, Chris Vaill <chrisvaill at gmail>

   Normalize is a tool for adjusting the volume of audio files to a
   standard level. This is useful for things like creating mixed CD's and
   mp3 collections, where different recording levels on different albums
   can cause the volume to vary greatly from song to song.

   Send bug reports, suggestions, comments to chrisvaill at gmail.

   normalize is free software. See the file COPYING for copying
   conditions.
     _________________________________________________________________

Installation synopsis

    1. ./configure options
    2. make
    3. make install

   See the file INSTALL for more extensive directions. See the man page,
   normalize.1, for usage. Run "./configure --help" for configure
   options.
     _________________________________________________________________

Dependencies

   These dependencies are optional. Normalize doesn't require any other
   packages to compile and run.

   MAD library (http://www.underbit.com/products/mad/)

   Normalize will use the MAD MPEG Audio Decoder library if you have it
   (highly recommended). This gives normalize the ability to read mp3
   files. MAD support in normalize was developed using MAD version
   0.14.2b; earlier versions may not work.

   You can run configure with the --without-mad option to turn off mp3
   read support.

   Audiofile library (http://www.68k.org/~michael/audiofile/)

   Normalize can use the audiofile library if version 0.2.2 or later is
   available on your system. This gives normalize the ability to read and
   write AIFF, AIFF-C, WAV, NeXT/Sun .snd/.au, Berkeley/IRCAM/CARL, and
   whatever else the audiofile library people decide to implement in the
   future.

   Audiofile support is not turned on by default, because the built-in
   WAV support is faster (only because it's specifically tailored for PCM
   WAVs), and because I'm guessing most people only ever need to
   normalize standard PCM WAV and mp3 files. If you only want to use
   normalize on standard PCM WAV and mp3 files, you don't need audiofile.
   If, however, you would like to be able to normalize all the different
   audio file formats that audiofile handles, run configure with the
   --with-audiofile option to turn on audiofile support.

   XMMS (http://www.xmms.org/)

   If you have xmms installed, the configure system will build the
   xmms-rva plugin, which honors the relative volume adjustment frames
   that normalize adds to ID3 tags. The option --disable-xmms prevents
   the plugin from being built.
     _________________________________________________________________

Questions and Answers

   1. What platforms does normalize work on?

   I've tested normalize on GNU/Linux and FreeBSD on x86, Solaris on
   Sparc, and Irix on MIPS. I've heard that it works on GNU/Linux on
   Alpha and on BeOS R5. As far as Windows is concerned, you can compile
   it using the Cygwin toolkit. Question 8, below, contains a brief
   overview of this process.

   I've tried to make the code as portable as possible, so I'd appreciate
   hearing whether normalize works on other platforms.

   2. What is normalize useful for?

   Example 1. Let's say you've got a bunch of wav files containing what
   are, in your estimation, Elvis's greatest hits, collected from various
   albums. You want to encode them as mp3's and add them to an
   established collection, but since they're all from different albums,
   they're all recorded at different volumes from each other and from the
   rest of your mp3 collection. If you've been using normalize on all
   your wav files before you encode them, your collection is normalized
   to the default volume level, and you want these new additions to be at
   the same level. Just run normalize with no options on the files, and
   each will be adjusted to the proper volume level:

            normalize "Hound Dog.wav" "Blue Suede Shoes.wav" \
                      "Here Comes Santa Claus.wav" ...

   Example 2. Suppose now you've just extracted all the wav files from
   the Gorilla Biscuits album "Start Today," which, you may know, is
   recorded at a particularly low volume. We want to make the whole album
   louder, but individual tracks should stay at the same volume relative
   to each other. For this we use batch mode. Say the files are named
   01.wav to 14.wav, and are in the current directory. We invoke
   normalize in batch mode to preserve the relative volumes, but
   otherwise, everything's the default:

            normalize -b *.wav

   You can then fire up your mp3 encoder, and the whole album will be
   uniformly louder.

   Example 3. Now suppose we want to encode the Converge album "When
   Forever Comes Crashing." This album has one song, "Ten Cents," that is
   really quiet while the rest of the songs have about the same (loud)
   volume. We'll turn up the verbosity so we can see what's going on:

        > normalize -bv *.wav
        Computing levels...
        Level for track01.cdda.wav: -9.3980dBFS (0.0000dBFS peak)
        Level for track02.cdda.wav: -9.2464dBFS (-0.1538dBFS peak)
        Level for track03.cdda.wav: -8.6308dBFS (-0.2520dBFS peak)
        Level for track04.cdda.wav: -8.7390dBFS (0.0000dBFS peak)
        Level for track05.cdda.wav: -8.1000dBFS (-0.0003dBFS peak)
        Level for track06.cdda.wav: -8.2215dBFS (-0.1754dBFS peak)
        Level for track07.cdda.wav: -8.9346dBFS (-0.1765dBFS peak)
        Level for track08.cdda.wav: -13.6175dBFS (-0.4552dBFS peak)
        Level for track09.cdda.wav: -9.0107dBFS (-0.1778dBFS peak)
        Level for track10.cdda.wav: -8.1824dBFS (-0.4519dBFS peak)
        Level for track11.cdda.wav: -8.5700dBFS (-0.1778dBFS peak)
        Standard deviation is 1.47 dB
        Throwing out level of -13.6175dBFS (different by 4.58dB)
        Average level: -8.6929dBFS
        Applying adjustment of -3.35dB...

   The volume of "Ten Cents," which is track 8, is 4.58 decibels off the
   average, which, given a standard deviation of 1.47 decibels, makes it
   a statistical aberration (which I've defined as anything off by more
   that twice the standard deviation, but you can set a constant decibel
   threshold with the -t option). Therefore, it isn't counted in the
   average, and the adjustment applied to the album isn't thrown off
   because of one song. Although the aberrant song's volume is not
   counted in the average, it is adjusted along with the rest of the
   files.

   Example 4. Finally, say you want to make a mixed CD of 80's songs for
   your mom or something. You won't allow any 80's songs to taint your
   hallowed mp3 collection, so the absolute volumes of these tracks don't
   matter, as long as they're all about the same, so mom doesn't have to
   keep adjusting the volume. For this, use the mix mode option,

        normalize -m *.wav

   and each track will be adjusted to the average level of all the
   tracks.

   3. How does normalize work?

   A little background on how normalize computes the volume of a wav
   file, in case you want to know just how your files are being munged:

   The volumes calculated are RMS amplitudes, which correspond (roughly)
   to perceived volume. Taking the RMS amplitude of an entire file would
   not give us quite the measure we want, though, because a quiet song
   punctuated by short loud parts would average out to a quiet song, and
   the adjustment we would compute would make the loud parts excessively
   loud.

   What we want is to consider the maximum volume of the file, and
   normalize according to that. We break up the signal into 100 chunks
   per second, and get the signal power of each chunk, in order to get an
   estimation of "instantaneous power" over time. This "instantaneous
   power" signal varies too much to get a good measure of the original
   signal's maximum sustained power, so we run a smoothing algorithm over
   the power signal (specifically, a mean filter with a window width of
   100 elements). The maximum point of the smoothed power signal turns
   out to be a good measure of the maximum sustained power of the file.
   We can then take the square root of the power to get maximum sustained
   RMS amplitude.

   As for the default target amplitude of 0.25 (-12dBFS), I've found that
   it's pretty close to the level of most of my albums already, but not
   so high as to cause a lot of limiting on quieter albums. You may want
   to choose a different target amplitude, depending on your music
   collection (just make sure you normalize everything to the same
   amplitude if you want it to all be the same volume!).

   Regarding clipping: since version 0.6, a limiter is employed to
   eliminate clipping. The limiter is on by default; you don't have to do
   anything to use it. The 0.5 series had a -c option to turn on
   limiting, but that limiter caused problems with inexact volume
   adjustment. The new limiter doesn't have this problem, and the -c
   option is considered deprecated (it will be removed in version 1.0).

   Please note that I'm not a recording engineer or an electrical
   engineer, so my signal processing theory may be off. I'd be glad to
   hear from any signal processing wizards if I've made faulty
   assumptions regarding signal power, perceived volume, or any of that
   fun signal theory stuff.

   4. Why don't you normalize using peak levels instead of RMS amplitude?

   Well, in early (unreleased) versions, this is how it worked. I found
   that this just didn't work well. The volume that your ear hears
   corresponds more closely with average RMS amplitude level than with
   peak level. Therefore, making the RMS amplitude of two files equal
   makes their perceived volume equal. (Approximately equal, anyway:
   certain frequencies sound louder at the same amplitude because the ear
   is just more sensitive to those frequencies. I may try to take this
   into account in a future version, but that opens up a whole new can of
   worms.)

   "Normalizing" by peak level generally makes files with small dynamic
   range very loud and does nothing to files with large dynamic ranges.
   There's not really any normalization being done, it's more of a
   histogram expansion. That said, since version 0.5, you can use the
   --peak option to do this in normalize if you're sure it's what you
   really want to do.

   5. Can normalize operate directly on mp3 files?

   Version 0.7 and up can operate directly on MPEG audio files. An mp3
   file is decoded (using Robert Leslie's MAD library) and analyzed on
   the fly, without the need for large temporary WAV files. The mp3 file
   is then "adjusted" by setting its relative volume adjustment
   information (technically, an "RVA2" frame is set in its ID3v2 tag).
   The advantage of this method is that the audio data doesn't need to be
   touched, and you don't incur the cost of re-encoding. The disadvantage
   is that your mp3 player needs to read and use relative volume
   adjustment ID3 frames. The normalize distribution now includes a
   plugin for xmms that honors volume adjustment frames. If you use an
   mp3 player other than xmms, you'll have to bug the author to support
   RVA2 frames in ID3 tags.

   If you'd rather change the volume of the mp3 audio data itself, you
   still have to decode to WAV, normalize the WAV, and re-encode. A
   script, normalize-mp3, is included in the normalize distribution to do
   this for you.

   6. Can normalize operate on ogg vorbis files?

   Version 0.8 will at least be able to read vorbis audio files.
   Adjusting is harder, though: the problem is that, unlike with ID3, as
   far as I know there's no standardized volume adjustment tag for ogg. I
   could just use, say, "VOLUME_ADJUST=X.XXdB" as an ogg comment, but
   there would be no reason for players to support it.

   It may be possible to twiddle the vorbis data itself to alter the
   volume in a lossless way. I'm looking into this, but it would be a big
   undertaking, not something that would be finished anytime soon.

   The current situation is that you have to decode to WAV, normalize the
   WAV, and re-encode. The normalize-ogg script is included in the
   normalize distribution to do this for you.

   7. How do I normalize a whole tree of files recursively?

   The "unix way" to do this is to use find:

        find . -type d -exec sh -c "normalize -b \"{}\"/*.mp3" \;

   will go directory by directory, running normalize -b on all mp3 files
   in each. If you don't want batch mode, just:

        find . -name \*.mp3 -exec normalize {} \;

   will run normalize on each mp3 file separately. If you want to run
   normalize in batch or mix mode on all files in the directory tree,
   use:

        find . -name \*.mp3 -print0 | xargs -0 normalize -b

   A built-in recurse option has been a very popular request, so I'm
   adding support for it in version 0.8.

   8. How do I use normalize in Windows?

   "I click on INSTALL but nothing happens. What's wrong?" Okay, here's
   the deal: normalize is free software, written for free operating
   systems such as Linux and FreeBSD. These happen to be unix-style
   operating systems, so normalize generally works on other non-free
   flavors of unix as well. Unlike Windows software, unix software such
   as normalize is meant to run on many different operating systems on
   many different architectures, so usually it comes in source code form
   and you have to compile it for your particular setup. If you are
   running some form of unix, normalize should compile right out of the
   box (let me know if it doesn't!). For other operating systems, such as
   Amiga, BeOS, OS/2, or Windows, you may have to jump through some hoops
   to get it to compile.

   A discussion of compiling unix software for Windows is way beyond the
   scope of this FAQ, but here's a quick rundown:

    1. You first need the Cygwin toolkit. After installing, start up a
       cygwin bash shell.
    2. Go to the directory where you unzipped the normalize archive -- it
       would be named something like normalize-x.y.z.
    3. Type "./configure", then "make", then "make install"
    4. If there were no errors, you can run normalize by typing
       "normalize" at the prompt. Normalize is a command-line utility, so
       you have to pass it command line options. Run "normalize --help"
       for a synopsis.
     _________________________________________________________________

   Copyright (c) 1999--2005, Chris Vaill <chrisvaill at gmail>

   Permission is granted to copy, distribute, and/or modify this document
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2 of the License, or (at your
   option) any later version.
