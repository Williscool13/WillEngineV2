# WillEngineV2

This is my second attempt at my own 3d game engine.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Getting Started](#getting-started)
- [Build Instructions](#build-instructions)
- [Setting Up Symlinks](#setting-up-symlinks)
- [Troubleshooting](#troubleshooting)
- [Additional Notes](#additional-notes)
- [Contact Me](#contact-me)

## Prerequisites

- Git
- Git LFS (Large File Storage)
- CMake (3.x or later)
- Visual Studio 2022 (or newer) with MSVC Compiler
    - Required because precompiled libraries were built with MSVC
- VulkanSDK (1.3.296)

## Getting Started

This engine only supports Windows at the moment and all instructions and setup will only be outlined for Windows.

1. Clone the repository:
   ```sh
   git clone https://github.com/Williscool13/WillEngineV2.git
   ```

2. Navigate to the project directory:
   ```sh
   cd WillEngineV2
   ```

3. Initialize and update submodules:
   ```sh
   git submodule init
   git submodule update
   ```

4. Ensure Git LFS is set up properly:

   ```sh
   git lfs install
   git lfs pull
   ```

## Build Instructions

1. Open CMake GUI
2. Set the source code directory to the root of the WillEngineV2 project
3. Choose a build directory (e.g., `build` or `cmake-build-debug`)
4. Click "Configure" and select your preferred generator
5. Click "Generate"
6. Open the generated project in your IDE or build it from the command line

## Setting Up Symlinks

After building the project, you need to set up symlinks for the shaders and assets directories to avoid creating unnecessary copies. 
For simplicity, you may simply copy the entire shaders and assets folder instead.

Open a Command Prompt as administrator in your build directory and run:

```cmd
mklink /D shaders ..\shaders
mklink /D assets ..\assets
```

> **Note:** Adjust the paths if your folder structure differs.

## Troubleshooting

### assert(m_init) failed in `VulkanBootstrap.h in Line 132`

This error is often caused by vkb failing to find a suitable GPU to use. It could be that your GPUs do not support Vulkan 1.3 or the use of some vulkan extensions (in this case Descriptor Buffers).
You can verify this by checking what extensions your GPU supports at https://vulkan.gpuinfo.org/.

Unfortunately there isn't much that can be done if you get this error. 

### mklink "You do not have sufficient privilege to perform this operation"

If `mklink` fails, ensure that:

- You are running Command Prompt as an **Administrator**.
- Your user account has the necessary permissions to create symlinks.

### shaderc link errors

- Use `git lfs pull` after cloning to ensure large files are correctly downloaded. Specifically shaderc_combinedd.lib and shaderc_combined.lib

## Contact Me

If you have any troubles with the code or would like to discuss game engine/rendering architecture, please feel free to contact me at twtw40@gmail.com.
