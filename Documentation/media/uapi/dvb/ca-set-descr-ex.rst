.. Permission is granted to copy, distribute and/or modify this
.. document under the terms of the GNU Free Documentation License,
.. Version 1.1 or any later version published by the Free Software
.. Foundation, with no Invariant Sections, no Front-Cover Texts
.. and no Back-Cover Texts. A copy of the license is included at
.. Documentation/media/uapi/fdl-appendix.rst.
..
.. TODO: replace it to GFDL-1.1-or-later WITH no-invariant-sections

.. _CA_SET_DESCR_EX:

===============
CA_SET_DESCR_EX
===============

Name
----

CA_SET_DESCR_EX


Synopsis
--------

.. c:function:: int ioctl(int fd, CA_SET_DESCR, struct ca_descr_ex *desc)
    :name: CA_SC2_SET_DESCR_EX


Arguments
---------

``fd``
  File descriptor returned by a previous call to :c:func:`open() <cec-open>`.

``desc``
  Pointer to struct :c:type:`ca_sc2_descr_ex`.


Description
-----------

.. note:: This ioctl is undocumented. Documentation is welcome.


Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
