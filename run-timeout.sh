#!/usr/bin/env bash
# Run program to turn off RPi backlight after a period of time,
# turn on when the touchscreen is touched.
# Best to run this script from /etc/rc.local to start at boot.

timeout_period=30 # seconds

# Find the device the touchscreen uses.  This can change depending on
# other input devices (keyboard, mouse) are connected at boot time.
for line in $(lsinput); do
        if [[ $line == *"/dev/input"* ]]; then
                word=$(echo $line | tr "/" "\n")
                for dev in $word; do
                        if [[ $dev == "event"* ]]; then
                                break
                        fi
                done
        fi
        if [[ $line == *"FT5406"* ]] ; then
                break
        fi
done
# Use nice as it sucks up CPU.
# Timeout is in /usr/local/bin so as not to conflict with /bin/timeout
# /usr/local/bin/timeout <timeout> <input_device>
#nice -n 19 /usr/local/bin/timeout $timeout_period $dev &
nice -n 19 ./timeout $timeout_period $dev &
