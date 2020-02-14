#!/bin/bash
echo ">>>>>>>>>> In script $0"

# The distribution will mount the RO 'live'  partition by itself,
# and normally, we have created another RW partition next to it.

disk=/dev/$(lsblk --list | gawk '{if(/\/lib\/live\/mount\/medium/){print gensub(/([a-zA-Z]+)([0-9]+)/, "\\1", "g", $1); exit}}')2


echo "Assuming RW partition on ${disk}"
echo "Checking filesystem..."
fsck.ext4 -y ${disk}
mount ${disk} /mnt -o data=journal
