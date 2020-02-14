#!/bin/bash -x

# We're back from the chroot, from which we have finished to
# install all that is needed by this live distro.

# We had previously (_A script) mounted dev/proc/sys to help
# expedite some installers while in the chroot.
# We need to unmount them now, else the squashfs will put them
# in the .iso, and we don't want any of dev/proc/sys on the .iso
# of course, as these are virtual (pseudo) filesystems.
umount $PROJECT_HOME/LIVE_BOOT/chroot/dev
umount $PROJECT_HOME/LIVE_BOOT/chroot/proc
umount $PROJECT_HOME/LIVE_BOOT/chroot/sys

# Clear temporary folders out of previous installations
rm -r $PROJECT_HOME/LIVE_BOOT/scratch
rm -r $PROJECT_HOME/LIVE_BOOT/image

# chroot folder contains the live distribution.
# Compress it into a squashfs.
mkdir -p $PROJECT_HOME/LIVE_BOOT/{scratch,image/live}
sudo mksquashfs \
    $PROJECT_HOME/LIVE_BOOT/chroot \
    $PROJECT_HOME/LIVE_BOOT/image/live/filesystem.squashfs \
    -e boot

# Copy the kernel and initramfs, from inside the chroot to the
# live directory - that will be used by grub just after
cp $PROJECT_HOME/LIVE_BOOT/chroot/boot/vmlinuz-* \
    $PROJECT_HOME/LIVE_BOOT/image/vmlinuz && \
cp $PROJECT_HOME/LIVE_BOOT/chroot/boot/initrd.img-* \
    $PROJECT_HOME/LIVE_BOOT/image/initrd

# Create a menu configuration for grub.
# Grub uses the 'search' commandto infer which device contains
# the live environment, looking for a file named DEBIAN_CUSTOM
cat <<'EOF' >$PROJECT_HOME/LIVE_BOOT/scratch/grub.cfg
search --set=root --file /DEBIAN_CUSTOM
insmod all_video
set default="0"
set timeout=30
menuentry "Toshiba M10" {
    gfxpayload=1280x800x32,1280x800
    linux /vmlinuz boot=live quiet nomodeset
    initrd /initrd
}
menuentry "Dell D610" {
    gfxpayload=1366x768x32,1366x768
    linux /vmlinuz boot=live quiet nomodeset
    initrd /initrd
}
menuentry "Dell Precision 5530" {
    gfxpayload=1920x1080x32,1920x1080
    linux /vmlinuz boot=live quiet nomodeset
    initrd /initrd
}
menuentry "default resolution" {
    linux /vmlinuz boot=live quiet nomodeset
    initrd /initrd
}
EOF
# END OF GRUB MENU


# Create the special (empty) file that Grub will look for
# This name matches the name found in the Grub configuration file
# just above.
touch $PROJECT_HOME/LIVE_BOOT/image/DEBIAN_CUSTOM

# Create a BIOS-bootable .ISO
grub-mkstandalone \
    --format=i386-pc \
    --output=$PROJECT_HOME/LIVE_BOOT/scratch/core.img \
    --install-modules="linux normal iso9660 biosdisk memdisk search tar ls vga vga_text videoinfo video videotest" \
    --modules="linux normal iso9660 biosdisk search vga vga_text videoinfo video videotest" \
    --locales="" \
    --fonts="" \
    "boot/grub/grub.cfg=$PROJECT_HOME/LIVE_BOOT/scratch/grub.cfg"

cat \
    /usr/lib/grub/i386-pc/cdboot.img \
    $PROJECT_HOME/LIVE_BOOT/scratch/core.img \
> $PROJECT_HOME/LIVE_BOOT/scratch/bios.img

xorriso \
    -as mkisofs \
    -iso-level 3 \
    -full-iso9660-filenames \
    -volid "DEBIAN_CUSTOM" \
    --grub2-boot-info \
    --grub2-mbr /usr/lib/grub/i386-pc/boot_hybrid.img \
    -eltorito-boot \
        boot/grub/bios.img \
        -no-emul-boot \
        -boot-load-size 4 \
        -boot-info-table \
        --eltorito-catalog boot/grub/boot.cat \
    -output "${PROJECT_HOME}/LIVE_BOOT/debian-custom.iso" \
    -graft-points \
        "${PROJECT_HOME}/LIVE_BOOT/image" \
        /boot/grub/bios.img=$PROJECT_HOME/LIVE_BOOT/scratch/bios.img

