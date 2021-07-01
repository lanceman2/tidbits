# decode_rtlsdr
Trying to understand RTLSDR dongle programming with iotcl() calls and
URBs via a gdb debugging session.

Remove the libusb development package:
```shell
apt remove libusb-1.0-0-dev
```

First read and run:
```shell
./install_libusb
```

Then read and run:
```shell
./install_librtlsdr
```

Then get a gdb session by running;
```shell
./gdb.bash
```

You'll need to learn/know gdb commands at that point.
It also helps to run *strace* programs like for example:
```shell
strace -f rtl_sdr -f 89100000 -s 2048000 -S -n 1025999 xxx1 2> yyy
```

