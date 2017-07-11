Description:
============

This program permits to mount a db like a file system, in read only. 


FEATURES:
=========

This program permits to mount a relational db like a file system, in read only. So it's possible to access that db using a shell (i.e. the ls command to list the tables, cat to list the data int the tables and so on).
At the moment PostgreSQL is supported.

Prerequisites:
==============

- C++11 compiler;
- libfuse  (FUSE)
- libpq    (PostgreSQL)

Tested on:

- Debian 7 (Wheezy)

Installation:
=============

- launch the configure script:
  ./configure
- Compile the program:
  make
- Install the program and the man page (optional):
  sudo make install

Instructions:
=============

See the man page included in the release.

