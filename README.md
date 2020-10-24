# Backlight dimmer
Leaving the backlight on the Official Raspberry Pi touchscreen can quickly wear it out.  If you have a use that requires the pi to be on all the time, but does not require the display on all the time, then dimming the backlight while not in use can dramatically increase the life of the backlight.

Backlight dimmer will transparently dim the display backlight after there has been no input for a specifed timeout, independent of anything using the display at the moment. It will then turn the touchscreen back on when input is received. The timeout period is set by a command-line argument.

**Note:** This does not stop the event from getting to whatever is running on the display. Whatever is running will still receive an event, even if the display
is off.

The program will use a linux event device like `/dev/input/event0` to receive events from the touchscreen, keyboard, mouse, etc., and `/sys/class/backlight/rpi-backlight/brightness` to dim the backlight. The event device is a command-line parameter without the /dev/input/ path specification.

# Installation

Clone the repository and change directories:
```
git clone https://github.com/DougieLawson/backlight_dimmer
cd backlight_dimmer
```

Build and run it!
```
make
sudo chown root:root  ./timeout; sudo chmod +s ./timeout
./timeout 10 event0
```

Copy to `/usr/local/bin` and make it setuid to allow anyone to run it.
```
sudo cp ./timeout /usr/local/bin
sudo cp ./lsinput /usr/local/bin
sudo cp ./run-timeout.sh /usr/local/bin
sudo chmod +s /usr/local/bin/timeout
sudo chmod +s /usr/local/bin/run-timeout.sh
```

**Note:** It must be run as root (setuid) or with `sudo` to be able to access the backlight. Or you can make the program setuid and owned by root with `sudo cp ...` and `sudo chmod ...`

It can be run at startup, for example by putting a line in 
`/etc/rc.local`. Or by updating `/etc/xdg/lxsession/LXDE-pi/autostart` with 
```
@run-timeout.sh
```

&copy; Copyright 2019-2020, Dougie Lawson, all rights reserved.
