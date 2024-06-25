sudo umount fs-root/
rm disk.img
dd if=/dev/zero of=disk.img bs=1024 count=512
mke2fs -t ext2 -b 2048 -N 64 disk.img
sudo mount -o loop disk.img fs-root/