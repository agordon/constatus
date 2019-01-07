# mjpg-streamer

## Overview

[mjpg-streamer](https://github.com/jacksonliam/mjpg-streamer) is a program
that connects to various cameras (e.g. UVC, RPi) and transmits the video
in various ways (e.g. MJPEG over HTTP, RTSP, etc.).

While constatus can connect to local cameras directly with (`type="video4linux"`),
it is sometimes beneficial to have a separate process do the camera work, and
have constatus connect through it. This is the case with low-end USB cameras
that might send bad information: instead of constatus crashing (or reporting
'camera disconnected'), have a separate process deal with oddeties.

mjpg-streamer is also helpful when streaming information from a remote
camera (e.g. the camera is on one computer and constatus is running on another).


## Installation

    sudo apt-get install cmake libjpeg8-dev
    git clone https://github.com/jacksonliam/mjpg-streamer
    cd mjpg-streamer
    cd mjpg-streamer-experimental
    make
    sudo make install

The resulting binary is `/usr/local/bin/mjpg_streamer` with additional
code in `/usr/local/lib/mjpg_streamer/`.

## Usage examples

A minimal usage example of a standard USB camera (UVC devices):

    mjpg_streamer -i "input_uvc.so" -w "output_http.so"

Then visit <http://[IP]:8080> to view the video stream.

## More information

The various plugins are here: <https://github.com/jacksonliam/mjpg-streamer/tree/master/mjpg-streamer-experimental/plugins>.

Each plugin directory contains a README file with options, e.g.:
<https://github.com/jacksonliam/mjpg-streamer/tree/master/mjpg-streamer-experimental/plugins/input_uvc>

To show the help screen of a plugin use: `mjpg_streamer -i "input_raspicam.so -h"`
or `mjpg_streamer -o "output_http.so -h"`.

## Helper scripts

Two helper scripts (for UVC devices and for RPi camera) are
`mjpg-streamer-raspicam-http.sh`, `mjpg-streamer-uvc-http.sh`.

At the top of each script are various options that can be changed.

Typical usage would be to copy these to `/usr/local/bin`, adjust as needed,
and run (or use them in init/systemd bootscripts).

The `mjpg-streamer-uvc-http.sh` script scans the attached USB devices
and connects to a specified devices based on its USB ID (from `lsusb`).
Adjust the script to match your USB devices. This allows connecting to
the correct USB camera regardless of boot order loading of `/dev/videoX`.

    cp mjpg-streamer-raspicam-http.sh /usr/local/bin/
    cp mjpg-streamer-uvc-http.sh /usr/local/bin/

## Constatus settings

The options in `mjpg-streamer-uvc-http.sh` and `mjpg-streamer-raspicam-http.sh`
work with the following constatus configuration:

    source = {
            id = "1-1";
            descr = "mycamera";
            type = "mjpeg";
            url = "http://127.0.0.1:8081/?action=stream";
            prefer-jpeg = true;
            quality = 75;
            width = 640;
            height = 480;
            max-fps = 5.0;
            resize-width = -1;
            resize-height = -1;
    }

Each script use a different TCP port by default (`8081` in the example above).
If you adjust the resolution or tcp port in the script, be sure to match
the corresponding changes in your constatus configuration file.

If running `mjpg-streamer` on a different computer, change the IP address
(`127.0.0.1` in the example above).

## systemd integration

Copy the `.service` files to `/etc/systemd/system`, adjust as needed (e.g.
the path of the executable script with `ExecStart`) and reload the systemd
daemon information:

    cp mjpg-streamer-raspicam-http.service /etc/systemd/system/
    systemctl daemon-reload
    systemctl start mjpg-streamer-raspicam-http

    cp mjpg-streamer-uvc-http.service      /etc/systemd/system
    systemctl daemon-reload
    systemctl start mjpg-streamer-uvc-http

To view services' status:

    systemctl status mjpg-streamer-raspicam-http
    systemctl status mjpg-streamer-uvc-http

To enable services at boot:

    systemctl enable mjpg-streamer-raspicam-http
    systemctl enable mjpg-streamer-uvc-http

To view log (for troubleshooting):

    journalctl --since today | grep -i mjpg

## Note aboue Security

The examples above assume a secure internal (not public) network.
They expose the video stream on unencrypted (HTTP) port without a password.
Anyone with network access would be able to view the video stream.

One common option is to instruct `mjpg_streamer` to connect only to localhost
(127.0.0.1), and only local processes running on the same computer would
be able to view the stream:

    mjpg_streamer -o "output_http.so -l 127.0.0.1 -p 8080"
