# Minimal NVIDIA Image Sharpening Compute Headless

NV Image Sharpener is a lightweight, headless Vulkan compute application that implements NVIDIA's Image Scaling algorithm for image sharpening. This tool is designed to process images in bulk, enhancing their sharpness using NVIDIA's scaling tech.


## Example

<table>
  <tr>
    <th>Input Image</th>
    <th>Output Image</th>
  </tr>
  <tr>
    <td><img src="docs/images/orangutan.png" width="400" alt="Input Image"></td>
    <td><img src="docs/images/orangutan_output.png" width="400" alt="Output Image"></td>
  </tr>
</table>

*Note: The output image demonstrates the sharpening effect with a sharpness level of 100%.*

## Features

- Utilizes Vulkan compute shaders for efficient image processing
- Implements NVIDIA's Image Scaling algorithm for high-quality image sharpening
- Supports bulk processing of images in a specified directory
- Allows customizable sharpness levels
- Outputs processed images in PNG format

## Prerequisites

- Vulkan-compatible GPU
- Vulkan SDK installed
- C++17 compatible compiler
- CMake (version 3.12 or higher)

## Building the Project

1. Clone the repository:
   ```bash
   git clone https://github.com/conholo/ComputeHeadless-NV-VK.git
   cd ComputeHeadless-NV-VK
   
2. Create a build directory and navigate to it:
   ```bash 
   mkdir build && cd build

3. Run CMake and build the project:
    ```bash
    cmake ..
    make
    ```
4. Navigate to the program binary: 
   ```bash
   cd ../bin/nv_image_enhancer
   ```

## Usage

After building the project, you can run the NV Image Sharpener using the following command:
   ```bash
       ./nv_image_enhancer <input_directory> [sharpness]
   ```
- `<input_directory>`: Path to the directory containing the images you want to process.
- `[sharpness]`: (Optional) Sharpness level, a value between 0 and 100. Default is 100% if not specified.

The program will process all supported image files (PNG, JPG, JPEG, BMP) in the specified directory and save the sharpened images in an "output" folder within the executable's directory.


### Example

   ```bash
       ./nv_image_enhancer  media/images 75.5
   ```
This command will process all images in the example `media/images` directory with a sharpness level of 75.5.


## Output

Processed images will be saved in the `output` directory created in the same location as the executable. Each output image will be named in the format:
   ```
     original_filename_NVSharpened_XX.XX%.png
   ```
Where `XX.XX` represents the sharpness level used.


## Acknowledgements

This project is based on NVIDIA's Image Scaling SDK and all code specifically related to the Sharpen/Scale was written by NVIDIA. For more information about the original SDK, please visit [NVIDIA's GitHub repository](https://github.com/NVIDIAGameWorks/NVIDIAImageScaling).

## Contributing

Contributions are welcome! If you have any improvements or find any issues, please feel free to open a pull request or an issue. Here's how you can contribute:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/CoolFeatureHere`)
3. Commit your changes (`git commit -m 'Add some Cool Feature'`)
4. Push to the branch (`git push origin feature/CoolFeatureHere`)
5. Open a Pull Request

Please make sure to adhere to the existing coding style.