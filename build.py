import os
import sys
import subprocess
import shutil

# Directories
WORKSPACE = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(WORKSPACE, "src")
BUILD_DIR = os.path.join(WORKSPACE, "build")

# Target images
OS_IMG = os.path.join(WORKSPACE, "os.img")
DISK_IMG = os.path.join(WORKSPACE, "disk.img")

# Helper to find Windows username for local appdata search
USER_PROFILE = os.environ.get("USERPROFILE", "")

# Standard paths to search for compilers in case they aren't in system PATH yet
TOOL_SEARCH_PATHS = {
    "nasm": [
        "nasm",  # Check system path first
        os.path.join(USER_PROFILE, "AppData", "Local", "Programs", "NASM", "nasm.exe"),
        "C:\\Program Files\\NASM\\nasm.exe",
    ],
    "clang": [
        "clang",
        "C:\\Program Files\\LLVM\\bin\\clang.exe",
    ],
    "ld.lld": [
        "ld.lld",
        "C:\\Program Files\\LLVM\\bin\\ld.lld.exe",
    ],
    "llvm-objcopy": [
        "llvm-objcopy",
        "C:\\Program Files\\LLVM\\bin\\llvm-objcopy.exe",
    ],
    "qemu": [
        "qemu-system-i386",
        "C:\\Program Files\\qemu\\qemu-system-i386.exe",
        "C:\\Program Files\\qemu\\qemu-system-x86_64.exe",
    ]
}

def find_tool(tool_name):
    paths = TOOL_SEARCH_PATHS.get(tool_name, [tool_name])
    for path in paths:
        # Check if absolute path exists
        if os.path.isabs(path):
            if os.path.exists(path):
                return path
        else:
            # Check if it exists in system PATH
            resolved = shutil.which(path)
            if resolved:
                return resolved
    return None

def verify_tools():
    print("[BUILD] Locating required build tools...")
    tools = {}
    missing = []
    
    for tool_name in TOOL_SEARCH_PATHS.keys():
        resolved_path = find_tool(tool_name)
        if resolved_path:
            tools[tool_name] = resolved_path
            print(f"  - Found {tool_name:12}: {resolved_path}")
        else:
            missing.append(tool_name)
            
    if missing:
        print("\n[ERROR] The following tools were not found:")
        for t in missing:
            print(f"  - {t}")
        print("\nIf you just installed them via winget, please try running the build again.")
        print("Alternatively, ensure their installation paths are correct in build.py.")
        sys.exit(1)
        
    return tools

def clean():
    print("[CLEAN] Cleaning up build artifacts...")
    if os.path.exists(BUILD_DIR):
        shutil.rmtree(BUILD_DIR)
    if os.path.exists(OS_IMG):
        os.remove(OS_IMG)
    if os.path.exists(DISK_IMG):
        os.remove(DISK_IMG)
    print("  Done.")

