April 28, 2011

Linux CIMaX+ Kernel driver.
Release Code version:

DISCLAIMER
----------

This is a Smardtv' Linux Kernel driver for use with
the CIMaX+ USB reference board hardware.

IMPORTANT NOTE
--------------

This packages contains a Linux Kernel driver to be installed in the
/lib/modules target directory in order to be loaded automaticaly.

1. High level driver: cimax+usb_driver.ko
  This driver provide the high level interface to control the CIMaX+ USB interface.

2. Firmware
  To have a functional hardware, the firmware must be uploaded to the device.

  The firmwares is delivered in 2 files for dedicated usage and must be
  installed in the Linux firmware directory on the target board:

    - /lib/firmware

  A) CIMaX+ firmware binary file
    Copy the following firmware file to the /lib/firmware directory:
      - firmware/cimax+_usbdvb.bin

  B) CIMaX+ configuration file
    Copy the following firmware file to the /lib/firmware directory:
      - firmware/cimax+usb.cfg


BUILD AND INSTALLATION INSTRUCTIONS
-----------------------------------

2. Building LINUX USB DVB driver
  On the target machine performs the following commands:

  1. cd src
  2. make
  3. make install

3. Installing firmware files
  On the target machine performs the following command:

  1. cp firmware/*.* /lib/firmware/

4. Installing rules file
  1. cp src/99-cimax+usb.rules /etc/udev/rules.d/
  2. udevadm control --reload-rules

5. Build modules dependencies
  On the target machine performs the following command:

  1. depmod -a

