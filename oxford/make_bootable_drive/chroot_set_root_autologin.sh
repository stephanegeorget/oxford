#!/bin/bash
# [SGEORGET20191206]
# Make sure root autologins at startup
# See https://superuser.com/questions/969923/automatic-root-login-in-debian-8-0$
pushd /lib/systemd/system
awk '{if(/ExecStart=/){print "ExecStart=-/sbin/agetty --noclear -a root %I $TERM"}else{print}}' getty\@.service > getty\@.service.tmp
mv getty\@.service.tmp getty\@.service
popd


