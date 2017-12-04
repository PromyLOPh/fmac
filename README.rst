f-MAC
=====

Implementation of f-MAC algorithm, based on “f-MAC: A Deterministic Media
Access Control Protocol Without Time Synchronization” by Utz Roedig, Andre
Barroso and Cormac J. Sreenan.

Usage
-----

Install ARM’s GCC_ for Linux. The release 2017q1 is known to work. Edit the
Makefile (BOARD=) depending on your target, then run `make`, which creates a
binary in bin/. `make gdb` starts the GNU debugger.

Project structure
-----------------

These files are important, everything else is just boilerplate.

fmac.c
    The actual MAC implementation
config.h
    A few compile-time configuration options
spiclient.c
    SPI slave implementation

Dependencies are added to `src/` as git submodules. These include:

bitbite
    Bitstream helper functions
dottedline
    8b10b encoding
prettylewis
    Hardware abstraction (HAL) for the TDA5340
rtt
    SEGGER’s RTT library
xmclib
    Infineon’s XMClib

.. _GCC: https://developer.arm.com/open-source/gnu-toolchain/gnu-rm

