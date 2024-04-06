# KatWalk C2/C2+/C2Core Sensors firmware patches

This directory contains patches for KatWalk sensors, with fixes to a race condition and removal of debugging code inside.

You don't need them if you use standard KatWalk C2 software and receiver. The Kat's Gateway can update the latest firmware itself.

## Description

The description of the firmware fixes described there:

* KatWalk C2 Saga, EP5: "overclocking and bugfixing" on [Habr], [Medium] and [LinkedIn].

[Habr]: https://habr.com/ru/articles/805747/
[Medium]: https://medium.com/@datacompboy/katwalk-c2-p-5-overclocking-and-bugfixing-0ff1fd853e49?source=friends_link&sk=4f44bf29291c8c3ad1b2522c5fd2d3a9
[LinkedIn]: https://www.linkedin.com/pulse/katwalk-c2-p5-overclocking-bugfixing-how-use-ghidra-analyse-fedorov-5nome/

## How to use

To update a sensor firmware:

* Take the sensor out of a shoe or a backplate
* Connect it to the PC
* [*] Press with a toothpick button inside of a "Firmware update" hole
* [*] Click with another toothpick the "Reset" button
* [*] Release the "Firmware update" button
* Run "update-feet-firmware.cmd" or "update-direction-firmware.cmd"

If you want to restore sensor firmware to original: do the same, but run "restore-*" script.

[*] Note: some sensor versions may not need to play with toothpicks and just connecting to PC and run script is sufficient.
You can try to do so, but if it does not work -- you need two toothpicks :)
