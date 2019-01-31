Rainbow project
===============

The idea is to create a NES mapper capable of internet communication. The mapper would embed an ESP8266 to do so. The mapper allows the NES software to read and write on the ESP serial by reading or writing $5000. It is also possible to read bit 7 of $5001 to know if the ESP has something to send on the serial port.

Official Raibow documentation is available here: https://hackmd.io/s/H1vBJKpX4

This repository
===============

It is an early attempt to provide some emulator support for this new mapper.

The big part is a patch for FCEUX to be able to emulate the Rainbow mapper. This patched version recognize mapper 3840 as the Rainbow mapper.

Also there is the sample ROM. It is a little nes ROM made to test the mapper, it allows to send simple ascii messages and shows received ascii from the server.

How to test
===========

First, compile FCEUX:

 $ scons

Then build the sample ROM:

 $ cd rainbow-related-stuff/sample-rom/ && xa nine.asm -C -o game.nes && cd ../..

Start a simple server (on another terminal):

 $ node rainbow-related-stuff/sample-server/sample-server-ws.js

Start the sample ROM with patched FCEUX:

 $ bin/fceux rainbow-related-stuff/sample-rom/game.nes

The ROM show wifi status and connection to server status at the top of the screen. Pressing a button on the first controller sends an acii message to the server. On the server's terminal if you type a message, it will be shown in the emulator.

How to understand
=================

The interesting ROM code is in rainbow-related-stuff/sample-rom/game/sample_irq.asm and rainbow-related-stuff/sample-rom/game/sample_noirq.asm. Both do the same: showing server state at screen's top, showing ascii messages from server and sending a message when pressing any button. sample_irq.asm does so by handling ESP messages asynchronously in the IRQ handler. sample_noirq.asm masks IRQs and polls the ESP when it expects a message.

The mapper code for FCEUX is in src/boards/rainbow.cpp It is not commented enought, sorry. There is an abstract class EspFirmware which expose the possibility to send and receive bytes (emulating the UART TX and RX) and exposes two gpios (like the classical ESP-01 board). Derived from this abstract class, the BrokeStudioFirmware class handle the networking and simulate features expected from the firmware. Code not in these classes is mapper code, for now it is a copy/past of the NROM code with extrat handling for port $5000 and $5001, communicating with the EspFirmware.
