f-MAC
=====

Implementation of f-MAC algorithm, based on `“f-MAC: A Deterministic Media
Access Control Protocol Without Time Synchronization”`__ by Utz Roedig, Andre
Barroso and Cormac J. Sreenan for Infineon’s TDA5340 TRX and XMC4500/XMC1100
microcontroller.

__ https://doi.org/10.1007/11669463_21

Usage
-----

Install ARM’s GCC_ for Linux and clone dependencies (see below). The release
2017q1 is known to work. Edit the Makefile (BOARD=) depending on your target,
then run `make`, which creates a binary in bin/.

While OpenOCD should work in theory, Segger’s JLink tools are usually faster
(albeit less stable). For the XMC4500 Relax Kit run::

    JLinkGDBServer -select USB=$(USBID) -x foo.jlink -device XMC4500-1024 -if swd -port 3333 -localhostonly 1

The XMC1100 (csmTDA) you can use ``-device XMC1100-32`` instead. Then ``make
gdb`` starts the GNU debugger. Then ``load``, ``monitor reset`` and
``continue`` to flash and run the program.

Remote control
^^^^^^^^^^^^^^

Unless several ``DEBUG_*`` options in config.h are set, this code will do
nothing, unless configured via SPI/UART.

READBUF
    Master sends command 01h. Slave responds with one packet from the FIFO.
WRITEBUF
    Master sends command 02h, followed by packet data. No response.
READREG
    Master sends command 03h and a 8 bit register number (see below). Slave
    responds with 32 bit register value.
WRITEREG
    Master sends command 04h, a 8 bit register number and a 32 bit register
    value. No response.

Available registers:

RXPENDING: 02h
    Number of received packets in FIFO (max 2, see function initFifos)
TXPENDING: 03h
    Number of packets waiting to be transmitted (max 2)
CONFIG: 05h
    From LSB to MSB, each one byte: Station ID, number of stations, payload
    size (max 32 bytes)

SPI
***

SPI uses the configuration parameters CLK_PH=1 and CLK_POL=0. For the correct
pinout see the top of spiclient.c. Since response times cannot be guaranteed,
the SPI protocol uses to steps. First, hold SS low, then send a request and
pull SS high again. After pulling SS low again the code will respond with
undefined words until a sync word ``AAh`` is received. The actual response
follows. Write requests have no response.

UART
****

UART uses 9600 baud, 8 data bits, 1 stop bit (see spiclientInit) and the same
MOSI/MISO pins for communication. At the end of every message (request or
response) a break symbol (hold down data line for more cycles than wordlength)
must be sent to terminate the message.

Project structure
-----------------

These files are important, everything else is just boilerplate.

fmac.c
    The actual MAC implementation
config.h
    A few compile-time configuration options
spiclient.c
    SPI and UART slave implementation (disregard the name)

Dependencies are added to ``src/`` as git submodules. These include:

bitbite_
    Bitstream helper functions
dottedline_
    8b10b encoding
prettylewis_
    Hardware abstraction (HAL) for the TDA5340
rtt_
    SEGGER’s RTT library
xmclib_
    Infineon’s XMClib

.. _GCC: https://developer.arm.com/open-source/gnu-toolchain/gnu-rm
.. _bitbite: https://github.com/PromyLOPh/libbitbite
.. _dottedline: https://github.com/PromyLOPh/libdottedline
.. _prettylewis: https://github.com/PromyLOPh/libprettylewis
.. _rtt: https://github.com/PromyLOPh/rtt
.. _xmclib: https://github.com/PromyLOPh/xmclib