def make_fat12_disk(boot_bin, kernel_bin, files_dict):
    # Create empty disk of 2880 sectors (1474560 bytes)
    disk = bytearray(1474560)
    
    # 1. Write boot sector
    disk[0:len(boot_bin)] = boot_bin
    
    # 2. Write kernel
    disk[512:512+len(kernel_bin)] = kernel_bin
    
    # Set up BPB fields inside boot sector (offset 11 in boot sector)
    disk[11] = 0x00; disk[12] = 0x02  # bytes_per_sector = 512
    disk[13] = 1                      # sectors_per_cluster = 1
    disk[14] = 65; disk[15] = 0        # reserved_sectors = 65
    disk[16] = 2                      # fat_count = 2
    disk[17] = 224; disk[18] = 0      # root_entry_count = 224
    disk[19] = 0x40; disk[20] = 0x0B  # total_sectors = 2880
    disk[21] = 0xF0                   # media_descriptor = 0xF0
    disk[22] = 9; disk[23] = 0        # sectors_per_fat = 9
    disk[24] = 18; disk[25] = 0      # sectors_per_track = 18
    disk[26] = 2; disk[27] = 0        # head_count = 2
    
    # Write FAT tables (Sector 65 to 73 and 74 to 82)
    fat1_offset = 65 * 512
    fat2_offset = 74 * 512
    
    disk[fat1_offset] = 0xF0
    disk[fat1_offset+1] = 0xFF
    disk[fat1_offset+2] = 0xFF
    
    disk[fat2_offset] = 0xF0
    disk[fat2_offset+1] = 0xFF
    disk[fat2_offset+2] = 0xFF
    
    # Write Root Directory entries (Sector 83 to 96)
    root_offset = 83 * 512
    
    current_cluster = 2
    entry_idx = 0
    
    for filename, content in files_dict.items():
        parts = filename.split('.')
        name = parts[0].upper()[:8]
        ext = parts[1].upper()[:3] if len(parts) > 1 else ""
        
        name = name.ljust(8)
        ext = ext.ljust(3)
        
        ent_off = root_offset + entry_idx * 32
        
        disk[ent_off:ent_off+8] = name.encode('ascii')
        disk[ent_off+8:ent_off+11] = ext.encode('ascii')
        disk[ent_off+11] = 0x00  # Normal file attribute
        disk[ent_off+26] = current_cluster & 0xFF
        disk[ent_off+27] = (current_cluster >> 8) & 0xFF
        
        file_size = len(content)
        disk[ent_off+28] = file_size & 0xFF
        disk[ent_off+29] = (file_size >> 8) & 0xFF
        disk[ent_off+30] = (file_size >> 16) & 0xFF
        disk[ent_off+31] = (file_size >> 24) & 0xFF
        
        # Write file contents
        content_offset = 0
        while content_offset < file_size:
            chunk = content[content_offset:content_offset+512]
            cluster_sec = 97 + (current_cluster - 2)
            sec_off = cluster_sec * 512
            disk[sec_off:sec_off+len(chunk)] = chunk
            content_offset += 512
            
            is_last = (content_offset >= file_size)
            next_val = 0xFFF if is_last else current_cluster + 1
            
            for fat_base in [fat1_offset, fat2_offset]:
                entry_byte_offset = (current_cluster * 3) // 2
                val16 = disk[fat_base + entry_byte_offset] | (disk[fat_base + entry_byte_offset + 1] << 8)
                if current_cluster & 1:
                    val16 = (val16 & 0x000F) | (next_val << 4)
                else:
                    val16 = (val16 & 0xF000) | (next_val & 0x0FFF)
                disk[fat_base + entry_byte_offset] = val16 & 0xFF
                disk[fat_base + entry_byte_offset + 1] = (val16 >> 8) & 0xFF
                
            if not is_last:
                current_cluster += 1
                
        current_cluster += 1
        entry_idx += 1
        
    return disk

def make_fat16_disk(size_mb, files_dict):
    # Total size in bytes (256 MB)
    total_bytes = size_mb * 1024 * 1024
    disk = bytearray(total_bytes)
    
    # 1. Write Boot Sector (LBA 0)
    # BPB fields
    disk[11] = 0x00; disk[12] = 0x02  # bytes_per_sector = 512
    disk[13] = 8                      # sectors_per_cluster = 8 (4KB per cluster)
    disk[14] = 4; disk[15] = 0        # reserved_sectors = 4
    disk[16] = 2                      # fat_count = 2
    disk[17] = 0x00; disk[18] = 0x02  # root_entry_count = 512
    disk[19] = 0; disk[20] = 0        # total_sectors_short = 0
    disk[21] = 0xF8                   # media_descriptor = 0xF8
    disk[22] = 0x01; disk[23] = 0x01  # sectors_per_fat = 257
    disk[24] = 63; disk[25] = 0       # sectors_per_track = 63
    disk[26] = 16; disk[27] = 0       # head_count = 16
    
    # Large sectors count at offset 32: total_sectors = total_bytes // 512
    total_sectors = total_bytes // 512
    disk[32] = total_sectors & 0xFF
    disk[33] = (total_sectors >> 8) & 0xFF
    disk[34] = (total_sectors >> 16) & 0xFF
    disk[35] = (total_sectors >> 24) & 0xFF
    
    disk[36] = 0x80                   # drive_number = 0x80
    disk[38] = 0x29                   # signature = 0x29
    disk[39] = 0x78; disk[40] = 0x56; disk[41] = 0x34; disk[42] = 0x12 # Volume ID
    disk[43:54] = b"SECONDARY  "      # Volume Label (11 bytes)
    disk[54:62] = b"FAT16   "         # Filesystem Type (8 bytes)
    
    # Boot signature at offset 510
    disk[510] = 0x55
    disk[511] = 0xAA
    
    # 2. Write FAT tables (FAT1 starts at sector 4, FAT2 at sector 261)
    fat1_offset = 4 * 512
    fat2_offset = 261 * 512
    
    # Media descriptor entries
    for base in [fat1_offset, fat2_offset]:
        disk[base] = 0xF8
        disk[base+1] = 0xFF
        disk[base+2] = 0xFF
        disk[base+3] = 0xFF
        
    # 3. Write Root Directory entries (Root starts at sector 518, size 32 sectors)
    root_offset = 518 * 512
    
    current_cluster = 2
    entry_idx = 0
    
    for filename, content in files_dict.items():
        parts = filename.split('.')
        name = parts[0].upper()[:8]
        ext = parts[1].upper()[:3] if len(parts) > 1 else ""
        
        name = name.ljust(8)
        ext = ext.ljust(3)
        
        ent_off = root_offset + entry_idx * 32
        
        disk[ent_off:ent_off+8] = name.encode('ascii')
        disk[ent_off+8:ent_off+11] = ext.encode('ascii')
        disk[ent_off+11] = 0x00        # attribute
        disk[ent_off+26] = current_cluster & 0xFF
        disk[ent_off+27] = (current_cluster >> 8) & 0xFF
        
        file_size = len(content)
        disk[ent_off+28] = file_size & 0xFF
        disk[ent_off+29] = (file_size >> 8) & 0xFF
        disk[ent_off+30] = (file_size >> 16) & 0xFF
        disk[ent_off+31] = (file_size >> 24) & 0xFF
        
        # Write file contents to clusters starting at sector 550
        # Each cluster is 8 sectors (4096 bytes)
        content_offset = 0
        cluster_chain = []
        
        while content_offset < file_size:
            chunk = content[content_offset:content_offset + 4096]
            cluster_sec = 550 + (current_cluster - 2) * 8
            sec_off = cluster_sec * 512
            disk[sec_off:sec_off+len(chunk)] = chunk
            content_offset += 4096
            
            cluster_chain.append(current_cluster)
            current_cluster += 1
            
        # Write FAT chain for this file
        for idx_chain, cl in enumerate(cluster_chain):
            is_last = (idx_chain == len(cluster_chain) - 1)
            next_val = 0xFFFF if is_last else cluster_chain[idx_chain + 1]
            
            for fat_base in [fat1_offset, fat2_offset]:
                entry_byte_offset = cl * 2
                disk[fat_base + entry_byte_offset] = next_val & 0xFF
                disk[fat_base + entry_byte_offset + 1] = (next_val >> 8) & 0xFF
                
        entry_idx += 1
        
    return disk

