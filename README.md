# Mini GUI Framework Demo

A deliberately small C GUI framework prototype with the same application API for:

- Windows / Win32
- macOS / Cocoa
- Linux / GTK4

The application code is in `src/main.c`. It contains nested containers:

```c
GuiBeginGrid(2);

    GuiBeginGroup("Controls");
        ...
    GuiEndGroup();

    GuiBeginGroup("Actions");
        ...
    GuiEndGroup();

GuiEndGrid();
```

The intended framework direction is that the application should not know about native GUI APIs.

## Windows with VS Code

Requirements:

- Visual Studio Build Tools or Visual Studio with the C/C++ workload
- CMake
- VS Code
- CMake Tools extension
- C/C++ extension

Open this directory in VS Code.

The default Windows build selects the Windows backend automatically:

```text
CMake: Configure
CMake: Build
```

Or from a Developer Command Prompt:

```bat
cmake -S . -B build -DGUI_PLATFORM=WINDOWS
cmake --build build --config Debug
```

Run:

```bat
build\Debug\mgui.exe
```

## Explicit platform selection

Windows:

```bat
cmake -S . -B build -DGUI_PLATFORM=WINDOWS
```

macOS:

```bash
cmake -S . -B build -DGUI_PLATFORM=MACOS
cmake --build build
```

Linux:

```bash
cmake -S . -B build -DGUI_PLATFORM=LINUX
cmake --build build
```

Linux requires GTK4 development files, for example on Debian/Ubuntu:

```bash
sudo apt install libgtk-4-dev
```

## About the prototype

This is intentionally a first architectural demo, not yet the finished layout engine.

The important part is the separation:

```text
Application
    |
    v
gui.h / gui.c       common API
    |
    +---- Win32 backend
    +---- Cocoa backend
    +---- GTK backend
```

The next major step should be replacing the temporary native-control positioning with a real platform-independent layout tree:

```text
Window
  |
  +-- Vertical
       |
       +-- Grid(2)
            |
            +-- Group
            |    +-- Button
            |    +-- Button
            |
            +-- Button
```

That will make nesting, automatic sizing, resizing, margins, spacing, and alignment work consistently on all three platforms.
