.. Permission is granted to copy, distribute and/or modify this
.. document under the terms of the GNU Free Documentation License,
.. Version 1.1 or any later version published by the Free Software
.. Foundation, with no Invariant Sections, no Front-Cover Texts
.. and no Back-Cover Texts. A copy of the license is included at
.. Documentation/media/uapi/fdl-appendix.rst.
..
.. TODO: replace it to GFDL-1.1-or-later WITH no-invariant-sections

.. _DMX_SET_SEC_MEM:

===============
DMX_SET_SEC_MEM
===============

Name
----

DMX_SET_SEC_MEM


Synopsis
--------

.. c:function:: int ioctl(fd, DMX_SET_SEC_MEM, &dvr_mem)
    :name: DMX_SET_SEC_MEM


Arguments
---------

``fd``
    File descriptor returned by :c:func:`open() <dvb-dmx-open>`.

``dvr_mem``
    dvr_mem is struct dmx_sec_mem for filter mem, it will receive dvr data, this mem is phy mem


Description
-----------

This ioctl call allows to set dvr phy mem for get dvr data.

Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
