## Forked by Nathan^2
+ dims more smoothly and uses xprintidle for detecting idleness
+ systemd timer used to run at boot
+ Regarding the touch screen, the device disables it after dimming the screen.  Then, when the user taps on the screen, it brightens the screen and enables the touch screen inputs. This way, we avoid propagating the mouse inputs when we can't see the screen. **(which was an issue in the original repo)**

## Running
Simply type in the terminal of the project's directory:
```
make
sudo ./timeout 30
```
where the 30 is 30 seconds of idleness

## Systemd file setup for running script at boot:
Enter into a terminal:
```
sudo systemctl edit --full backlight_dimmer.service
```
Enter the following into the new file:

**Note: Change the ExecStart to your location of the timeout script**
```
[Unit]
Description=Used to dim the raspberry pi's backlight upon idleness

[Service]
ExecStart=/home/pi/Documents/Github/myForks/backlight_dimmer/timeout 30

[Install]
WantedBy=multi-user.target

```
Then reboot!

-----
# Backlight dimmer
Leaving the backlight on the Official Raspberry Pi touchscreen can quickly wear it out.
If you have a use that requires the pi to be on all the time, but does not require the
display on all the time, then turning off the backlight while not in use can dramatically
increase the life of the backlight.

Backlight dimmer will transparently dim the display backlight after there
has been no input for a specifed timeout, independent of anything using the display
at the moment. It will then turn the touchscreen back on when input is received. The
timeout period is set by a command-line argument.

~**Note:** This does not stop the event from getting to whatever is running on the
display. Whatever is running will still receive an event, even if the display
is off.~ Fixed in this Fork

The program will use a linux event device like `/dev/input/event0` to receive events
from the touchscreen, keyboard, mouse, etc., and `/sys/class/backlight/rpi-backlight/brightness`
to dim the backlight. The event device is a command-line parameter without the
/dev/input/ path specification.

# Installation

Clone the repository and change directories:
```
git clone https://github.com/DougieLawson/backlight_dimmer
cd backlight_dimmer
```

Build and run it!
```
make
sudo setuid  ./timeout
./timeout 10 event0
```

Multiple devices may be specified.

**Note:** It must be run as root or with `sudo` to be able to access the backlight. Or you can make the program setuid and owned by root
It can be run at startup, for example by putting a line in
`/etc/rc.local`


### Conflict with console blanker

When running this program without X Windows running, such as when running a Kivy
program in the console at startup, you may run into a conflict with a console
blanker.  In such an instance, the backlight will be turned on, but with the
console blanked, it seems like the backlight has not come on.

In this case, follow one of these methods for disabling the console blanker:
   * Raspbian Jessie :
     Add the following line to /etc/rc.local (on the line before the final exit 0)
     and reboot:
```
  sh -c "TERM=linux setterm -blank 0 >/dev/tty0"
```

   Even though /dev/tty0 is used, this should propagate across all terminals.

   * Raspbian Wheezy :
     Edit /etc/kbd/config and change the values for the variable shown below,
     then reboot:
```
  BLANK_TIME=0
```

&copy; Copyright 2019, Dougie Lawson, all rights reserved.
