# Custom Linux File System in C

## Overview

Custom file system built in C using block-level I/O and a FAT-style volume, with full file and directory operation support via a shell interface.

---

![Demo](demo/file-system-demo.gif)

---

## Features

- Custom volume format and initialization
- FAT-style free space management
- Directory entries with full metadata
- File Control Blocks with buffered read/write
- Persistent storage across runs
- Command-line shell with built-in commands

---

## Shell Commands

Supported commands:
- `ls`, `cd`, `md`, `pwd`,
- `touch`, `cat`, `rm`, `cp`,
- `mv`, `cp2fs`, `cp2l`, `help`

---

## Project Structure

```
- src/*.c     # C source files
- include/*.h # Header files
- obj/*.o     # Compiled object files
- Makefile    # Build script
- Dockerfile  # Docker image configuration
```

---

## Build & Run
```bash
git clone https://github.com/keyprocedure/linux-file-system.git
cd linux-file-system
```

Option 1: Using Docker:  
```bash
# Build the Docker image
docker build -t linux-fs .

# Run the shell in a Docker container 
docker run -it -v $(pwd)/volume:/app/volume linux-fs
```

Option 2: Run Natively on Linux:  
```bash
# Compile the system
make

# Launch the shell
make run    
```

---

## Technical Summary

### Core Components
- **VCB:** Tracks volume info and root dir location
- **FAT:** Manages used/free blocks in a linked list style
- **Directories:** Support nested structures and metadata (`.`, `..`)
- **Buffered I/O:** Efficiently reads/writes to disk in blocks
- **Persistence:** All state is saved to a volume file between runs

---
