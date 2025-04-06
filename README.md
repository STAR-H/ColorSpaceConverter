# Color Space Converter

A command-line tool for converting raw image data (NV12 or Bayer format) to JPEG format. It supports multi-threaded processing for improved performance.

## Features
- Convert **NV12** (YUV) format to JPEG.
- Convert **Bayer** (raw sensor data) format to JPEG with support for multiple patterns (`rggb`, `grbg`, `gbrg`, `bggr`).
- Multi-threaded processing for batch conversion.
- Command-line interface for easy integration into workflows.

## Dependencies
- **OpenCV** (for image processing and JPEG encoding).
- **C++17** (for threading and filesystem operations).

## Installation
1. Ensure OpenCV is installed on your system. For example, on Ubuntu:
   ```bash
   sudo apt-get install libopencv-dev
   ```
2. Clone this repository and build the project:
   ```bash
   git clone https://github.com/your-repo/ColorSpaceConverter.git
   cd ColorSpaceConverter
   mkdir build && cd build
   cmake .. && make
   ```

## Usage
### Basic Command
```bash
./ColorSpaceConverter -i <input> --format <format> --width <width> --height <height> [--pattern <pattern>]
```

### Parameters
| Parameter       | Description                                                                 |
|----------------|-----------------------------------------------------------------------------|
| `-i`           | Input file or directory path.                                               |
| `--format`     | Input format (`nv12` or `bayer`).                                           |
| `--width`      | Width of the input image (in pixels).                                       |
| `--height`     | Height of the input image (in pixels).                                      |
| `--pattern`    | Required for Bayer format (`rggb`, `grbg`, `gbrg`, `bggr`).                |

### Examples
1. **Convert a single NV12 file**:
   ```bash
   ./ColorSpaceConverter -i input.nv12 --format nv12 --width 1920 --height 1080
   ```
   Output: `input.jpg`

2. **Convert a directory of Bayer files**:
   ```bash
   ./ColorSpaceConverter -i ./raw_images --format bayer --width 1280 --height 720 --pattern rggb
   ```
   Output: All `.raw` files in `./raw_images` are converted to `.jpg`.

3. **Multi-threaded processing**:
   The tool automatically uses all available CPU cores for batch processing.

## Notes
1. **Input Data**:
   - For `NV12`, the input file must be raw YUV data.
   - For `Bayer`, the input file must be raw sensor data (10-bit or 16-bit).

2. **Output**:
   - The output JPEG files are saved in the same directory as the input files, with the `.jpg` extension.

3. **Performance**:
   - For large datasets, multi-threading significantly improves processing speed.

4. **Error Handling**:
   - Invalid input files or missing parameters will result in an error message.

## License
MIT
