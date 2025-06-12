![cp_raw_banner](https://github.com/cinepi/cinepi-raw/assets/25234407/71591abc-f9b2-467e-806f-30557bcd1491)

*fork of rpicam-apps that builds upon the rpicam-raw app, offering cinema dng recording capabillities and integration with REDIS offering an abstract "API" like layer for custom integrations / controls.*

Requirements
-----
Please install the below requirements before continuing with the rest of the build process. On a Raspberry Pi the packages can be installed with:

```bash
sudo apt update
sudo apt install -y git build-essential cmake libboost-program-options-dev \
    libtiff-dev libhiredis-dev redis-server
```

The [Redis++](https://github.com/sewenew/redis-plus-plus) library is required and can be built from source if not available as a package.

Build
-----
To build the application create a build directory and run CMake followed by `make`:

```bash
mkdir build
cd build
cmake ..
make -j4
# optional
sudo make install
```

Command Line Options
--------------------
The application inherits most options from the upstream libcamera-apps tools. Below is a list of the available arguments extracted from the source:

- `--help`, `-h` – Print the help message
- `--version` – Display build version
- `--list-cameras` – List the connected cameras
- `--camera` – Select camera index
- `--verbose`, `-v` – Set verbosity level
- `--config`, `-c` – Read options from a configuration file
- `--info-text` – Text shown on the preview window
- `--width` – Output image width
- `--height` – Output image height
- `--timeout`, `-t` – Run time in milliseconds
- `--output`, `-o` – Output filename pattern
- `--post-process-file` – Post processing configuration file
- `--rawfull` – Use full resolution raw frames
- `--nopreview`, `-n` – Disable preview window
- `--preview`, `-p` – Set preview window geometry
- `--fullscreen`, `-f` – Fullscreen preview
- `--qt-preview` – Use the Qt based preview window
- `--hflip` – Apply horizontal flip
- `--vflip` – Apply vertical flip
- `--rotation` – Image rotation (0 or 180)
- `--roi` – Digital zoom region
- `--shutter` – Fixed shutter speed (µs)
- `--analoggain` – Fixed analogue gain
- `--gain` – Fixed gain value
- `--metering` – Metering mode
- `--exposure` – Exposure mode
- `--ev` – Exposure compensation
- `--awb` – Automatic white balance mode
- `--awbgains` – Manual AWB gains
- `--flush` – Flush output data immediately
- `--wrap` – Reset output counter after N files
- `--brightness` – Adjust image brightness
- `--contrast` – Adjust image contrast
- `--saturation` – Adjust image saturation
- `--sharpness` – Adjust image sharpness
- `--framerate` – Fixed frame rate
- `--denoise` – Denoise mode
- `--viewfinder-width` – Viewfinder frame width
- `--viewfinder-height` – Viewfinder frame height
- `--tuning-file` – Camera tuning file
- `--lores-width` – Width of low resolution stream
- `--lores-height` – Height of low resolution stream
- `--mode` – Camera mode specification
- `--viewfinder-mode` – Preview mode specification
- `--buffer-count` – Number of in flight buffers
- `--viewfinder-buffer-count` – Buffers for preview
- `--autofocus-mode` – Autofocus algorithm mode
- `--autofocus-range` – Autofocus search range
- `--autofocus-speed` – Autofocus lens speed
- `--autofocus-window` – Autofocus region of interest
- `--lens-position` – Manual lens position
- `--hdr` – Enable High Dynamic Range mode
- `--metadata` – Save frame metadata
- `--metadata-format` – Metadata format (`json` or `txt`)
- `--bitrate`, `-b` – Video bitrate (H264)
- `--profile` – H264 profile
- `--level` – H264 level
- `--intra`, `-g` – Intra frame period
- `--inline` – Insert PPS/SPS headers in every I‑frame
- `--codec` – Recording codec
- `--save-pts` – Save PTS timestamps
- `--quality`, `-q` – MJPEG quality
- `--listen`, `-l` – Wait for a network client
- `--keypress`, `-k` – Pause/resume on ENTER key
- `--signal`, `-s` – Pause/resume on signal
- `--initial`, `-i` – Initial recording state
- `--split` – Split recording on pause/resume
- `--segment` – Segment recording into fixed lengths
- `--circular` – Circular buffer size in MB
- `--frames` – Number of frames to record
- `--libav-format` – libav format (when enabled)
- `--libav-audio` – Record audio (when enabled)
- `--audio-codec` – Audio codec name
- `--audio-source` – Audio input source
- `--audio-device` – Audio device name
- `--audio-bitrate` – Audio bitrate
- `--audio-samplerate` – Audio sampling rate
- `--av-sync` – Audio/video sync offset

### CinePI‑raw specific options

The following arguments are defined in `raw_options.hpp` and are unique to the
CinePI variant:

- `--redis` – Redis connection URI (default `redis://127.0.0.1:6379/0`)
- `--clip-number` – Starting clip counter used in folder names
- `--media-dest` – Destination path for DNG clips and stills
- `--folder` – Name of the recording folder
- `--wb` – Override white balance multiplier
- `--sensor` – Camera sensor name written to DNG metadata
- `--model` – Camera model identifier
- `--make` – Camera manufacturer string
- `--serial` – Unique camera serial number
- `--clipping` – Black level clipping threshold

Recording Workflow
------------------
1. **Start Redis**
   ```bash
   redis-server --daemonize yes
   ```
2. **Launch cinepi-raw**
   ```bash
   ./build/cinepi-raw -t 0 --mode 1920:1080:12:P --framerate 24
   ```
3. **Control recording through redis-cli**
   ```bash
   redis-cli set rec 1     # start recording
   redis-cli set rec 0     # stop recording
   redis-cli set stll 1    # capture a still frame
   redis-cli set iso 800   # change ISO
   redis-cli set shutter_s 48  # shutter speed (1/x seconds)
   ```
   All available keys are defined in `cinepi/cinepi_state.hpp` and include `iso`, `awb`, `cg_rb`, `shutter_a`, `shutter_s`, `fps`, `width`, `height`, `mode`, and `compress`.
4. **Files**
   DNG sequences are written under `/media/RAW/<clip_folder>` while stills are saved in `/media/RAW/stills`. Copy them off the Pi using your preferred method (`scp`, removing the media, etc.) and inspect them with any DNG‑compatible viewer.
5. **Post Processing**
   Optional stages under `post_processing_stages/` can be enabled to perform additional image manipulation or analysis once the frames are saved.

Developer Notes
---------------
`cinepi_raw.cpp` drives the application and owns the main event loop. `cinepi_controller.cpp` handles the Redis interface and updates capture parameters on the fly. Raw frames are converted to CinemaDNG by `dng_encoder.cpp`, which spawns worker threads to process frames and writes them using the classes in `output/`. Exploring these files alongside `cinepi_state.hpp` provides a good overview of the capture pipeline.

License
-------

The source code is made available under the simplified [BSD 2-Clause license](https://spdx.org/licenses/BSD-2-Clause.html).
