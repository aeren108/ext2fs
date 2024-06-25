# EXT2 File System

## How To Build

Run `make all` in the project directory.

It will produce an executable called `je2fs` in the `bin/` directory.

## Testing the File System

First create an image file for the disk with 512 blocks of size 1024 bytes.

```
dd if=/dev/zero of=disk.img bs=1024 count=512
```

Format the disk with ext2 file system using mke2fs.

```
mke2fs -t ext2 -b 2048 -N 64 disk.img
```

Program is going to work with the `disk.img` file you created.
In order to monitor the filesystem you can mount it to a directory.

```
sudo mount -o loop disk.img fs-root/
```

### Commands


```
./je2fs disk.img mkdir </abs/path/to/dirname>
```

```
./je2fs disk.img rmdir </abs/path/to/directory>
```

```
./je2fs disk.img rm </abs/path/to/file>
```

