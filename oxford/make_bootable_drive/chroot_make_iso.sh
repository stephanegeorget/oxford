#!/bin/bash -x
# run apt-cache search linux-image to find the proper version of linux image and HEADERS to install.
# These change from time to time with a new release of debootstrap.

cd /root
chmod +x *.sh

echo "oxford-live" > /etc/hostname

if [ "$1" != "debug" ]; then

apt-get update
apt-get install --no-install-recommends --assume-yes \
    linux-image-4.9.0-12-rt-amd64 \
    linux-headers-4.9.0-12-rt-amd64 \
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

echo "root:toor" | chpasswd

. chroot_set_root_autologin.sh


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
mv /etc/systemd/logind.conf{.wip,}




#echo "Press any key..."
#read -n 1 -s

cd /root

# Fix link to linux headers to make it easy to find
ln -s /usr/src/linux-headers-4.9.0-12-rt-amd64 /usr/src/linux-headers-$(uname -r)

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

cd oxford/oxford/cdk-5.0-20171209
./configure
make -B
make -B install
cd ..
make -B

# Install Node-RED
apt-get install --no-install-recommends --assume-yes ca-certificates
bash <(curl -sL https://raw.githubusercontent.com/node-red/linux-installers/master/deb/update-nodejs-and-nodered)

# Install node-red-dashboard
pushd /root/.node-red
npm install node-red-dashboard
popd

# Copy this project's main Node-RED configuration file
pwd
cp node-red-oxford-flows.json /root/.node-red/flows_$(cat /etc/hostname).json

# Make sure it starts automatically
systemctl enable nodered.service



fi

# Ensure that the flag that triggers the setup finalization is unset
rm /root/DISTRO_INIT_DONE

