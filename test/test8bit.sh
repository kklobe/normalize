#!/bin/sh

LC_ALL=POSIX
LC_NUMERIC=POSIX
export LC_ALL LC_NUMERIC

# correct answers
MONO_BEFORE=8f187448c9f463ca9ff674da833ed20bddcbdd27
STEREO_BEFORE=3e654c31fc9a0d6bf459ef57acb61c93c2e760f5
LVL_MONO="-6.1497dBFS  -3.0883dBFS  -5.8503dB  mono.wav"
LVL_STEREO="-6.1497dBFS  -3.0883dBFS  -5.8503dB  stereo.wav"
MONO_AFTER=c0435f30b6e9439850e0782d99b0376697833685
STEREO_AFTER=c8a7cfc25d1a380702b1eb2ede84e40e2b1b2f83

exec 3>> test.log
echo "Testing 8-bit wavs..." >&3

../src/mktestwav -a 0.5 -b 1 -c 1 mono.wav
../src/mktestwav -a 0.5 -b 1 -c 2 stereo.wav

# Check that the files written by mktestwav are correct
CHKSUM=`tail -c +44 mono.wav | shasum`
case "$CHKSUM" in
    $MONO_BEFORE*) ;;
    *) echo "FAIL: created mono.wav has bad checksum!" >&3; exit 1 ;;
esac
CHKSUM=`tail -c +44 stereo.wav | shasum`
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
CHKSUM=`tail -c +44 mono.wav | shasum`
case "$CHKSUM" in
    $MONO_AFTER*) ;;
    *) echo "FAIL: adjusted mono.wav has bad checksum!" >&3; exit 1 ;;
esac
CHKSUM=`tail -c +44 stereo.wav | shasum`
case "$CHKSUM" in
    $STEREO_AFTER*) ;;
    *) echo "FAIL: adjusted stereo.wav has bad checksum!" >&3; exit 1 ;;
esac

echo "mono.wav and stereo.wav adjusted successfully..." >&3
echo "PASSED!" >&3

exit 0
