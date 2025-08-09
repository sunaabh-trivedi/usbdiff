# usbdiff

**`usbdiff`** is a lightweight command-line tool to detect and track changes in directories. It compares the contents of a given directory to a saved snapshot and highlights new, deleted, or modified files. Optionally, it can **copy changed files** to a target backup directory, making it useful for incremental USB backups and file synchronisation.

Written in C, with minimal dependencies, and designed for performance and portability.

---

# Installation

After locally cloning the repository, the binary can be built simply by invoking `make`. 

On Windows, the **`setup.bat`** script will add the executable to the PATH so it can be run from anywhere in the system. On Linux, the **`setup.sh`** script should be used instead.

Then, the diff of a directory can be generated with

```
usbdiff <directory>
```

To copy only the files that have changed (i.e., files that will appear in the diff output) to a new directory, the `--copy-to` flag can be used

```
usbdiff --copy-to <destination dir> <source dir>
```

# Features

- Detects:
  - **New files** not present in the previous snapshot
  - **Deleted files** that no longer exist
  - **Modified files** based on size and timestamps
- Optional: **Copy changed files** to a backup directory
- Human-readable diff output
- JSON snapshot of directory produced in **.usbdiff.json**
- Cross-platform support (Linux and Windows)