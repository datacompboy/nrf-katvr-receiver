# nRF-based KatVR C2/C2+ Receiver

This app aims to implement bluetooth receiver compatible with KatVR's C2/C2+ treadmill sensors.

## Features

- KatVR Sensors BT protocol
  - Connect to multiple devices (static list: Direction, Left Feet, Right Feet)
  - Capture notifications sent by devices and know who sent them.

## Tested on

- (main) Seeed Studio XIAO nRF52840
- nRF52840 Dongle

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
  - Parse BT notification from direction sensor
  - Parse BT notification from feet sensors
  - Connect & receive events from seat
- KatVR Gateway USB protocol
  - Devices
    - Expose "Kat Receiver" HID device
    - Expose "Kat Seat Receiver" HID device
  - Protocol
    - Enable / disable stream command
    - Sensor position notification
    - "Pair" with sensors
    - Fake LED / Vibration commands
- BT Peripheral mode
  - Export sensors data over BLE in standard way
  - Export connected sensors MACs over BLE in R/W mode
- Moonshots
  - Support on-chip 6-axis IMU (e.g. from Seeed Studio XIAO nRF52840 Sense) for "head" direction
  - on-chip gateway, mixing IMU & sensors and export data over HID
