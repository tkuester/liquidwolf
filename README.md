# LiquidWolf

Proof of concept modem using Liquid DSP. This code is pretty atrocious right
now. Don't trust it for anything.

## Building

You need [liquiddsp](https://github.com/jgaeddert/liquid-dsp), as well as
libsndfile. You can build the software by just typing "make".

## Usage

Simply pass it a wavfile to run.

```
$ ./liquidwolf <some_file.wav>
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

Right now it gets about ~900 packets on track 1 of the
[TNC Test CD](http://wa8lmf.net/TNCtest/), and fails miserably on track 2.

Depending on the state of my debug code, there may be a file created called
out.f32, which you can load up in audacity to view internal signals. I really
should fix that up.

## TODO

Everything actually

## Credits

Special thanks to WB2OSZ for his groundbreaking and incredibly well documented
work on [direwolf](https://github.com/wb2osz/direwolf).
