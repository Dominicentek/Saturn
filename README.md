# Saturn Studio

An experimental fork of [Saturn](https://github.com/Llennpie/Saturn) that has features originally planned for the original version,
but would make too much of a difference to implement, so it made more sense to make it a separate version.

**I still encourage using the original version.** This version is intended for users that are more experienced with the program.

Donate to sm64rise, the creator of Saturn, [here](https://ko-fi.com/J3J05B5WR).

### Features of the Studio version

- [x] A different UI, closer to Moon edition
- [x] Multiple Mario support
  - [x] Models
  - [x] Colorcodes
  - [x] Animations
  - [x] Keyframing
  - [x] Misc settings
  - [x] In-game bone editor
- [X] Input recording/playback
- [x] Recording transparent .webm videos (or .png sequences) directly in the editor
- [x] Capturing transparent screenshots
- [x] Mouse-based camera controls

### Controls

* Left click to spawn a new Mario
* Right click him to modify his properties (or use the Marios menu incase it doesn't work)
* Mouse based camera controls
  * Hold left click to pan the camera
  * Hold right click to rotate it
  * Use the scrollwheel to zoom in and out
  * Hold left shift to zoom slower
  * Hold left ctrl to zoom faster
* Keyboard based camera controls
  * WSAD to move, W and S is forward and backward respectively
  * Holding P makes the W and S keys up and down respectively
  * Holding O makes the camera rotate instead
  * Left shift to move slower
  * Left ctrl to move faster

## Building

### Windows

1. Install [MSYS2](https://msys2.org)
2. Once it is installed, run the **MINGW64** shell
3. Install dependencies using this command:
   ```
   pacman -S git make gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-glew python3 mingw-w64-x86_64-dlfcn
   ```
5. Continue the steps for Linux starting from step 2
6. After you're finished compiling, run `explorer .` to open a file explorer window in the current directory

### Linux

1. Install dependencies
   * Debian
     ```
     sudo apt install git make gcc python3 libsdl2-dev libglew-dev
     ```
   * Arch
     ```
     sudo pacman -S git make gcc python3 sdl2 glew
     ```
   * Fedora
     ```
     sudo dnf install git make gcc python3 SDL2 glew
     ```
   * SUSE
     ```
     sudo zypper install git make gcc SDL2 glew
     ```
2. Clone the repository
   ```
   git clone https://github.com/Dominicentek/Saturn saturn-studio
   cd saturn-studio
   ```
3. Compile the project
   ```
   make -j$(nproc)
   ```
4. Build files will be located in `build/us_pc`

## Installing FFmpeg

### Windows

1. You can get FFmpeg from [here](https://www.gyan.dev/ffmpeg/builds/)
2. Once you download it, extract the archive to C:/ffmpeg
3. Make sure that there's a directory C:/ffmpeg/bin and that it contains "ffmpeg.exe"
4. In the start menu, search for "environment variables"
5. On the dialog, click "environment variables"
6. In the "system" section, find the "Path" entry
7. Select it and click edit
8. Add a new entry and type in "C:\ffmpeg\bin"
9. Apply everything
10. Restart Saturn Studio if it's running

Alternatively, you can put the FFmpeg executables in the same directory the Saturn Studio executable is located in.

### Linux

You can use your distribution's package manager

* Debian: `sudo apt install ffmpeg`
* Arch: `sudo pacman -S ffmpeg`
* Fedora: `sudo dnf install ffmpeg`
* SUSE: `sudo zypper install ffmpeg`

After it's installed, restart Saturn Studio if it's running
