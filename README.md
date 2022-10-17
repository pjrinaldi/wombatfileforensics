# Wombat File Forensics
Linux command line tool to display forensic artifacts for files from mounted filesystems. The tool will display the forensic artifacts for the file system, i.e. FAT, NTFS, EXT, HFS, etc..
The tool will make use of blake3 to provide hashing of file content, the ability to export the files contents to a file with or without slack.

This tool will be the basis for the implementation of wombatlogical imaging tool, so wombatlogical will be on hold while I develop this.

## Current File Systems In Progress
* NTFS
* EXT2, EXT3, EXT4

## Current File Systems Implemented
* FAT12, FAT16, FAT32, EXFAT
