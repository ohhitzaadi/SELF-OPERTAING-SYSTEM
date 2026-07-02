
# 🖥️ NexusOS

### A fully custom operating system built from scratch in x86 Assembly and C


**No libraries. No frameworks. Every byte from bootloader to window manager — written from the ground up.**

## 📖 About

NexusOS is a bare-metal x86 operating system written entirely from scratch. It boots from a custom bootloader written in NASM assembly, transitions into 32-bit protected mode, and loads a modular C kernel that provides memory management, multitasking, a graphical window manager, a FAT file system, and multiple user-space applications — all running in Ring 3 with proper privilege separation.

This is **not** a Linux distribution or a fork of an existing OS. Every component — from the MBR bootloader to the Tetris game — is hand-written.

---

## ✨ Features

### 🔧 Bootloader
- Written in x86 real-mode assembly (NASM)
- Loads kernel from disk via BIOS `int 0x13` with CHS addressing
- Loads FAT12 floppy image into memory
- Sets up GDT (Global Descriptor Table) with code/data segments
- Enables A20 gate via Fast A20 (Port 0x92)
- Transitions CPU into 32-bit protected mode
- Sets VGA Mode 13h (320×200, 256-color) before entering protected mode

### 🧠 Kernel
- **Memory Management**
  - Physical Memory Manager (PMM) — bitmap-based page frame allocator for 32 MB RAM (8192 frames)
  - Virtual Memory Manager (VMM) — page directory/table setup with identity mapping
  - Kernel Heap — `malloc()` / `free()` implementation with a 2 MB heap
- **CPU Configuration**
  - GDT with kernel & user-mode segments (Ring 0 + Ring 3)
  - TSS (Task State Segment) for privilege level switching
  - IDT with 256 interrupt gates
  - PIC remapping (IRQ0–IRQ15 → INT 0x20–0x2F)
- **Interrupts & Drivers**
  - PIT Timer — configured at 100 Hz for preemptive scheduling
  - PS/2 Keyboard — scancode-to-ASCII translation with key event handling
  - PS/2 Mouse — full 3-byte packet parsing with cursor tracking
  - ATA Hard Drive — PIO-mode sector read/write for FAT16 disk access
- **Multitasking**
  - Preemptive round-robin scheduler
  - Process Control Blocks (PCBs) with state tracking
  - Context switching via timer interrupt (IRQ0)
  - `sys_yield()` system call for cooperative scheduling
- **System Calls**
  - `int 0x80` software interrupt interface (DPL=3 for user-space access)
  - Syscalls for: process spawn, yield, exit, keyboard input, mouse state, file I/O, graphics rendering

### 🖥️ Window Manager
- VGA Mode 13h (320×200, 256 colors)
- Double-buffered rendering (backbuffer at `0x300000`)
- Drawing primitives: pixel, line, rectangle, filled rectangle, circle, character, string
- Mouse cursor rendering
- Per-process graphical windows

### 📂 File System
- **FAT12** — floppy disk image (1.44 MB, 2880 sectors) for kernel and shell
- **FAT16** — hard disk image (256 MB) for user applications
- Full directory traversal and file read/write support
- ELF32 binary loader — parses ELF headers and loads PT_LOAD segments

### 🎮 User-Space Applications
All applications run in **Ring 3** (user mode) with system call access via `int 0x80`:

