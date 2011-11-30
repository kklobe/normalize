#!/bin/sh

LC_ALL=POSIX
LC_NUMERIC=POSIX
export LC_ALL LC_NUMERIC

# correct answers
MONO_BEFORE=0e37a15946316f3db6fc9a23abf3f946
STEREO_BEFORE=449a7f8dc0f38912606e1ebcfc5bac85
LVL_MONO="-6.1497dBFS  -3.0883dBFS  -5.8503dB  mono.wav"
LVL_STEREO="-6.1497dBFS  -3.0883dBFS  -5.8503dB  stereo.wav"
MONO_AFTER=b684ba44e69d7171c7938acc3ae4d1c6
STEREO_AFTER=7a0d972b53f9c63bf714da73eac9f86a

exec 3>> test.log
echo "Testing 8-bit wavs..." >&3

../src/mktestwav -a 0.5 -b 1 -c 1 mono.wav
../src/mktestwav -a 0.5 -b 1 -c 2 stereo.wav

# Check that the files written by mktestwav are correct
CHKSUM=`tail -c +44 mono.wav | md5sum`
case "$CHKSUM" in
    $MONO_BEFORE*) ;;
    *) echo "FAIL: created mono.wav has bad checksum!" >&3; exit 1 ;;
esac
CHKSUM=`tail -c +44 stereo.wav | md5sum`
case "$CHKSUM" in
    $STEREO_BEFORE*) ;;
    *) echo "FAIL: created stereo.wav has bad checksum!" >&3; exit 1 ;;
esac

echo "mono.wav and stereo.wav created..." >&3

# Check that normalize correctly measures the volume of the files
NORM=`../src/normalize -qn mono.wav`
if test x"$NORM" != x"$LVL_MONO"; then
    echo "FAIL: measured volume of mono.wav is incorrect:" >&3
    echo "    should be: $LVL_MONO" >&3
    echo "    got:       $NORM" >&3
    exit 1
fi
NORM=`../src/normalize -qn stereo.wav`
if test x"$NORM" != x"$LVL_STEREO"; then
    echo "FAIL: measured volume of stereo.wav is incorrect:" >&3
    echo "    should be: $LVL_STEREO" >&3
    echo "    got:       $NORM" >&3
    exit 1
fi

echo "mono.wav and stereo.wav measured successfully..." >&3

# Check that normalize correctly normalizes the volume of the files
../src/normalize -q mono.wav
../src/normalize -q stereo.wav
CHKSUM=`tail -c +44 mono.wav | md5sum`
case "$CHKSUM" in
    $MONO_AFTER*) ;;
    *) echo "FAIL: adjusted mono.wav has bad checksum!" >&3; exit 1 ;;
esac
CHKSUM=`tail -c +44 stereo.wav | md5sum`
case "$CHKSUM" in
    $STEREO_AFTER*) ;;
    *) echo "FAIL: adjusted stereo.wav has bad checksum!" >&3; exit 1 ;;
esac

echo "mono.wav and stereo.wav adjusted successfully..." >&3
echo "PASSED!" >&3

exit 0
