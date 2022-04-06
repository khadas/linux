.. Permission is granted to copy, distribute and/or modify this
.. document under the terms of the GNU Free Documentation License,
.. Version 1.1 or any later version published by the Free Software
.. Foundation, with no Invariant Sections, no Front-Cover Texts
.. and no Back-Cover Texts. A copy of the license is included at
.. Documentation/media/uapi/fdl-appendix.rst.
..
.. TODO: replace it to GFDL-1.1-or-later WITH no-invariant-sections

.. _DMX_SET_DECODE_INFO:

===================
DMX_SET_DECODE_INFO
===================

Name
----

DMX_SET_DECODE_INFO


Synopsis
--------

.. c:function:: int ioctl(fd, DMX_SET_DECODE_INFO, &decode_info)
    :name: DMX_SET_DECODE_INFO


Arguments
---------

``fd``
    File descriptor returned by :c:func:`open() <dvb-dmx-open>`.

``decode_info``
    decode_info is struct decoder_mem_info, it describe the decoder rp


Description
-----------

This ioctl call allows to set the decoder rp to dmx, it will use this for flow control.

Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
