# Project 03: Filesystem checker for xv6

## Overview
You will develop a file system checker that reads in a disk image containing an xv6 filesystem and checks for inconsistencies.

### ❗Important tips❗
* **Work on the project in short (1-3 hour) sessions over multiple days.** This will give you time to contemplate problems and ask questions. The opportunity to revise your project is contingent upon your git commit history demonstrating that you started the project at least one week before it was due and worked on the project over multiple days.
* **Abundantly create and use helper functions.** Several types of errors will require you to read/examine the same filesystem structure (i.e., an inode) or repeat the same task for all instances of the same structure (e.g., all inodes). You should create and use separate functions for specific subtasks (e.g., reading an indode, reading a directory entry) and separate functions for reading/examining one instance versus all instances of a structure (e.g., handling a single inode verus iterating over all inodes). 
* **Review code your partner wrote.** You should understand and be able to explain any code your partner (co-)authored. It is especially important to review any code your partner wrote without you to ensure you do not duplicate code (e.g., both write functions that iterate over all of an inode's data blocks).
* **Leverage what you learned in labs 7 and 8, and consult the [xv6 book](https://pdos.csail.mit.edu/6.1810/2024/xv6/book-riscv-rev4.pdf), [class notes](https://drive.google.com/drive/u/1/folders/1DUV8HfJMPD8Kqi5kHrhllLS2gsCPaCLb), and [OSTEP](https://pages.cs.wisc.edu/~remzi/OSTEP/).**

## Getting started 
Clone your repository in your home directory on your assigned tiger server.

All of the files in the `kernel` and `mkfs` directories are copied directly from the [xv6 source code](https://github.com/colgate-cosc301-spring25/xv6-riscv), which is covered by the MIT License contained in the `LICENSE` file.

The code for your filesystem checker should be put in `chkfs.c` and compiled using `make`.

## Errors to detect
Your checker should verify each of the properties listed below are satisfied by the file system on the disk. When one of these does not hold, print the **exact** error message shown below and exit immediately with return code 1 (i.e., `return 1` from the main function or call `exit(1)` from any function).

1. For in-use inodes, each block that is (in)directly pointed-to by the inode is valid (i.e., points to a valid data block address within the image). Note: you must check indirect blocks too, when they are in use. 
    ```
    ERROR: bad address in inode
    ```
2. Each directory contains `.` and `..` entries. 
    ```
    ERROR: directory not properly formatted
    ```
3. Each `..` entry in directory refers to the proper parent inode, and parent inode points back to it. 
    ```
    ERROR: parent directory mismatch
    ```
4. Each block (in)directly pointed-to from an in-use inode is also marked in use in the bitmap.
    ```
    ERROR: address used by inode but marked free in bitmap
    ```
5. Each block marked in-use in the bitmap is (in)directy pointed-to by an inode. 
    ```
    ERROR: bitmap marks block in use but it is not in use
    ```
6. Each block (in)directly pointed-to by an in-use inode is only (in)directly pointed-to from one inode.
    ```
    ERROR: address used more than once
    ```
7. Each inode marked used in the inode table must be referred to in at least one directory. 
    ```
    ERROR: inode marked used but not found in a directory
    ```
8. Each inode referenced by an entry for a valid directory is actually marked in use in inode table. 
    ```
    ERROR: inode referred to in directory but marked free
    ```

If the file system image does not contain any inconsistencies, no error messages should be printed (other debug output is fine) and `chkfs` should return code 0.

You'll need to read specific sectors (or blocks) of the file system image to access the superblock, free blocks bitmap, portions of the inode table, and data blocks. You can read blocks of the image using the `rblock` function included in `chkfs.c`.

Your code **must be broken into functions**; do not put all of the code in main. Furthermore, you should **avoid duplicate code**; if multiple functions contain the same block of code, move the duplicated code into a separate function that is called by the other functions.

You are welcome to copy code from `mkfs/mkfs.c` to `chkfs.c`, but you **must**:
1. Include a comment indicating the original source of the code
2. Be able to explain what the code is doing

## Testing and debugging
Your code must output the **exact** error messages listed above—the error message must appear on its own line with no other text—when an inconsistency within the file system is detected. If desired, you may output additional information on separate lines to help you understand the file system structure and debug your code.

You should start by running your code on the `uncorrupted.img` file included in your repository:
```bash
./chkfs uncorrupted.img
```
No errors should be reported for this disk image.

You should also test that your checker can detect inconsistencies. You can use the program `corruptfs` (included in your repo) to introduce specific types of inconsistencies into the `xv6` filesystem. First, create a copy of the uncorrupted filesystem image:
```bash
cp uncorrupted.img corrupted.img
```
Then, run corruptfs as follows:
```bash
./corruptfs corrupted.img TYPE
```
replacing `TYPE` with the number corresponding to the error in the above list that you want to introduce. Finally, run `chkfs` on the file system image `corrupted.img`. Your checker should output the appropriate error message for the corruption you added and no other error messages.

I recommend you only apply one corruption at a time to a file system image. In other words, to test a different type of corruption, you should re-execute both of the above commands, replacing `TYPE` with the new type of corruption you want to test.

The `corruptfs` program randomly chooses which directory, inode, etc. to corrupt. To make the random selection deterministic, include a random seed (i.e., a positive integer) as an additional command line argument to `corruptfs`. Also, because of the random nature of `corruptfs`, it will occasionally make a random choice such that it is impossible to introduce the specified type of corruption. If this occurs, `corruptfs` will indicate that it failed to corrupt the image; simply run `corruptfs` again (with a different random seed) so it makes a different random choice.

## Functionality
A project whose functionality is **satisfactory** must have two or fewer minor bugs. Failing to detect a specific type of error under some scenarios or falsely reporting a specific type of error under some scenarios is a minor bug.

A project whose functionality **needs minor improvements** must have two or fewer major bugs. Never detecting a specific type of error or always reporting a specific type of error is a major bug.

## Design

A project whose design is **satisfactory** must adhere to **all of the following**:
* **No errors or warnings during compilation:** You may be tempted to ignore compiler warnings, but they almost always mean there is a bug in your code.
* **Use multiple functions:** As a general rule of thumb, a function is too long if you must scroll in VS Code to see the entire function and/or there are more than three levels of nesting (e.g., an if statement within a for loop within an if statement within a while loop).
* **Avoid duplicated code:** Do not repeat the same (or very similar) code in multiple places in a function or in multiple functions. Instead, put the repeated code into a separate "helper" function that is called by other functions that rely on this code.
* **Minimize the use of global variables:** Use local variables and parameters whenever possible.
* **No memory errors or leaks:** Run `valgrind` to confirm all memory is freed and no other memory errors exist in your program.
* **Include comments:** Each function (except for main) and struct definition, must be preceded by a short comment that describes what the function/struct does.

A project that adheres to **half to three-quarters of the above** has a design that **needs minor improvements**.

## Submission instructions
You should **commit and push** your updated files to your git repository. However, do not wait until your entire implementation is working before you commit it to your git repository; you should commit your code each time you write and debug a piece of functionality. 

## Acknowledgemnts
This project is derived from the _File System Checker_ project developed by Remzi and Andrea Arpaci-Dusseau at the University of Wisconsin-Madison.