#!/bin/bash

# This batch file is called from the .bashrc
# It each time a new terminal is spawned
# We put here the final steps that make distribution ready to use.
# However, these steps must be executed only exactly once, not each time
# a new terminal is spawned. File "DISTRO_INIT_DONE" flags that initialization
# was done.
if [ ! -f /root/DISTRO_INIT_DONE ]; then
    touch /root/DISTRO_INIT_DONE
    # Remaining steps of the installation:

    # Fix locale, mainly for tmux:
    localectl set-locale LANG=en_US.UTF-8
    gawk '{if(/#.*en_US.UTF-8/){print gensub(/#[ \t]*(.*)/, "\\1", "g", $0)}else{print}}' /etc/locale.gen > /etc/locale.tmp
    mv /etc/locale.tmp /etc/locale.gen
    locale-gen

    # Mount a read/write work partition
    source /root/mount_rw_partition.sh

    # prevent screen from turning off
    setterm -blank 0 -powerdown 0

    # launch nmtui, to give the user the possibility to connect via Wi-Fi
    nmtui

    # Launch Oxford
    /root/oxford/oxford/bin/Release/oxford
fi

# Code below can be executed for each new terminal

