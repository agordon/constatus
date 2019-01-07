# UVC Streamer

## overview

[UVC_streamer](https://github.com/bsapundzhiev/uvc-streamer) is a program
(similar to [mjpg_streamer](https://github.com/jacksonliam/mjpg-streamer))
that connects to a USB camera device and streams its video using MJPEG format.

Unlike `mjpg_streamer`, `uvc_streamer` supports older cameras that work
with less-common pixel formats (e.g. YUV and GRGR/BGBG).
Example of such cameras are `Logitech, Inc. QuickCam Messenger Plus` and
'Logitech, Inc. QuickCam Express`.


## When to use uvc_streamer

One clear case is when mjpg-streamer fails to load with the following error:

    $ mjpg_streamer -i "input_uvc.so"
    i: Using V4L2 device.: /dev/video0
    i: Desired Resolution: 324 x 240
    i: Frames Per Second.: 5
    i: Format............: JPEG
    i: TV-Norm...........: DEFAULT
    i: The specified resolution is unavailable, using: width 320 height 240 instead
    i: Could not obtain the requested pixelformat: MJPG , driver gave us: RGB3
       ... will try to handle this by checking against supported formats.
   Init v4L2 failed !! exit fatal
    i: init_VideoIn failed

Another indication is `v4l2-ctl` shows `GRBG` or `GRGR/BGBG` or mentions
`Bayer` formats:

    $ v4l2-ctl -d /dev/video0 --list-formats-ext
    ioctl: VIDIOC_ENUM_FMT
	Index       : 0
	Type        : Video Capture
	Pixel Format: 'GRBG'
	Name        : 8-bit Bayer GRGR/BGBG
                      Size: Discrete 352x292

## Installation

    git clone https://github.com/bsapundzhiev/uvc-streamer
    # or: git clone https://github.com/agordon/uvc-streamer.git (see below)
    cd uvc-streamer/
    make
    cp ./uvc_stream /usr/local/bin/

## Usage example

    uvc_stream -d /dev/video0 -g  # for GRBG/GRGR/BGBG cameras
    uvc_stream -d /dev/video0 -y  # for YUYV cameras
    uvc_stream -d /dev/video0     # for moderm MJPG cameras - but then mjpg_streamer is better...

Once the program is running, open a web-browser and visit
<http://[IP]:8080/stream.mjpeg> or <http://[IP]:8080/snapshot.jpeg>.


## Note about black and white video

If the camera reports GRBG format, and the video streams appear in black and
white only, it is possible the original `uvc_streamer` code does not match
your camera. A patched version is available here:
<https://github.com/agordon/uvc-streamer.git>. Use it with `git clone`
instead of the original repository.

## Helper script

A helper script is `uvc-streamer-http.sh`.

At the top of the script are various options that can be changed.

Typical usage would be to copy it to `/usr/local/bin`, adjust as needed,
and run (or use it in init/systemd bootscripts).

The `uvc-streamer-http.sh` script scans the attached USB devices
and connects to a specified devices based on its USB ID (from `lsusb`).
Adjust the script to match your USB devices. This allows connecting to
the correct USB camera regardless of boot order loading of `/dev/videoX`.

    cp uvc-streamer-http.sh /usr/local/bin/

## Constatus settings

The options in `uvc-streamer-http.sh` work with the following constatus configuration:

    source = {
            id = "1-1";
            descr = "mycamera";
            type = "mjpeg";
            url = "http://127.0.0.1:8083/stream.mjpeg";
            prefer-jpeg = true;
            quality = 75;
            width = 640;
            height = 480;
            max-fps = 5.0;
            resize-width = -1;
            resize-height = -1;
    }

The script use a fixed TCP port by default (`8083` in the example above).
If you adjust the resolution or tcp port in the script, be sure to match
the corresponding changes in your constatus configuration file.

If running `uvc-streamer` on a different computer, change the IP address
(`127.0.0.1` in the example above).

## systemd integration

Copy the `.service` file to `/etc/systemd/system`, adjust as needed (e.g.
the path of the executable script with `ExecStart`) and reload the systemd
daemon information:

    cp uvc-streamer-http.service /etc/systemd/system/
    systemctl daemon-reload
    systemctl start uvc-streamer-http

To view service status:

    systemctl status uvc-streamer-http

To enable service at boot:

    systemctl enable uvc-streamer-http

To view log (for troubleshooting):

    journalctl --since today | grep -i uvc

## Note aboue Security

The examples above assume a secure internal (not public) network.
They expose the video stream on unencrypted (HTTP) port without a password.
Anyone with network access would be able to view the video stream.
