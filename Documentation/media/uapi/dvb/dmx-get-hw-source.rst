.. Permission is granted to copy, distribute and/or modify this
.. document under the terms of the GNU Free Documentation License,
.. Version 1.1 or any later version published by the Free Software
.. Foundation, with no Invariant Sections, no Front-Cover Texts
.. and no Back-Cover Texts. A copy of the license is included at
.. Documentation/media/uapi/fdl-appendix.rst.
..
.. TODO: replace it to GFDL-1.1-or-later WITH no-invariant-sections

.. _DMX_GET_HW_SOURCE:

==============
DMX_GET_HW_SOURCE
==============

Name
----

DMX_GET_HW_SOURCE


Synopsis
--------

.. c:function:: int ioctl(fd, DMX_GET_HW_SOURCE, &source)
    :name: DMX_GET_HW_SOURCE


Arguments
---------

``fd``
    File descriptor returned by :c:func:`open() <dvb-dmx-open>`.

``source``
    source is DMA0~7/FRONTEND0~7


Description
-----------

This ioctl call allows to get dmx source, this source is one stream id.

Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
