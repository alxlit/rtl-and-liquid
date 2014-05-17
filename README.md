# rtl-and-liquid

![](https://raw.githubusercontent.com/alxlit/rtl-and-liquid/master/interface.png)

It's an AM/FM software-defined radio application that uses [RTL-SDR](http://sdr.osmocom.org/trac/wiki/rtl-sdr) and [liquid-dsp](http://liquidsdr.org/). It is written purely in C and has quite good performance, capable of ~70 ksamples/s throughput with FM on a [BeagleBone Black](http://beagleboard.org/Products/BeagleBone%20Black). It also features a novel web-based interface that uses WebSockets (via [libwebsockets](http://libwebsockets.org)) to stream data and WebAudio to play it.
The interface is a **prototype** and is rather fragile.
It works with Chromium (run with `--disable-web-security`) or Firefox 29+.

## Usage

Install dependencies as in `provision.sh`.
If you want to use the AM receiver, you'll need an upconverter such as the [Ham-It-Up](http://www.hamradioscience.com/ham-it-up-hf-converter/).
You may need to change `ARCH_OPTION` flag `-mfloat-abi=softfp` to `=hard` in liquid-dsp's configure.ac.

### Deploying to BeagleBone

I used Arch Linux ARM.
You should be able to use Angstrom or OpenEmbedded as you like.
Procure uSD card, then follow the directions [here](http://archlinuxarm.org/platforms/armv7/ti/beaglebone-black). Hold the button by the uSD card slot to boot from it (or install to the eMMC if you like). Run an upgrade, `pacman -Syu`, and install the following packages (`pacman -S <package>`):

```
base-devel // pick and choose if you like to save space
cmake
fftw
git
unzip
wget
```

Then copy over the provision script, `scp provision.sh root@192.168.x.x:/root` and run it (usually building as root isn't a good idea, so consider creating another user).
Additionally, do the following:

```
$ echo "/usr/local/lib" >> /etc/ld.so.conf.d/libc.conf
$ ldconfig
$ echo "install dvb_usb_rtl28xxu /bin/false" >> /etc/modprobe.d/blacklist.conf
$ cp build/librtlsdr-master/rtl-sdr.rules /etc/udev/rules.d/
```

## Authors

Alex Little, Ryan McCall, and Patrick Evensen worked on this project for Virginia Tech's *ECE 4984: Software-Defined and Cognitive Radio Design* course, offered in Spring 2014.
Thanks to assitance from Dr. Dietrich (the course instructor) and Dr. Gaeddert (creator of liquid-dsp).

## License

[MIT](https://en.wikipedia.org/wiki/MIT_License)
