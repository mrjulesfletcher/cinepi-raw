# CinePi RAW Deep Dive

This document supplements the top level `README.md` with more in‑depth information about the code structure and a detailed list of every command line parameter.  It is split into two parts:

1. **Low level overview** – how the major classes work and how data flows through the application.
2. **High level tutorial** – step by step usage and the full set of command line options.

## Low Level Overview

The `cinepi-raw` executable is built on top of the standard `libcamera` application framework.  The following classes are key:

- **`CinePIRecorder`** (`cinepi/cinepi_recorder.hpp`) derives from `LibcameraApp` and owns a `DngEncoder`.  It exposes callbacks so that encoded data can be written to disk.  Frames are queued and returned once the encoder signals completion.
- **`CinePIController`** (`cinepi/cinepi_controller.hpp`) runs in a background thread.  It connects to Redis and watches keys like `is_recording`, `iso` and `shutter_s`.  When values change it updates camera controls and publishes statistics.
- **`DngEncoder`** (`cinepi/dng_encoder.*`) receives raw frame buffers and writes CinemaDNG files.  The encoder also publishes metadata when requested.
- **`cinepi_raw.cpp`** contains the main event loop.  Frames are captured, handed to the encoder and optionally displayed via the preview subsystem.  The loop reacts to Redis messages so recording can be started and stopped without restarting the application.

During initialisation the controller synchronises defaults such as width, height and ISO with Redis.  New clip folders are created under the path stored in `mediaDest` using helper functions from `cinepi/utils.hpp`.

### Redis Keys

Constants for the control keys are defined in `cinepi_state.hpp`.  Examples include `CONTROL_KEY_RECORD`, `CONTROL_KEY_ISO` and `CONTROL_KEY_SHUTTER_SPEED`.
When a value is changed the key name is published on the `cp_controls` channel which causes the controller to reconfigure the camera.

## High Level Tutorial

1. **Build dependencies** such as `libcamera`, `redis++` and `hiredis`.  The normal build uses CMake:
   ```bash
   sudo apt install build-essential cmake libtiff-dev libhiredis-dev libredis++-dev
   git clone https://github.com/cinepi/cinepi-raw.git
   cd cinepi-raw && mkdir build && cd build
   cmake ..
   make -j4
   ```
2. **Run Redis** so that `cinepi-raw` can exchange control messages.
3. **Launch the application**.  A basic invocation looks like
   ```bash
   ./build/cinepi-raw --redis redis://127.0.0.1:6379/0 --width 1920 --height 1080
   ```
4. **Toggle recording** using the Redis keys.  For example
   ```bash
   redis-cli set is_recording 1
   redis-cli publish cp_controls is_recording
   ```
   Setting `0` stops recording.
5. **Inspect output** in the folder configured by `--mediaDest` (defaults to `/media/RAW`).

### Complete Argument Reference

The program inherits a large set of options from several classes.  They are grouped below roughly in the order they appear in `core/options.hpp`, `core/video_options.hpp`, `core/still_options.hpp` and `cinepi/raw_options.hpp`.

#### Generic Options
- `--help` (`-h`)
- `--version`
- `--list-cameras`
- `--camera <index>`
- `--verbose` (`-v`)
- `--config` (`-c`) path
- `--info-text <string>`
- `--width <pixels>`
- `--height <pixels>`
- `--timeout` (`-t` ms)
- `--output` (`-o` file)
- `--post-process-file <json>`
- `--rawfull`
- `--nopreview` (`-n`)
- `--preview` (`-p` x,y,w,h)
- `--fullscreen` (`-f`)
- `--qt-preview`
- `--hflip`
- `--vflip`
- `--rotation <0|180>`
- `--roi x,y,w,h`
- `--shutter <us>`
- `--analoggain <gain>`
- `--gain <gain>`
- `--metering <mode>`
- `--exposure <mode>`
- `--ev <comp>`
- `--awb <mode>`
- `--awbgains r,b`
- `--flush`
- `--wrap <count>`
- `--brightness <val>`
- `--contrast <val>`
- `--saturation <val>`
- `--sharpness <val>`
- `--framerate <fps>`
- `--denoise <mode>`
- `--viewfinder-width <px>`
- `--viewfinder-height <px>`
- `--tuning-file <file>`
- `--lores-width <px>`
- `--lores-height <px>`
- `--mode W:H:bit-depth:P|U`
- `--viewfinder-mode W:H:bit-depth:P|U`
- `--buffer-count <n>`
- `--viewfinder-buffer-count <n>`
- `--autofocus-mode <mode>`
- `--autofocus-range <mode>`
- `--autofocus-speed <mode>`
- `--autofocus-window x,y,w,h`
- `--lens-position <val>`
- `--hdr`
- `--metadata <file>`
- `--metadata-format txt|json`

#### Video Options
- `--bitrate` (`-b`)
- `--profile`
- `--level`
- `--intra` (`-g`)
- `--inline`
- `--codec h264|mjpeg|yuv420`
- `--save-pts <file>`
- `--quality` (`-q`, MJPEG)
- `--listen` (`-l`)
- `--keypress` (`-k`)
- `--signal` (`-s`)
- `--initial pause|record`
- `--split`
- `--segment <ms>`
- `--circular <MB>`
- `--frames <count>`
- `--libav-format <fmt>` (if built with libav)
- `--libav-audio` (if built with libav)
- `--audio-codec <codec>`
- `--audio-source <src>`
- `--audio-device <dev>`
- `--audio-bitrate <bps>`
- `--audio-samplerate <Hz>`
- `--av-sync <us>`

#### Still Options
- `--quality` (`-q`, JPEG)
- `--exif` (`-x` key=value)
- `--timelapse <ms>`
- `--framestart <n>`
- `--datetime`
- `--timestamp`
- `--restart <interval>`
- `--keypress` (`-k`)
- `--signal` (`-s`)
- `--thumb width:height:quality|none`
- `--encoding` (`-e` jpg|png|rgb|bmp|yuv420)
- `--raw` (`-r`)
- `--latest <name>`
- `--immediate`
- `--autofocus-on-capture`

#### cinepi-raw Specific Options
- `--redis <url>`
- `--clip_number <n>`
- `--mediaDest <path>`
- `--folder <name>`
- `--wb <multiplier>`
- `--sensor <string>`
- `--model <string>`
- `--make <string>`
- `--serial <string>`
- `--clipping <percent>`

Running `./cinepi-raw --help` will print these options with the same descriptions that appear in the source.