| Application | Description |
|---|---|
| **Shell** | Command-line interface with 150-line scrollback history, command parsing, and app launching |
| **Text Editor** | File editor with keyboard input and save-to-disk functionality |
| **Paint** | Pixel drawing application with mouse-driven canvas |
| **Tetris** | Fully playable Tetris game with piece rotation and scoring |
| **Doom Raycaster** | 3D raycasting engine inspired by Wolfenstein/Doom |
| **Raytrace** | Real-time ray tracing renderer |
| **Mouse Demo** | Interactive PS/2 mouse driver demonstration |

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────┐
│                  User Space (Ring 3)                 │
│  ┌───────┐ ┌──────┐ ┌─────┐ ┌──────┐ ┌──────────┐  │
│  │ Shell │ │ Edit │ │Paint│ │Tetris│ │ Raycaster│  │
│  └───┬───┘ └──┬───┘ └──┬──┘ └──┬───┘ └────┬─────┘  │
│      └────────┴────────┴───────┴───────────┘        │
│                    int 0x80 (Syscalls)               │
├─────────────────────────────────────────────────────┤
│                 Kernel Space (Ring 0)                │
│  ┌──────────┐ ┌──────────┐ ┌──────────────────────┐ │
│  │ Scheduler│ │  Memory  │ │    System Calls      │ │
│  │  (sched) │ │  (PMM/   │ │  (spawn, yield,      │ │
│  │          │ │   VMM/   │ │   read, write, gfx)  │ │
│  │          │ │   Heap)  │ │                      │ │
│  └──────────┘ └──────────┘ └──────────────────────┘ │
│  ┌──────────────────────────────────────────────────┐│
│  │               Services Layer                     ││
│  │  Window Manager │ File Service │ Device Service  ││
│  └──────────────────────────────────────────────────┘│
│  ┌──────────────────────────────────────────────────┐│
│  │               Hardware Drivers                   ││
│  │  Keyboard │ Mouse │ Timer │ ATA Disk │ VGA/IO   ││
│  └──────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────┤
│  Bootloader (Real Mode → Protected Mode)            │
│  BIOS Disk Load │ GDT Setup │ A20 Gate │ Mode 13h  │
└─────────────────────────────────────────────────────┘
```

---

## 📁 Project Structure

```
.
├── src/
│   ├── boot.asm              # MBR bootloader (x86 real mode → protected mode)
│   ├── kernel_entry.asm      # Kernel entry point & ISR stubs (assembly)
│   ├── kernel.c              # Main kernel: GDT, IDT, PIC, kmain()
│   ├── linker.ld             # Kernel linker script
│   ├── libuser.c             # User-space system call library
│   ├── libuser.h             # User-space API header
│   ├── test_user.asm         # Assembly test program for Ring 3
│   ├── kernel/
│   │   ├── mem.c / mem.h     # PMM, VMM, heap, string utilities
│   │   ├── sched.c / sched.h # Process scheduler & context switching
│   │   ├── syscall.c         # System call handler & ELF loader
│   │   └── drivers/
│   │       ├── io.c / io.h       # Port I/O (inb/outb)
│   │       ├── keyboard.c / .h   # PS/2 keyboard driver
│   │       ├── mouse.c / .h      # PS/2 mouse driver
│   │       ├── timer.c / .h      # PIT timer driver (100 Hz)
│   │       └── ata.c / ata.h     # ATA PIO disk driver
│   ├── services/
│   │   ├── window_manager.c / .h  # VGA Mode 13h graphics & compositing
│   │   ├── file_service.c / .h    # FAT12/FAT16 filesystem
│   │   └── device_service.c / .h  # Device abstraction layer
│   └── apps/
│       ├── shell.c           # Command-line shell
│       ├── editor.c          # Text editor
│       ├── paint.c           # Paint application
│       ├── tetris.c          # Tetris game
│       ├── doom.c            # Doom-style raycaster
│       ├── raytrace.c        # Ray tracing demo
│       └── mousedemo.c       # Mouse driver demo
├── build/                    # Compiled object files and ELF binaries
├── build.py                  # Build system (Python)
├── compile_flags.txt         # Clang compile flags for IDE support
├── os.img                    # Bootable floppy image (1.44 MB FAT12)
└── disk.img                  # Hard drive image (256 MB FAT16)
```

---

## 🚀 Getting Started

### Prerequisites

Install the following tools on **Windows**:

| Tool | Purpose | Install |
|---|---|---|
| **NASM** | Assembler for bootloader & kernel entry | [nasm.us](https://www.nasm.us/) |
| **LLVM/Clang** | C compiler (cross-compiles to i386) | [llvm.org](https://llvm.org/) |
| **LLD** | LLVM linker (included with LLVM) | Included with Clang |
| **llvm-objcopy** | Binary extraction from ELF | Included with LLVM |
| **QEMU** | x86 system emulator | [qemu.org](https://www.qemu.org/) |
| **Python 3** | Build script runner | [python.org](https://www.python.org/) |

### Build & Run

```bash
# Clone the repository
git clone https://github.com/ohhitzaadi/SELF-OPERTAING-SYSTEM.git
cd SELF-OPERTAING-SYSTEM

# Build and launch in QEMU
python build.py
```

The build system will:
1. Assemble the bootloader (`boot.asm`)
2. Compile the kernel and all services/drivers
3. Link everything into a flat binary
4. Build all user-space applications as ELF binaries
5. Format a 1.44 MB FAT12 floppy image (`os.img`)
6. Format a 256 MB FAT16 hard disk image (`disk.img`)
7. Launch QEMU with the OS

### Build Output

```
[SUCCESS] Built os.img successfully!
  - Bootloader:    512 bytes (1 sector)
  - Kernel:        32768 bytes (64 sectors)
  - Floppy Image:  1474560 bytes (1.44 MB FAT12)
  - Hard Disk:     268435456 bytes (256 MB FAT16)
```

---

## 🎮 Usage

Once NexusOS boots, you'll be dropped into the **Shell**. Available commands:

| Command | Description |
|---|---|
| `help` | List available commands |
| `clear` | Clear the screen |
| `tetris` | Launch Tetris |
| `doom` | Launch the Doom raycaster |
| `paint` | Launch Paint |
| `editor` | Launch the text editor |
| `raytrace` | Launch the ray tracer |
| `mousedemo` | Launch the mouse demo |

---

## 🛠️ Technical Details

| Component | Specification |
|---|---|
| **Architecture** | x86 (i386), 32-bit protected mode |
| **Boot** | Custom MBR bootloader, BIOS boot |
| **Graphics** | VGA Mode 13h — 320×200, 256 colors |
| **RAM** | 32 MB (8192 page frames, 4 KB each) |
| **Heap** | 2 MB kernel heap |
| **Storage** | FAT12 (floppy) + FAT16 (hard disk) |
| **Scheduler** | Preemptive round-robin at 100 Hz |
| **User mode** | Ring 3 with `int 0x80` syscall interface |
| **Compiler** | Clang (cross-compile to i386-elf) |
| **Assembler** | NASM (x86 assembly) |
| **Linker** | LLD (LLVM linker) |
| **Emulator** | QEMU (`qemu-system-i386`) |

---

## 📜 License

This project is open source. Feel free to explore, learn from, and build upon it.

---

<div align="center">

**Built with ❤️ and raw machine code**

*If you found this project interesting, consider giving it a ⭐ on GitHub!*

</div>
]]>
