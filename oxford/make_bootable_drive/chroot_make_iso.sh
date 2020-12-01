#!/bin/bash -x
# run apt-cache search linux-image to find the proper version of linux image and HEADERS to install.
# These change from time to time with a new release of debootstrap.
# Do that from within the chroot environment, not from the host!

cd /root
chmod +x *.sh

echo "oxford-live" > /etc/hostname

if [ "$1" != "debug" ]; then

# The following packages must be in the "main" apt repository source list
apt-get update
apt-get install --no-install-recommends --assume-yes \
    linux-image-4.9.0-13-rt-amd64 \
    linux-headers-4.9.0-13-rt-amd64 \
    live-boot \
    systemd-sysv \
    network-manager \
    dhcpcd5 \
    wireless-tools \
    wpagui \
    curl \
    openssh-client \
    iputils-ping \
    nano \
    sudo \
    man \
    wget \
    gawk \
    usbutils \
    tree \
    screen \
    locales \
    tmux \
    iw \
    iproute2 \
    netbase \
    keyboard-configuration \
    console-setup

# Install wireless firmware for broadcom
# First, add what is required to add "contrib" and "non-free" to sources list
# For now, we should have only "main":
cat /etc/apt/sources.list
apt-get update
apt-get install --no-install-recommends --assume-yes software-properties-common
apt-get update
# Now we have access to apt-add-repository tool
# Add contrib and non-free to sources list
apt-add-repository non-free
apt-add-repository contrib
# Update apt sources with these non-free and contrib sources
apt-get update
# Now, firmware-b43-installer is accessible. Install it:
apt-get install --no-install-recommends --assume-yes firmware-b43-installer


# Set up root account
echo "root:toor" | chpasswd

# Call this script which set things up to automatically log in as root
. chroot_set_root_autologin.sh

# bashrc is the default terminal.
# Modify .bashrc to add our entry point script named live_autorun.sh
echo "Add entry in .bashrc"
gawk '{if(/live_autorun/){exit 1;}}' /root/.bashrc
if [ $? -eq 0 ]; then
    # .bashrc does not contain an entry point to this project
    # so add it now:
    echo "/root/live_autorun.sh" >> .bashrc
fi


echo "Fix logind.conf to disregard laptop lid close events"
# See https://unix.stackexchange.com/questions/200125/debian-8-jessie-laptop-stops-working-after-closing-the-laptop-lid
# Transform:
# #HandleLidSwitch[optionally_more_stuff]=whatever
# into:
# HandleLidSwitch[optionally_more_stuff]=ignore
gawk "{if(/HandleLidSwitch/){print gensub(/.*(HandleLidSwitch[a-zA-Z]*)=(.*)/, \"\\\\1=ignore\", \"g\", \$0)}else{print \$0}}" /etc/systemd/logind.conf > /etc/systemd/logind.conf.wip
# Line below is a shorthand for mv /etc/systemd/logind.conf.wip /etc/systemd/logind.conf
mv /etc/systemd/logind.conf{.wip,}


#echo "Press any key..."
#read -n 1 -s

cd /root

# Fix link to linux headers to make it easy to find
ln -s /usr/src/linux-headers-4.9.0-12-rt-amd64 /usr/src/linux-headers-$(uname -r)

# Some packages required to get Oxford going
apt-get install --no-install-recommends --assume-yes \
    usbutils \
    alsa-utils \
    midisport-firmware\
    console-data

# Install the necessary tools to build Oxford from source code
apt-get install --no-install-recommends --assume-yes \
    libpopt-dev \
    build-essential \
    git \
    libasound2-dev \
    libncurses5-dev \
    libncursesw5-dev \
    mosquitto \
    mosquitto-clients \
    libmosquitto-dev


apt-get clean

cd /root

git config --global http.sslVerify false

git clone https://www.github.com/stephanegeorget/oxford

# Build the Curses Development Kit (cdk)
cd oxford/oxford/cdk-5.0-20171209
./configure
make -B
make -B install
cd ..
# Build oxford
make -B
# No need to install, we'll call the binary from its build folder directly


# Install Node-RED
# But first ensure that ca-certificates is installed, to pull the script below from https
apt-get install --no-install-recommends --assume-yes ca-certificates
bash <(curl -sL https://raw.githubusercontent.com/node-red/linux-installers/master/deb/update-nodejs-and-nodered)
#apt-get install nodejs
npm cache clean --force
npm install -g --unsafe-perm node-red node-red-admin

# Install node-red-dashboard
pushd /root/.node-red
npm install node-red-dashboard
popd

# Copy this project's main Node-RED configuration file
pwd
cp node-red-oxford-flows.json /root/.node-red/flows_$(cat /etc/hostname).json

# Make sure Node-RED starts automatically
systemctl enable nodered.service



fi

# Ensure that the flag that triggers the setup finalization is unset
rm /root/DISTRO_INIT_DONE

