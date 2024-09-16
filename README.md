# WillEngineV2

## Table of Contents
- [Prerequisites](#prerequisites)
- [Getting Started](#getting-started)
- [Build Instructions](#build-instructions)
- [Setting Up Symlinks](#setting-up-symlinks)
- [Troubleshooting](#troubleshooting)
- [Additional Notes](#additional-notes)

## Prerequisites

- Git
- CMake (3.x or later)
- A C++ compiler (e.g., GCC, Clang, MSVC)

## Getting Started

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

## Build Instructions

1. Open CMake GUI
2. Set the source code directory to the root of the WillEngineV2 project
3. Choose a build directory (e.g., `build` or `cmake-build-debug`)
4. Click "Configure" and select your preferred generator
5. Click "Generate"
6. Open the generated project in your IDE or build it from the command line

## Setting Up Symlinks

After building the project, you need to set up symlinks for the shaders and assets directories. 
To avoid creating unnecessary copies. For simplicity, you may simply copy the entire shaders and assets folder instead.

### Windows

Open a Command Prompt as administrator in your build directory and run:

```cmd
mklink /D shaders ..\shaders
mklink /D assets ..\assets
```

> **Note:** Adjust the paths if your folder structure differs.

## Troubleshooting

### SDL2.dll Not Found

If you're prompted for an SDL2.dll, copy the SDL2.dll from this repository into the working directory of your built project.

## Additional Notes

- Make sure you have the necessary permissions to create symlinks on your system.
- For Windows users, you may need to enable Developer Mode or run your IDE as administrator to create symlinks.