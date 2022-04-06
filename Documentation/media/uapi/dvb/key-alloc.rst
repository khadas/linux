.. Permission is granted to copy, distribute and/or modify this
.. document under the terms of the GNU Free Documentation License,
.. Version 1.1 or any later version published by the Free Software
.. Foundation, with no Invariant Sections, no Front-Cover Texts
.. and no Back-Cover Texts. A copy of the license is included at
.. Documentation/media/uapi/fdl-appendix.rst.
..
.. TODO: replace it to GFDL-1.1-or-later WITH no-invariant-sections

.. _KEY_ALLOC:

==============
KEY_ALLOC
==============

Name
----

KEY_ALLOC


Synopsis
--------
.. code-block:: c

     int ioctl(int fd, KEY_ALLOC, struct key_alloc *params)

Arguments
---------

``fd``
    File descriptor returned by open /dev/key device.

``params``

    Pointer to structure containing filter parameters.


Description
-----------

This ioctl call alloc the key index according to is_iv
parameters provided. the key index return from parameter .


Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
