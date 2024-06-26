# nRF-based KatVR C2/C2+ Receiver -- 133 Hz version

This app aims to implement bluetooth receiver compatible with KatVR's C2/C2+ treadmill sensors.

Project address: https://github.com/datacompboy/nrf-katvr-receiver/

Read article about it at [Medium] or [LinkedIn] or [Habr].

[Habr]: https://habr.com/ru/articles/802125/
[Medium]: https://medium.com/@datacompboy/katwalk-c2-p-3-cutting-the-wire-987753f49da8?source=friends_link&sk=a4e0e2d3f71be681bc206b1d9a3d307a
[LinkedIn]: https://www.linkedin.com/pulse/katwalk-c2-p3-cutting-wire-anton-fedorov-4abae/

## Tested on

- Seeed Studio XIAO nRF52840

## READ ME!

This is a version of the receiver that is designed to work at 133Hz with the sensors.

**While this leads to lower latencies, it requires to use a fixed firmware for all sensors you use.**

## Installation on Seeed Stdio XIAO nRF52840 dongle

- Download binary release package
- Unpack
- Exit from the KAT Gateway
- Disconnect your KAT Walk USB cable
- Attach your Seeed Studio XIAO nRF52840 dongle to usb port and double-press the reset button (left from usb port),
  so that new drive explorer window pops up.
- Double-click "`install.cmd`" script to install firmware and write pairing with your sensors.
- The get stable connection, please update sensors firmware!

To update a sensor firmware:

- Take the sensor out of a shoe or a backplate
- Connect it to the PC
- Press with a toothpick button inside of a "Firmware update" hole
- Click with another toothpick the "Reset" button
- Release the "Firmware update" button
- From "Sensors" folder run "update-feet-firmware.cmd" or "update-direction-firmware.cmd"

If you want to restore sensor firmware back to stock: do the same, but run "restore-*" script.

You need an updated firmware to enjoy lower latencies with this receiver, but the use of fixed firmware with the official
receiver will lead to very-very fast movement in games... I'm working on a better solution.

## Features

- KatVR Sensors BT protocol
  - Connect to multiple devices (static list: Direction, Left Feet, Right Feet)
  - Capture notifications sent by devices and know who sent them.
  - Parse BT notification from feet sensors
  - Parse BT notification from direction sensor
  - Achieved 133Hz/sensor frequency!
    - Default receiver has ~80Hz/sensor
    - Single sensor has 250Hz limit;
    - BLE4 has limit of 400Hz for all connections with single central.
- KatVR Gateway USB protocol
  - Devices
    - Expose "Kat Receiver" HID device
  - Protocol
    - Enable / disable stream command
    - Sensor position notification
    - Fake LED / Vibration commands
    - Pair with sensors
    - Allow to set SerialNumber for cloning (use device-uniq serial otherwise)
- KatVr Gateway Compatibility
  - Device can be used as separate receiver, by following standard pairing procedure
  - Device can clone settings from existing KAT receiver using provided `clone-kat-device.ps1` powershell script.

## How to build

- Prerequisites
  - Install nRF Connect for Desktop
    - Install Toolchain Manager
      - Install the nRF SDK (as of today, v2.5.2)
      - Install the Serial Terminal (convenient to peek at live-over-usbcom debug stream)
    - If you use nRF52840 Dongle -- also install "Programmer"
  - Install VS Code
    - Install "nRF Connect vor VS Code Extension Pack" thru extensions marketplace
- Checkout the project from github, open in VS Code
- Create build config:
  - Go to `nRF Connect` tab
  - Go to `Applications` pane
  - Click `Create new build configuration`
  - Choose your dongle type from dropdown, the rest can be left as is
    - If you wish to enable debuggign (Enables USB console and logging), add an extra CMake option:

      ```shell
      -DOVERLAY_CONFIG=prj_debug.conf
      ```

  - Now use "Build" action from `Actions` pane under the chosen build.
- Flashing to the dongle:
  - First, find the output directory
    - under the `NRF-KATVR-RECEIVER` pane there are `Output files` section
    - right-click on any file here, and click "Copy path to file"
    - open system explorer, paste the path into address bar, remove filename, enter
  - If you use XIAO nRF52840:
    - Double-click the tiny "reset" button left from USB-C port
    - The new disk will appear in the system
    - From the output files folder (see above), copy `zephyr.uf2` file into the new disk
    - Once copy finishes, disk disappears and dongle reboot into app
  - If you use nRF52840 Dongle:
    - Dongle has an easy-accessible white button (not the right one) and right near it another
          one, that can be pressed along the PCB in horizontal motion -- that's the flashing button. Click it.
    - Open Programmer from nRF Connect Desktop
    - Select dongle in top-left corner (it's handy to keep auto-connect for multiple iterations, it reconnects if device is in flasher mode)
    - "Add file" to add from the Output files the "zephyr.hex".
      - If you change the code and want to re-flash, you can just click "Reload files" to reload latest copy from disk
    - Press "Write" to actually flash the device.
    - Once it finishes, device will reboot into device mode
- Connecting to debug console
  - Run the "Serial Terminal" from the nRF Desktop
  - Select "nRF KAT-VR Receiver" from the left-top corner of terminal, (hint: "autoconnect")
  - Enjoy the logs!

## TODO

- KatVR Sensors BT protocol
  - Connect & receive events from seat
- KatVR Gateway USB protocol
  - Devices
    - Expose "Kat Seat Receiver" HID device
    - Let to choose mimic C2/C2+ or C2Core receivers
  - USB Host / BT Hid
    - Create child device to control vibration/LED wirelessy
- BT Peripheral mode (doesn't seems like worth it)
  - Export connected sensors MACs over BLE in R/W mode
  - Export sensors data over BLE in standard way
    - unlikely with standard sensors.
    - it could be possible to reexport over BLE, but in 1 packet that contains all 3 sensors data without gaps, increased MTU and packets frequency decreased down to ~80hz...
    - or should be possible to update firmware to enable 2Mbit PHY, then it *may* fit. more digging needed.
- Moonshots
  - Support on-chip 6-axis IMU (e.g. from Seeed Studio XIAO nRF52840 Sense) for "head" direction
  - on-chip gateway, mixing IMU & sensors and export data over HID
