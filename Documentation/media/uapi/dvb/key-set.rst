.. Permission is granted to copy, distribute and/or modify this
.. document under the terms of the GNU Free Documentation License,
.. Version 1.1 or any later version published by the Free Software
.. Foundation, with no Invariant Sections, no Front-Cover Texts
.. and no Back-Cover Texts. A copy of the license is included at
.. Documentation/media/uapi/fdl-appendix.rst.
..
.. TODO: replace it to GFDL-1.1-or-later WITH no-invariant-sections

.. _KEY_SET:

==============
KEY_SET
==============

Name
----

KEY_SET


Synopsis
--------
.. code-block:: c

    int ioctl( int fd, KEY_SET, struct key_descr *key)

Arguments
---------

``fd``
    File descriptor returned by open /dev/key.

``key``

    Pointer to structure containing key parameters.


Description
-----------

This ioctl call update the key contents according to the key index.
if update the odd/even key, you can call this ioctl to update key to
key_table, but this just for ree mode, not ta mode.


Return Value
------------

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
