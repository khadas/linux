.. Permission is granted to copy, distribute and/or modify this
.. document under the terms of the GNU Free Documentation License,
.. Version 1.1 or any later version published by the Free Software
.. Foundation, with no Invariant Sections, no Front-Cover Texts
.. and no Back-Cover Texts. A copy of the license is included at
.. Documentation/media/uapi/fdl-appendix.rst.
..
.. TODO: replace it to GFDL-1.1-or-later WITH no-invariant-sections

.. _KEY_GET_FLAG:

==============
KEY_GET_FLAG
==============

Name
----

KEY_GET_FLAG


Synopsis
--------
.. code-block:: c

    int ioctl( int fd, KEY_GET_FLAG, struct key_desc *params)

Arguments
---------

``fd``
    File descriptor returned by open /dev/key.

``params``

    Pointer to structure containing key desc parameters.
    key[0] will return the flag


Description
-----------

This ioctl call return the key status for debug
the key status return from the key[0] in the struct key_descr

Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
