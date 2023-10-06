#!/bin/bash
echo "KRB START AUDIO SCRIPT"
sleep 5
IRQ_PID=`ps -eLo pid,cmd | grep irq/43-sdma | awk 'NR==1{print $1}'`
chrt -f -p 95 $IRQ_PID
IRQ_PID=`ps -eLo pid,cmd | grep irq/36-mmc0 | awk 'NR==1{print $1}'`
#chrt -f -p 95 $IRQ_PID

echo -n performance | tee /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo -n performance | tee /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor
echo -n performance | tee /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor
echo -n performance | tee /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor

export JACK_NO_AUDIO_RESERVATION=1
amixer -cwm8904audio set 'Capture Input' ADC
amixer -cwm8904audio set 'Capture' 5
amixer -cwm8904audioa set 'Capture' 5
amixer -cwm8904audioc set 'Capture' 5
amixer -cwm8904audio set 'Headphone' 57
amixer -cwm8904audioa set 'Headphone' 57
amixer -cwm8904audioc set 'Headphone' 57
jackd -R -P90 -dalsa -dmerge -r48000 -p32 -n2 &
sleep 5
#jackd -R -P90 --name codec1 -dalsa -dsysdefault:CARD=wm8904audio_1 -r16000 -p256 -n2 &
#jackd -R -P90 --name codec2 -dalsa -dhw:CARD=wm8904audio_2 -r16000 -p256 -n2 &
#jack_connect system:capture_1 system:playback_1
#jack_connect system:capture_2 system:playback_2
jack_connect system:capture_3 system:playback_3
#jack_connect system:capture_4 system:playback_4
#jack_connect system:capture_5 system:playback_5
#jack_connect system:capture_6 system:playback_6
#jack_connect system:capture_7 system:playback_7
#jack_connect system:capture_8 system:playback_8

