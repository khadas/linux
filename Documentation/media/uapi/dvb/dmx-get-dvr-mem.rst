.. Permission is granted to copy, distribute and/or modify this
.. document under the terms of the GNU Free Documentation License,
.. Version 1.1 or any later version published by the Free Software
.. Foundation, with no Invariant Sections, no Front-Cover Texts
.. and no Back-Cover Texts. A copy of the license is included at
.. Documentation/media/uapi/fdl-appendix.rst.
..
.. TODO: replace it to GFDL-1.1-or-later WITH no-invariant-sections

.. _DMX_GET_DVR_MEM:

==============
DMX_GET_DVR_MEM
==============

Name
----

DMX_GET_DVR_MEM


Synopsis
--------

.. c:function:: int ioctl(fd, DMX_GET_DVR_MEM, &mem_info)
    :name: DMX_GET_DVR_MEM


Arguments
---------

``fd``
    File descriptor returned by :c:func:`open() <dvb-dmx-open>`.

``mem_info``
    mem_info is struct dvr_mem_info, it will get wp_offset in dvr mem.


Description
-----------

This ioctl call allows to get dvr mem offset in dvr phy mem.

Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. 


The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
