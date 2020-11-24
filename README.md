# LiquidWolf

Proof of concept modem using Liquid DSP. This code is pretty atrocious right
now. Don't trust it for anything important.

## Building

liquidwolf has three main dependencies, [libliquid2d](https://github.com/jgaeddert/liquid-dsp),
libsndfile, and CMake for the build system.

On Ubuntu, to install the dependencies:

```bash
sudo apt install cmake libsndfile1-dev libliquid-dev
```

Then, clone the repo and build

```
git clone https://github.com/tkuester/liquidwolf
cd liquidwolf
mkdir build
cd build
cmake ..
make -j8
```

You can also run the unit tests with `make test`.

## Usage

Simply pass it a wavfile to run, or pipe to it from stdin.

```
$ ./liquidwolf ./AFSK_1200_baud.wav
Opened ./AFSK_1200_baud.wav: 44100 Hz, 1 chan
Quality: 1.00
0000 - 82 a0 a4 a6 40 40 e0 a6 6a 6e 98 9c 40 61 03 f0
0010 - 3d 34 36 30 33 2e 36 33 4e 2f 30 31 34 33 31 2e
0020 - 32 36 45 2d 4f 70 2e 20 41 6e 64 72 65 6a 40 65
S57LN-0 (rsp) -> APRS-0 (cmd):
> =4603.63N/01431.26E-Op. Andrej
================================
Done
Processed 38816 samples
11224911 samp / sec
254.5x speed
1 packets
0 one flip packets
```

You can also pipe in a stream of le16 samples, useful for a UDP stream from
GQRX, or `rtl_fm`.

```
$ nc -lu 7355 | ./liquidwolf -r 48000 -

$ rtl_fm -M fm -f 144.39M -s 48000 -E deemp -g 28 | ./liquidwolf -r 48000 -
```

Right now it gets 956 packets on track 1 of the
[TNC Test CD](http://wa8lmf.net/TNCtest/), and 844 on track 2. It doesn't seem
to do very well with `rtl_fm` though, I suspect that's because of an issue
with packets that are too quiet. More research, later!

## TODO

* Native RTL-SDR handling
* Improve handling of signals regardless of preemphasis
* KISS functionality

## Credits

Thanks to W6KWF for his fantastic 2014 thesis on APRS, "Examining Ambiguities
in the Automatic Packet Reporting System".

Special thanks to WB2OSZ for his groundbreaking and incredibly well documented
work on [direwolf](https://github.com/wb2osz/direwolf).

Thanks to [sharebrained](https://twitter.com/sharebrained) for helping me improve
on the signal processing!
