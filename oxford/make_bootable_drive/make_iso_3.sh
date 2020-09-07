#!/bin/bash -x

# This script takes one argument, the size of the USB device (or SD card, whatever)
# on which the live distribution will be written.
# This acts as a safeguard to prevent partitioning and formatting erasing the wrong media.
# use lsblk to find the media you want to write the live distro on.
#
# sdb      8:16   1 *3,8G* 0 disk  ==> call this script with argument 3,8G to select sdb
# ├─sdb1   8:17   1 1023M  0 part /media/root/63DD-1DC5
# └─sdb2   8:18   1  2,8G  0 part /media/root/648D-54B3
#


echo ">>>>>>>>>> In script $0"

if [ "$1" == "" ]; then
    echo "NEED ONE ARGUMENT - example: 3,8G"
    exit 1
else
    echo "Looking for device with size $1"
fi

disk=/dev/$(lsblk --list | awk "{if(/$1/){print \$1; exit}}")

if [ "${disk}" == "/dev/" ]; then
    echo "Could not find proper disk"
    exit 1
fi

# From now on, work with disk ${disk}
echo "Working with disk ${disk}"

echo "Make sure all partitions on that disk are unmounted"
sudo umount ${disk}1
sudo umount ${disk}2
sudo umount ${disk}3
sudo umount ${disk}4

echo "Nuke /mnt/usb in case there was some leftovers from previous failed install"
umount /mnt/usb
rm -r /mnt/usb

# then recreate this mount point - now we are sure it is completely empty
sudo mkdir -p /mnt/usb


echo "Nuke beginning of the drive"
# This is required in case the USB drive has leftovers of previous installations,
# mixing up MBR and GPT, etc.
dd if=/dev/zero of=${disk} seek=1 count=2047
# Now we're sure the disk is clean

# Partition the disk
# The first partition is for the live distribution
# The second partition is a read-write partition that can store data
# created while running live.
sudo parted --script $disk \
    mklabel msdos \
    mkpart primary fat32 1MiB 1GiB \
    mkpart primary ext2 1GiB 100%

sleep 2

# Format all with FAT32
sudo mkfs.vfat -F32 ${disk}1
sudo mkfs.ext4 ${disk}2

# Mount the first partition as /mnt/usb
# That will contain the live distribution.
sudo mount ${disk}1 /mnt/usb

# Install Grub disk $disk, with support files located on ${disk}1/boot
# that is accessible from mount point /mnt/usb/boot
sudo grub-install \
    --target=i386-pc \
    --boot-directory=/mnt/usb/boot \
    --recheck \
    $disk

# Adjust directory tree
sudo mkdir -p /mnt/usb/{boot/grub,live}

# Copy the squashfs file, which contains the bulk of the distibution files
sudo cp -r $PROJECT_HOME/LIVE_BOOT/image/* /mnt/usb/
# The above command returns immediately, but copying that file actually takes a while.
# The copying happens in the background

# Add the grub configuration file
sudo cp \
    $PROJECT_HOME/LIVE_BOOT/scratch/grub.cfg \
    /mnt/usb/boot/grub/grub.cfg

# Manually unmount /mnt/usb. This command will return when the squashfs
# copy is over. Again, it might take several minutes.


echo "Unmounting device {disk}"
echo "DO NOT UNPLUG the USB drive until it has been properly unmounted."
echo "This might take several minutes"
echo ".............................."

sudo umount /mnt/usb

echo "{disk} unmounted - "
echo "You can unplug the USB drive"
