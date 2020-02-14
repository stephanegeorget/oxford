#!/bin/bash -x


# Start with installing all the tools we need to create a
# live image
apt-get update

apt-get install \
    debootstrap \
    squashfs-tools \
    xorriso \
    grub-pc-bin \
    grub-efi-amd64-bin \
    mtools

# Create a working directory for the live image.
mkdir $PROJECT_HOME/LIVE_BOOT


# The following block is extremely long to complete
# If "debug" is passed as a parameter, skip this
if [ "$1" != "debug" ]; then
debootstrap \
    --arch=amd64 \
    --variant=minbase \
    stretch \
    $PROJECT_HOME/LIVE_BOOT/chroot \
    http://ftp.us.debian.org/debian/

fi

# The following scripts must run from the image folders,
# either from the chrooted environment, or while running
# live (on the target machine).
cp chroot_make_iso.sh $PROJECT_HOME/LIVE_BOOT/chroot/root
cp live_autorun.sh $PROJECT_HOME/LIVE_BOOT/chroot/root
cp chroot_set_root_autologin.sh $PROJECT_HOME/LIVE_BOOT/chroot/root
cp mount_rw_partition.sh $PROJECT_HOME/LIVE_BOOT/chroot/root


# While in the chroot environment, some installers will need
# access to some kind of proc/dev/sys filesystems.
# We pass the proc/dev/sys of the host environment (this computer)
# so these scripts/installers can complete.
# This is a trick to prepare the maximum possible in chroot,
# and leave as little pre-job housekeeping possible
# when running live (and so speed up the startup time).
mount --bind /proc $PROJECT_HOME/LIVE_BOOT/chroot/proc
mount --bind /dev $PROJECT_HOME/LIVE_BOOT/chroot/dev
mount --bind /sys $PROJECT_HOME/LIVE_BOOT/chroot/sys

# Chroot to the image in construction, and continue to execute
# installation scripts from within.
chroot $PROJECT_HOME/LIVE_BOOT/chroot /root/chroot_make_iso.sh $1


