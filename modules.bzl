# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 The Android Open Source Project

"""
This module contains a full list of kernel modules
 compiled by GKI.
"""

load("//common:common_drivers/modules.bzl", "ALL_MODULES_REMOVE")

_COMMON_GKI_MODULES_LIST = [
    # keep sorted
    "drivers/block/zram/zram.ko",
    "drivers/bluetooth/btbcm.ko",
    "drivers/bluetooth/btqca.ko",
    "drivers/bluetooth/btsdio.ko",
    "drivers/bluetooth/hci_uart.ko",
    "drivers/net/can/dev/can-dev.ko",
    "drivers/net/can/slcan.ko",
    "drivers/net/can/vcan.ko",
    "drivers/net/mii.ko",
    "drivers/net/ppp/bsd_comp.ko",
    "drivers/net/ppp/ppp_deflate.ko",
    "drivers/net/ppp/ppp_generic.ko",
    "drivers/net/ppp/ppp_mppe.ko",
    "drivers/net/ppp/pppox.ko",
    "drivers/net/ppp/pptp.ko",
    "drivers/net/slip/slhc.ko",
    "drivers/net/usb/aqc111.ko",
    "drivers/net/usb/asix.ko",
    "drivers/net/usb/ax88179_178a.ko",
    "drivers/net/usb/cdc_eem.ko",
    "drivers/net/usb/cdc_ether.ko",
    "drivers/net/usb/cdc_ncm.ko",
    "drivers/net/usb/r8152.ko",
    "drivers/net/usb/r8153_ecm.ko",
    "drivers/net/usb/rtl8150.ko",
    "drivers/net/usb/usbnet.ko",
    "drivers/usb/class/cdc-acm.ko",
    "drivers/usb/serial/ftdi_sio.ko",
    "drivers/usb/serial/usbserial.ko",
    "kernel/kheaders.ko",
    "lib/crypto/libarc4.ko",
    "mm/zsmalloc.ko",
    "net/6lowpan/6lowpan.ko",
    "net/6lowpan/nhc_dest.ko",
    "net/6lowpan/nhc_fragment.ko",
    "net/6lowpan/nhc_hop.ko",
    "net/6lowpan/nhc_ipv6.ko",
    "net/6lowpan/nhc_mobility.ko",
    "net/6lowpan/nhc_routing.ko",
    "net/6lowpan/nhc_udp.ko",
    "net/8021q/8021q.ko",
    "net/bluetooth/bluetooth.ko",
    "net/bluetooth/hidp/hidp.ko",
    "net/bluetooth/rfcomm/rfcomm.ko",
    "net/can/can.ko",
    "net/can/can-bcm.ko",
    "net/can/can-gw.ko",
    "net/can/can-raw.ko",
    "net/ieee802154/6lowpan/ieee802154_6lowpan.ko",
    "net/ieee802154/ieee802154.ko",
    "net/ieee802154/ieee802154_socket.ko",
    "net/l2tp/l2tp_core.ko",
    "net/l2tp/l2tp_ppp.ko",
    "net/mac802154/mac802154.ko",
    "net/nfc/nfc.ko",
    "net/rfkill/rfkill.ko",
    "net/tipc/diag.ko",
    "net/tipc/tipc.ko",
]

# Deprecated - Use `get_gki_modules_list` function instead.
COMMON_GKI_MODULES_LIST = _COMMON_GKI_MODULES_LIST

_ARM64_GKI_MODULES_LIST = [
    # keep sorted
]

_RISCV64_GKI_MODULES_LIST = [
    # keep sorted
]

_X86_64_GKI_MODULES_LIST = [
    # keep sorted
]

# buildifier: disable=unnamed-macro
def get_gki_modules_list(arch = None):
    """ Provides the list of GKI modules.

    Args:
      arch: One of [arm64, x86_64, riscv64].

    Returns:
      The list of GKI modules for the given |arch|.
    """
    gki_modules_list = [] + _COMMON_GKI_MODULES_LIST
    if arch == "arm64":
        gki_modules_list += _ARM64_GKI_MODULES_LIST
    elif arch == "x86_64":
        gki_modules_list += _X86_64_GKI_MODULES_LIST
    elif arch == "riscv64":
        gki_modules_list += _RISCV64_GKI_MODULES_LIST
    else:
        fail("{}: arch {} not supported. Use one of [arm64, x86_64, riscv64]".format(
            str(native.package_relative_label(":x")).removesuffix(":x"),
            arch,
        ))

    remove_modules_items = {module: None for module in depset(ALL_MODULES_REMOVE).to_list()}
    gki_modules_list = [module for module in depset(gki_modules_list).to_list() if module not in remove_modules_items] \
			if remove_modules_items else gki_modules_list

    return gki_modules_list
