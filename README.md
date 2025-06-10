# CinePi RAW

![cp_raw_banner](https://github.com/cinepi/cinepi-raw/assets/25234407/71591abc-f9b2-467e-806f-30557bcd1491)

*CinePi RAW* is a fork of Raspberry Pi's `rpicam-apps` project that extends the original `libcamera-raw` application with CinemaDNG recording support and a Redis driven control interface. This repository also keeps the original demo applications from `rpicam-apps` and provides a set of utilities and post-processing examples for experimentation.

## Table of Contents

1. [Features](#features)
2. [Requirements](#requirements)
3. [Building-from-Source](#building-from-source)
4. [Directory-Layout](#directory-layout)
5. [Applications-and-Scripts](#applications-and-scripts)
6. [Command-Line-Options](#command-line-options)
7. [Using-the-Redis-Interface](#using-the-redis-interface)
8. [Example-Workflow](#example-workflow)
9. [Tutorials](#tutorials)
10. [License](#license)

## Features

- Capture RAW frames directly to CinemaDNG sequences.
- Redis based API for controlling recording parameters remotely.
- Includes standard `libcamera-hello`, `libcamera-still`, `libcamera-jpeg`, `libcamera-vid`, `libcamera-raw` and `libcamera-detect` applications.
- Optional post-processing stages (object detection, HDR, segmentation, etc.) configured via JSON files in `assets/`.
- Lossless LJ92 compression option to keep file sizes manageable.
- Multithreaded DNG encoder for sustained high frame rates.
- Histogram and other frame statistics are published over Redis in real time.

## Architecture Overview

CinePi RAW keeps the original libcamera application framework and layers a small controller on top. The `cinepi-raw` executable interacts with the camera through `LibcameraApp` classes in `core/`. Frames are passed to `DngEncoder` which writes CinemaDNG files while `CinePIController` publishes and receives settings from Redis. This design allows you to tweak camera parameters in real time without restarting the app.
At runtime the main loop in `cinepi_raw.cpp` performs the following steps:

1. `CinePIController` synchronises options from Redis and spawns a thread that listens for control messages.
2. `CinePIRecorder` configures the camera via libcamera based on these options and starts the preview.
3. For each captured frame the `DngEncoder` converts the Bayer data to CinemaDNG. Compression and metadata insertion happen on worker threads.
4. The encoded frame is pushed to an `Output` implementation that writes to disk.
5. Statistics such as focus distance and histograms are fed back to Redis for external monitoring.

Post-processing stages located under `post_processing_stages/` can hook into this pipeline and manipulate images before they are written. Stages are enabled through JSON configuration files.
## Requirements
Install the following dependencies before building:

- [Redis](https://github.com/redis/redis)
- [Hiredis](https://github.com/redis/hiredis)
- [Redis++](https://github.com/sewenew/redis-plus-plus)
- libtiff development files
- libcamera and its headers (see Raspberry Pi documentation)
- Boost program_options library
- Optional: OpenCV if you plan to use the CV post-processing stages
- Optional: TensorFlow Lite for neural network based stages

## Building from Source


The project uses CMake. The general workflow on a Raspberry Pi is:

```bash
sudo apt install build-essential cmake libtiff-dev libhiredis-dev libredis++-dev
# build libcamera separately if not already installed
# see: https://www.raspberrypi.com/documentation/computers/camera_software.html#building-libcamera-and-libcamera-apps

# fetch the code
git clone https://github.com/cinepi/cinepi-raw.git
cd cinepi-raw
mkdir build && cd build
cmake ..
make -j4
# sudo make install  # optional
```

The resulting binaries appear in `build/`. When compiling on a desktop host you
can cross compile for the Raspberry\u00a0Pi using a toolchain file:

```bash
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/rpi.toolchain.cmake \
    -DCMAKE_INSTALL_PREFIX=/opt/cinepi \
    -DENABLE_OPENCV=1 -DENABLE_TFLITE=1
make
```

The toolchain file sets the cross compiler and Pi sysroot. Installing to a
prefix keeps the output isolated for packaging.

Extra CMake flags control optional components:

```bash
# Enable OpenCV and TFLite based stages
cmake .. -DENABLE_OPENCV=1 -DENABLE_TFLITE=1
```

When cross compiling you may need to provide paths to the Raspberry Pi sysroot and the cross compiler.


## Directory Layout

The repository closely mirrors the upstream `libcamera-apps` layout with additional source files for the CinePi RAW functionality.

- **apps/** – Standard demo applications from `rpicam-apps` (`libcamera-hello`, `libcamera-jpeg`, `libcamera-still`, `libcamera-vid`, `libcamera-raw`, `libcamera-detect`). These serve as examples and test programs.
- **cinepi/** – Implementation of `cinepi-raw` together with the `CinePIController` that talks to Redis. Includes `cinepi_raw.cpp`, the `CinePIRecorder` class and the DNG encoder.
- **core/** – Common `libcamera` helper code (`LibcameraApp`, option parsers and the post-processing pipeline) shared by all apps.
- **encoder/** – H.264/MJPEG encoders plus the custom DNG encoder used by CinePi RAW.
- **image/** – Routines for handling BMP, JPEG, PNG and DNG images.
- **output/** – Base classes that write encoded output to files or pipes. Applications plug their encoder into these objects.
- **preview/** – Abstraction layer for displaying the camera preview with DRM/KMS, EGL, Qt or a null implementation for headless operation.
- **post_processing_stages/** – Example post-processing stages (HDR merge, object detection, segmentation, etc.). They are dynamically loaded based on JSON configuration.
- **assets/** – Sample JSON files demonstrating how to configure post-processing stages. Each file corresponds to a stage name.
- **utils/** – Python helper scripts (`camera-bug-report`, `checkstyle.py`, `timestamp.py`, `test.py`, `version.py`) used for debugging and CI.

### Key Components

- **cinepi/cinepi_raw.cpp** – main entry point running the event loop and coordinating Redis control.
- **cinepi/cinepi_controller.* **– background thread handling Redis messages and publishing camera statistics.
- **cinepi/dng_encoder.* **– writes frames in CinemaDNG format with optional lossless compression.
- **core/libcamera_app.* **– base class wrapping libcamera APIs and providing the preview and capture pipeline.
- **encoder/** – contains concrete encoder implementations, including H.264 and MJPEG, reused by the demos.
- **utils/test.py** – runs basic automated tests to verify the binaries operate correctly.

### Post-processing pipeline

Image manipulations such as HDR merging, annotation or machine learning inference
are implemented as modular *post-processing stages*. Each stage inherits from
`PostProcessingStage` and can be enabled by passing a JSON file with the
`--post-process-file` option. The JSON describes the parameters for the stage and
matches the filenames in `assets/`. Multiple stages can be chained together in
the order specified in the configuration file.

## Applications and Scripts

### cinepi-raw

Main application that records CinemaDNG frames. It uses `CinePIController` to read and publish state via Redis and relies on the custom DNG encoder in the `encoder/` directory. The program exposes all standard libcamera options plus a few extras:

- `--redis` – Redis connection string (default `redis://127.0.0.1:6379/0`).
- `--clip_number` – Starting clip counter.
- `--mediaDest` – Path where clips are created (default `/media/RAW`).
- `--wb` – Fixed white balance multiplier.
- `--clipping` – Highlight clipping threshold.

Run `./cinepi-raw --help` to see the full list of options inherited from `VideoOptions` and `Options`.

### libcamera-* utilities

The `apps/` directory contains the standard utilities from `rpicam-apps`:

- **libcamera-hello** – Simple preview window.
- **libcamera-jpeg** – Capture a JPEG image.
- **libcamera-still** – Flexible still image capture tool.
- **libcamera-vid** – Record video (H.264, MJPEG or raw YUV).
- **libcamera-raw** – Record raw frames without encoding.
- **libcamera-detect** – Example using post-processing with TensorFlow Lite.

All programs share the same extensive command line options (see below).

### Utility scripts

- **camera-bug-report** – Collect system information and logs for bug reports.
- **checkstyle.py** – clang-format based style checker used by CI (`./utils/checkstyle.py --fix` formats the codebase).
- **timestamp.py** – Analyse timestamp files created with the `--save-pts` option.
- **test.py** – Basic smoke tests for the applications (requires the binaries to be built beforehand).
- **version.py** – Helper for generating version strings from git.

## Command Line Options

All applications inherit a large set of command line options defined in `core/options.hpp` and `core/video_options.hpp`. Highlights include:

- General: `--camera`, `--width`, `--height`, `--nopreview`, `--fullscreen`, `--timeout`, `--output`.
- Image controls: `--shutter`, `--gain`, `--awb`, `--awbgains`, `--brightness`, `--contrast`, `--saturation`, `--sharpness`.
- Video settings: `--codec` (`h264`, `mjpeg`, `yuv420`), `--bitrate`, `--profile`, `--level`, `--intra`, `--listen`, `--split`, `--segment`, `--circular`.
- Focus and exposure: `--metering`, `--exposure`, `--ev`, `--autofocus-mode`, `--lens-position`.
- Misc: `--save-pts`, `--viewfinder-width`, `--viewfinder-height`, `--lores-width`, `--lores-height`, `--denoise`, `--hdr`, `--metadata`.
### cinepi-raw specific options

In addition to the generic video options the program defines extra parameters:

- `--redis` – connection string for the Redis server.
- `--clip_number` – initial clip counter value.
- `--mediaDest` – directory where clip folders are created.
- `--folder` – override the folder name for the next recording.
- `--wb` – fixed white balance multiplier (disables auto white balance).
- `--sensor` – override the sensor model string written to DNG metadata.
- `--model` – camera model field for metadata.
- `--make` – camera make field for metadata.
- `--serial` – serial number for metadata.
- `--clipping` – clipping percentage to mark highlight saturation.

`cinepi-raw` adds Redis specific parameters noted above.

## Redis Keys and Controls

The application reads and writes several keys under Redis. The most important ones are listed below:

| Key | Purpose |
|-----|---------|
| `is_recording` | Set to `1` or `0` to start/stop recording. |
| `iso` | Analogue gain value in ISO/100 units. |
| `awb` | Enable (`1`) or disable (`0`) automatic white balance. |
| `cg_rb` | Comma separated red and blue gains when AWB is disabled. |
| `shutter_a` | Shutter angle in degrees. |
| `shutter_s` | Shutter speed in frames per second. |
| `fps` | Desired capture framerate. |
| `width` | Output width. |
| `height` | Output height. |
| `mode` | Camera sensor mode string. |
| `compress` | Enable DNG compression (`1` or `0`). |
| `cam_init` | Flag to reconfigure the camera. |
| `rec` | Publish to trigger start/stop recording. |
| `stll` | Publish to capture a single still frame. |
## Using the Redis Interface

`cinepi-raw` listens to the `cp_controls` channel. To change a parameter:

1. Set the desired value in Redis.
2. Publish the key name on the `cp_controls` channel.

Values are read as strings and interpreted by the application. The recorder publishes statistics on the `cp_stats` channel and histograms on `cp_histogram` so you can build custom monitoring dashboards.

Example: start recording a new clip

```bash
redis-cli set is_recording 1
redis-cli publish cp_controls is_recording
```

To stop recording send `0` instead. Keys for other settings include `iso`, `awb`, `cg_rb`, `shutter_s`, `fps`, `width`, `height` and `compress`. The controller publishes statistics on the `cp_stats` channel and histograms on `cp_histogram`.

A minimal Python snippet to toggle recording might look like:

```python
import redis
r = redis.Redis()
r.set("is_recording", 1)
r.publish("cp_controls", "is_recording")
```

## Example Workflow

```bash
# Start the redis server (if not already running)
redis-server &

# Run the application
./build/cinepi-raw --redis redis://127.0.0.1:6379/0 --width 1920 --height 1080

# Trigger a recording
redis-cli set is_recording 1
redis-cli publish cp_controls is_recording

# Stop
redis-cli set is_recording 0
redis-cli publish cp_controls is_recording
```

### Example: recording with all options

Below shows a longer command that exercises most arguments. Adjust the
values as needed for your camera:

```bash
./build/cinepi-raw \
    --redis redis://localhost:6379/0 \
    --width 4096 --height 2160 \
    --codec yuv420 --fps 24 --timeout 60000 \
    --mediaDest /media/RAW --clip_number 1 --folder test_take \
    --wb 1.2,1.1 --sensor imx477 --model "CinePi" --make "Raspberry Pi" \
    --serial 0001 --clipping 0.98 --denoise off --hdr 0 \
    --post-process-file assets/hdr_merge.json
```

The command above records one minute of 4K uncompressed DNG frames,
writes them under `/media/RAW/test_take`, embeds metadata and disables
on-camera denoise. Post-processing stages can be chained by specifying
additional JSON files.

Captured DNG frames are stored under `/media/RAW/<generated_folder>`.

## Tutorials

### 1. Build and record a clip

```bash
sudo apt install build-essential cmake libtiff-dev libhiredis-dev libredis++-dev
git clone https://github.com/cinepi/cinepi-raw.git
cd cinepi-raw && mkdir build && cd build
cmake .. && make -j4
```

Start the Redis server and capture a short clip:

```bash
redis-server &
./cinepi-raw --timeout 10000 --output first.dng
```

### 2. Remote control via Redis

```bash
./cinepi-raw --redis redis://127.0.0.1:6379/0 &
redis-cli set iso 800
redis-cli publish cp_controls iso
```

Use `redis-cli` to set other keys (`is_recording`, `awb`, `fps`, ... ) while the
program runs. Publish the key name on `cp_controls` to apply changes.

### 3. Using post-processing stages

Select a configuration from `assets/` to enable a processing pipeline:

```bash
./cinepi-raw --post-process-file assets/hdr_merge.json --timeout 10000
```

Multiple JSON files can be specified, separated by commas, to chain stages.

For more information about libcamera options see the official Raspberry Pi
camera documentation.

## Testing

The repository includes a small Python test suite that exercises the command line
applications. After building, run:

```bash
python3 utils/test.py --exe-dir build --output-dir /tmp
```

The script will execute each application and verify that basic functionality
works. It requires numpy and, for some tests, `exiftool` to be installed.

## License

The source code is made available under the simplified [BSD 2‑Clause license](https://spdx.org/licenses/BSD-2-Clause.html).
