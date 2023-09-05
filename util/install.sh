#!/bin/bash
# GDFS install script for Ubuntu.


ubuntu=`uname -v | grep -i -q "ubuntu"; echo $?`

if [ "$ubuntu" == "0" ]; then
  ./configure
  make
  sudo make install
  make clean

  if [ -f /bin/systemd ]; then
    cp ./util/gdfs.service /usr/src/systemd/system/gdfs.service
  else
    cp ./util/init_script /etc/init.d/gdfs
  fi
fi
