#!/bin/sh

LC_ALL=POSIX
LC_NUMERIC=POSIX
export LC_ALL LC_NUMERIC

# correct answers
MONO_BEFORE=6557f9f2212baac35c31ece87c5b294b61598540
STEREO_BEFORE=da97dd1ed748fb40901b6b9d2b46362a549f97c7
LVL_MONO="-6.0206dBFS  -3.0104dBFS  -5.9794dB  mono.wav"
LVL_STEREO="-6.0206dBFS  -3.0104dBFS  -5.9794dB  stereo.wav"
MONO_AFTER=72be935c3517f85a326751bb3177d694d0853c82
STEREO_AFTER=2755af6e4f9b834afa771a8ad436ef4bc6a26412

exec 3>> test.log
echo "Testing 24-bit wavs..." >&3

../src/mktestwav -a 0.5 -b 3 -c 1 mono.wav
../src/mktestwav -a 0.5 -b 3 -c 2 stereo.wav

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