def build(tools):
    print("[BUILD] Starting compilation...")
    os.makedirs(BUILD_DIR, exist_ok=True)
    
    boot_asm = os.path.join(SRC_DIR, "boot.asm")
    boot_bin = os.path.join(BUILD_DIR, "boot.bin")
    
    entry_asm = os.path.join(SRC_DIR, "kernel_entry.asm")
    entry_o = os.path.join(BUILD_DIR, "kernel_entry.o")
    
    linker_ld = os.path.join(SRC_DIR, "linker.ld")
    kernel_elf = os.path.join(BUILD_DIR, "kernel.elf")
    kernel_bin = os.path.join(BUILD_DIR, "kernel.bin")

    # 1. Assemble Bootloader (Flat 16-bit binary)
    print("  1. Assembling Bootloader...")
    cmd_boot = [tools["nasm"], "-f", "bin", boot_asm, "-o", boot_bin]
    subprocess.run(cmd_boot, check=True)
    
    # Verify bootloader is exactly 512 bytes
    boot_size = os.path.getsize(boot_bin)
    if boot_size != 512:
        print(f"[ERROR] Bootloader size is {boot_size} bytes (must be exactly 512 bytes).")
        sys.exit(1)
        
    # 2. Assemble Kernel Entry (32-bit ELF object)
    print("  2. Assembling Kernel Entry...")
    cmd_entry = [tools["nasm"], "-f", "elf32", entry_asm, "-o", entry_o]
    subprocess.run(cmd_entry, check=True)
    
    # 3. Compile Modular Kernel & Services C code
    print("  3. Compiling Modular Kernel & Services C code...")
    kernel_sources = [
        "kernel.c",
        "kernel/mem.c",
        "kernel/sched.c",
        "kernel/syscall.c",
        "kernel/drivers/io.c",
        "kernel/drivers/ata.c",
        "kernel/drivers/keyboard.c",
        "kernel/drivers/mouse.c",
        "kernel/drivers/timer.c",
        "services/window_manager.c",
        "services/file_service.c",
        "services/device_service.c"
    ]
    kernel_objs = []
    for src in kernel_sources:
        src_path = os.path.join(SRC_DIR, src)
        obj_name = src.replace("/", "_").replace(".c", ".o")
        obj_path = os.path.join(BUILD_DIR, obj_name)
        
        cmd_clang = [
            tools["clang"],
            "-target", "i386-pc-none-elf",
            "-ffreestanding",
            "-m32",
            "-mno-sse",
            "-mno-mmx",
            "-fno-stack-protector",
            "-fno-pic",
            "-fno-pie",
            "-Oz",
            "-I", SRC_DIR,
            "-c", src_path,
            "-o", obj_path
        ]
        subprocess.run(cmd_clang, check=True)
        kernel_objs.append(obj_path)
    
    # 4. Link Kernel Object Files (using LLD)
    print("  4. Linking Kernel Objects...")
    cmd_link = [
        tools["ld.lld"],
        "-T", linker_ld,
        entry_o
    ] + kernel_objs + [
        "-o", kernel_elf
    ]
    subprocess.run(cmd_link, check=True)
    
    # 5. Extract flat binary kernel using objcopy
    print("  5. Generating Flat Kernel Binary...")
    cmd_objcopy = [
        tools["llvm-objcopy"],
        "-O", "binary",
        kernel_elf, kernel_bin
    ]
    subprocess.run(cmd_objcopy, check=True)
    
    # 6. Read kernel binary and pad it to sector boundary (512 bytes)
    print("  6. Padding Kernel and building OS image...")
    with open(kernel_bin, "rb") as f:
        kernel_data = f.read()
        
    kernel_len = len(kernel_data)
    fixed_kernel_size = 64 * 512 # 32 KB
    if kernel_len > fixed_kernel_size:
        print(f"[WARNING] Kernel size ({kernel_len} bytes) exceeds bootloader reading buffer ({fixed_kernel_size} bytes)!")
        fixed_kernel_size = ((kernel_len + 511) // 512) * 512
        print(f"[WARNING] Adjusting bootloader load size is recommended if you edit boot.asm.")
        
    padded_kernel_data = kernel_data + b"\x00" * (fixed_kernel_size - kernel_len)
    
    with open(boot_bin, "rb") as f:
        boot_data = f.read()
        
    # 7. Compile and Link User Space Library & C Apps
    print("  7. Compiling User Space Library & Applications...")
    libuser_c = os.path.join(SRC_DIR, "libuser.c")
    libuser_o = os.path.join(BUILD_DIR, "libuser.o")
    
    cmd_lib = [
        tools["clang"],
        "-target", "i386-pc-none-elf",
        "-ffreestanding",
        "-m32",
        "-mno-sse",
        "-mno-mmx",
        "-fno-stack-protector",
        "-fno-pic",
        "-fno-pie",
        "-Oz",
        "-I", SRC_DIR,
        "-c", libuser_c,
        "-o", libuser_o
    ]
    subprocess.run(cmd_lib, check=True)
    
    apps = {
        "SHELL.ELF": ("apps/shell.c", "0x400000"),
        "TETRIS.ELF": ("apps/tetris.c", "0x500000"),
        "DOOM.ELF": ("apps/doom.c", "0x600000"),
        "MOUSEDEMO.ELF": ("apps/mousedemo.c", "0x700000"),
        "RAYTRACE.ELF": ("apps/raytrace.c", "0x800000"),
        "PAINT.ELF": ("apps/paint.c", "0x900000"),
        "EDITOR.ELF": ("apps/editor.c", "0xA00000")
    }
    
    app_binaries = {}
    
    for app_name, (app_src, base_addr) in apps.items():
        print(f"  - Building C Application: {app_name} (linked at {base_addr})...")
        app_src_path = os.path.join(SRC_DIR, app_src)
        app_obj_path = os.path.join(BUILD_DIR, app_name.replace(".ELF", ".o"))
        app_elf_path = os.path.join(BUILD_DIR, app_name.lower())
        
        cmd_app_c = [
            tools["clang"],
            "-target", "i386-pc-none-elf",
            "-ffreestanding",
            "-m32",
            "-mno-sse",
            "-mno-mmx",
            "-fno-stack-protector",
            "-fno-pic",
            "-fno-pie",
            "-Oz",
            "-I", SRC_DIR,
            "-c", app_src_path,
            "-o", app_obj_path
        ]
        subprocess.run(cmd_app_c, check=True)
        
        cmd_app_link = [
            tools["ld.lld"],
            "-Ttext", base_addr,
            app_obj_path, libuser_o,
            "-o", app_elf_path
        ]
        subprocess.run(cmd_app_link, check=True)
        
        with open(app_elf_path, "rb") as f:
            app_binaries[app_name] = f.read()

    # Build legacy ASM application
    print("  - Building ASM Application: TEST.ELF...")
    test_user_asm = os.path.join(SRC_DIR, "test_user.asm")
    test_user_o = os.path.join(BUILD_DIR, "test_user.o")
    test_user_elf = os.path.join(BUILD_DIR, "test_user.elf")
    
    cmd_user_asm = [tools["nasm"], "-f", "elf32", test_user_asm, "-o", test_user_o]
    subprocess.run(cmd_user_asm, check=True)
    
    cmd_user_link = [
        tools["ld.lld"],
        "-Ttext", "0x400000",
        test_user_o,
        "-o", test_user_elf
    ]
    subprocess.run(cmd_user_link, check=True)
    
    with open(test_user_elf, "rb") as f:
        app_binaries["TEST.ELF"] = f.read()
        
    # 8. Formatting OS image as FAT12 with KERNEL & SHELL
    print("  8. Formatting OS image as FAT12 with KERNEL & SHELL...")
    # Floppy only contains SHELL.ELF to boot cleanly
    floppy_apps = {
        "SHELL.ELF": app_binaries["SHELL.ELF"]
    }
    fat12_disk_data = make_fat12_disk(boot_data, padded_kernel_data, floppy_apps)
    
    with open(OS_IMG, "wb") as f:
        f.write(fat12_disk_data)
        
    # 9. Formatting 256MB Hard Drive as FAT16 with applications
    print("  9. Formatting 256MB Hard Drive as FAT16 with all apps...")
    hdd_apps = {
        "TETRIS.ELF": app_binaries["TETRIS.ELF"],
        "DOOM.ELF": app_binaries["DOOM.ELF"],
        "MOUSEDEMO.ELF": app_binaries["MOUSEDEMO.ELF"],
        "RAYTRACE.ELF": app_binaries["RAYTRACE.ELF"],
        "PAINT.ELF": app_binaries["PAINT.ELF"],
        "EDITOR.ELF": app_binaries["EDITOR.ELF"],
        "TEST.ELF": app_binaries["TEST.ELF"],
        "PAINT.RAW": b"\x00" * 12800,
        "NOTE.TXT": b" " * 1024
    }
    fat16_disk_data = make_fat16_disk(256, hdd_apps)
    
    with open(DISK_IMG, "wb") as f:
        f.write(fat16_disk_data)
        
    img_size = os.path.getsize(OS_IMG)
    hdd_size = os.path.getsize(DISK_IMG)
    print(f"\n[SUCCESS] Built {OS_IMG} successfully!")
    print(f"  - Bootloader: {len(boot_data)} bytes (1 sector)")
    print(f"  - Kernel:     {len(padded_kernel_data)} bytes ({len(padded_kernel_data)//512} sectors)")
    print(f"  - Floppy Image: {img_size} bytes ({img_size//512} sectors total, standard 1.44MB floppy)")
    print(f"  - Hard Disk Image: {hdd_size} bytes ({hdd_size//512} sectors total, 256MB FAT16 hdd)")

def run(tools):
    if not os.path.exists(OS_IMG):
        print(f"[ERROR] Disk image {OS_IMG} not found! Build it first.")
        sys.exit(1)
    if not os.path.exists(DISK_IMG):
        print(f"[ERROR] HDD image {DISK_IMG} not found! Build it first.")
        sys.exit(1)
        
    print("[RUN] Launching QEMU System Emulator...")
    # Run QEMU with floppy A (-fda) and secondary hard drive (-hda)
    cmd_qemu = [
        tools["qemu"],
        "-fda", OS_IMG,
        "-hda", DISK_IMG,
        "-boot", "a",
        "-display", "gtk,zoom-to-fit=on"
    ]
    # Run asynchronously so we don't lock Python process
    subprocess.Popen(cmd_qemu)
    print("  QEMU started. Enjoy your OS!")

def main():
    import argparse
    parser = argparse.ArgumentParser(description="NexusOS Build System")
    parser.add_argument("--clean", action="store_true", help="Clean build directory")
    parser.add_argument("--build", action="store_true", help="Compile and link OS")
    parser.add_argument("--run", action="store_true", help="Boot OS in QEMU")
    args = parser.parse_args()
    
    # If no flags are passed, do a full build & run
    if not (args.clean or args.build or args.run):
        args.build = True
        args.run = True
        
    if args.clean:
        clean()
        if not (args.build or args.run):
            return
            
    tools = verify_tools()
    
    if args.build:
        build(tools)
        
    if args.run:
        run(tools)

if __name__ == "__main__":
    main()
