
# xv6 Filesystem using FUSE

A userspace filesystem that allows Linux to access and modify an xv6 filesystem image using FUSE.

## Features

Implemented FUSE operations:
- getattr
- readdir
- read
- write
- create
- mkdir
- unlink
- truncate

## Requirements

- Ubuntu Linux / WSL2
- GCC
- FUSE3
- pkg-config

## Installation

sudo apt update
sudo apt install build-essential gcc make fuse3 libfuse3-dev pkg-config git

## Setup

git clone YOUR_GITHUB_REPOSITORY_URL
cd os-fuse-xv6

cp images/fs-team.img images/fs-work.img

## Compilation

cd src

gcc -Wall -Wextra -D_FILE_OFFSET_BITS=64 \
main.c disk.c inode.c filedata.c path.c alloc.c \
$(pkg-config --cflags --libs fuse3) -o xv6fs

## Running

mkdir -p /mnt/xv6

./xv6fs ../images/fs-work.img /mnt/xv6 -f

## Testing

ls -la /mnt/xv6

echo "Hello FUSE" > /mnt/xv6/test.txt
cat /mnt/xv6/test.txt

mkdir /mnt/xv6/testdir

truncate -s 5 /mnt/xv6/test.txt

rm /mnt/xv6/test.txt

## Unmounting

Press Ctrl+C in the terminal running FUSE.

Alternatively:

fusermount3 -u /mnt/xv6

## Important Notes

- Always work on fs-work.img.
- Never modify the same image simultaneously through xv6 and FUSE.
- This is an educational implementation without transactional crash recovery.
