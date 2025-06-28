# xv6 Filesystem Checker

A comprehensive filesystem integrity checker for xv6 filesystems, similar to `fsck` on Unix systems. Detects and reports eight categories of filesystem corruption through low-level disk analysis.

## Overview

This tool performs deep structural analysis of xv6 filesystem images to detect inconsistencies that could lead to data corruption or system instability. It validates filesystem metadata, directory structures, inode relationships, and block allocation consistency.

## Features

- **Comprehensive Validation**: Checks 8 categories of filesystem corruption
- **Low-level Analysis**: Direct disk block reading and parsing
- **Bitmap Verification**: Validates block allocation consistency  
- **Directory Structure**: Ensures proper parent-child relationships
- **Inode Integrity**: Verifies inode table consistency
- **Memory Efficient**: Minimal memory footprint with efficient algorithms

## Filesystem Corruption Detection

The checker validates the following filesystem properties:

### 1. Block Address Validation
**Error**: `ERROR: bad address in inode`
- Validates all direct and indirect block pointers
- Ensures addresses point to valid data blocks within filesystem bounds
- Checks both direct blocks and indirect block traversal

### 2. Directory Format Validation  
**Error**: `ERROR: directory not properly formatted`
- Verifies every directory contains `.` and `..` entries
- Ensures `.` entry points to directory's own inode
- Validates directory entry structure integrity

### 3. Parent-Child Consistency
**Error**: `ERROR: parent directory mismatch`
- Confirms `..` entries point to correct parent inodes
- Verifies bidirectional parent-child relationships
- Handles special case of root directory

### 4. Block Allocation Consistency
**Error**: `ERROR: address used by inode but marked free in bitmap`
- Cross-references inode block usage with allocation bitmap
- Ensures all referenced blocks are marked as allocated
- Validates both direct and indirect block references

### 5. Bitmap Accuracy
**Error**: `ERROR: bitmap marks block in use but it is not in use`
- Identifies blocks marked allocated but not referenced by any inode
- Prevents space leaks and allocation errors
- Accounts for system blocks (superblock, inodes, bitmap)

### 6. Block Reference Uniqueness
**Error**: `ERROR: address used more than once`
- Detects blocks referenced by multiple inodes
- Prevents data corruption from shared block references
- Maintains filesystem integrity invariants

### 7. Inode Reference Validation
**Error**: `ERROR: inode marked used but not found in a directory`
- Ensures all allocated inodes are reachable from directory tree
- Prevents orphaned inodes and resource leaks
- Validates filesystem connectivity

### 8. Directory Reference Integrity
**Error**: `ERROR: inode referred to in directory but marked free`
- Confirms directory entries point to allocated inodes
- Prevents references to uninitialized or freed inodes
- Maintains directory consistency

## Technical Implementation

### Core Architecture

**Block Reader** (`rblock` function):
```c
int rblock(uint bnum, void *buf)
```
- Low-level disk block reading interface
- Handles filesystem image file operations
- Provides foundation for all filesystem access

**Inode Management**:
```c
int read_inode(struct superblock *sb, uint inum, struct dinode *dip)
int check_inode_blocks(struct superblock *sb, struct dinode *dip)
```
- Reads inode data from disk blocks
- Validates block address ranges and allocation

**Directory Analysis**:
```c
int read_all_dirents(struct superblock *sb, struct dinode *dip, struct dirent *entries, int max_entries)
int check_dot_and_dotdot(struct dirent *entries, int count, uint self_inum)
```
- Parses directory entries from data blocks
- Validates directory structure requirements

**Bitmap Operations**:
```c
int is_block_allocated(struct superblock *sb, uint blockno)
int *build_block_reference_map(struct superblock *sb)
```
- Reads and interprets allocation bitmap
- Builds comprehensive block usage maps

### Data Structures

**Filesystem Metadata**:
- Superblock parsing and validation
- Inode table traversal and analysis
- Directory entry interpretation

**Reference Tracking**:
- Block reference counting for duplicate detection
- Inode reference mapping for orphan detection
- Parent-child relationship validation

## Building and Usage

### Prerequisites
- GCC compiler
- POSIX-compliant system

### Build Instructions
```bash
make
```

### Running the Checker
```bash
./chkfs filesystem.img
```

### Return Codes
- **0**: Filesystem is consistent (no errors)
- **1**: Corruption detected (specific error message printed)

## Testing

### Test with Clean Filesystem
```bash
./chkfs uncorrupted.img
# Should produce no output and return 0
```

### Test with Corrupted Filesystem
```bash
# Create corrupted test image
cp uncorrupted.img corrupted.img
./corruptfs corrupted.img 1  # Introduce type 1 corruption

# Run checker
./chkfs corrupted.img
# Should output: ERROR: bad address in inode
```

### Corruption Types for Testing
Use `corruptfs` tool to introduce specific corruption types (1-8) corresponding to the error categories above.

## Code Structure

```
├── chkfs.c             # Main checker implementation
├── kernel/             # xv6 filesystem headers
│   ├── fs.h           # Filesystem structure definitions  
│   ├── types.h        # Basic type definitions
│   └── stat.h         # File metadata structures
└── Makefile           # Build configuration
```

## Algorithm Complexity

- **Time Complexity**: O(n + m) where n = inodes, m = blocks
- **Space Complexity**: O(n) for reference tracking arrays
- **I/O Complexity**: Single pass through filesystem with minimal seeks

## Key Algorithms

### Block Reference Analysis
1. **Build Reference Map**: Single pass through all inodes to count block references
2. **Bitmap Comparison**: Compare reference counts with allocation bitmap
3. **Duplicate Detection**: Identify blocks referenced multiple times

### Directory Tree Validation  
1. **Structure Check**: Validate `.` and `..` in every directory
2. **Parent Verification**: Confirm bidirectional parent-child relationships
3. **Reachability Analysis**: Ensure all inodes reachable from root

### Inode Consistency
1. **Address Validation**: Check all direct/indirect block addresses
2. **Type Verification**: Ensure inodes match directory references
3. **Allocation Consistency**: Cross-reference with inode allocation table

## Educational Context

This project demonstrates understanding of:
- Filesystem data structures and layout
- Low-level disk operations and binary data parsing
- Graph algorithms for tree validation
- Memory management and efficient algorithms
- Systems programming and debugging techniques

## Technical Skills Demonstrated

- **C Programming**: Complex data structure manipulation and pointer arithmetic
- **Systems Programming**: Low-level file I/O and binary data parsing
- **Algorithm Design**: Efficient graph traversal and validation algorithms
- **Debugging**: Systematic approach to finding filesystem inconsistencies
- **Software Engineering**: Modular design with helper functions and clear interfaces

## Filesystem Knowledge

- **Disk Layout**: Superblock, inode tables, data blocks, allocation bitmaps
- **Directory Structures**: Directory entries, parent-child relationships
- **Indirect Blocks**: Multi-level indirection for large files
- **Allocation Algorithms**: Block allocation and deallocation strategies

## License

Based on xv6 filesystem format, which is MIT licensed. Checker implementation is original work.

## Acknowledgments

Built for xv6 filesystem format developed at MIT. Filesystem checker logic and implementation is original work demonstrating deep understanding of filesystem internals.
