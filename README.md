# Air Control USB Device Reader

This is a simple program to read temperature and CO2 concentration from an Air Control USB device. The program was developed with the assistance of ChatGPT by making it analyze lnicola's rust code. The URL of the original rust project is [https://github.com/lnicola/co2mon](https://github.com/lnicola/co2mon).

## Compatibility

This program has been tested with the [TFA-Dostmann AIRCO2NTROL MINI](https://www.tfa-dostmann.de/en/produkt/co2-monitor-airco2ntrol-mini/) sensor (USB VID: 0x04d9, USB PID: 0xa052).

## Example Output

Here's an example of the JSON output format from the program:

```json
{
  "timestamp": "2023-08-11 16:51:18",
  "temperature": 21.48,
  "co2": 727
}
{
  "timestamp": "2023-08-11 16:51:23",
  "temperature": 21.48,
  "co2": 726
}
```

## Example Output with --all

Here's an example of the JSON output format when using the `--all` command-line switch:

```json
{
  "timestamp": "2023-08-11 16:51:18",
  "relative_humidity": 0,
  "temperature_kelvin": 4736,
  "temperature_fahrenheit": 0,
  "co2_concentration": 916,
  "co2_concentration_uncalibrated(unsure)": 897,
  "value_C": 3248,
  "value_O": 7064,
  "value_R": 11801,
  "value_V": 11828,
  "value_W": 7863,
  "value_m": 3971,
  "value_n": 19566,
  "value_q": 843
}
```

## Prerequisites

- GCC (GNU Compiler Collection)
- HIDAPI Library

### Ubuntu and Debian

You can install the HIDAPI library and GCC on Debian / Ubuntu using the following commands:

```bash
sudo apt-get update
sudo apt-get install libhidapi-dev gcc
```
### Arch Linux

To install the HIDAPI library and GCC on Arch Linux, you can use the following command:

```bash
sudo pacman -S hidapi gcc
```

## Installation Instructions

- Clone this repository:

```bash
git clone https://github.com/tpoechtrager/co2mon_c.git
cd co2mon_c
```

- Compile the program:

```bash
make
```

## Getting Started

Run the compiled executable:

```bash
./co2mon
```
 
The program will read data from the Air Control USB device and display temperature and CO2 concentration in JSON format.

Press Ctrl+C to stop the program.

## Available Command-Line Switches

The available command-line switches are:

```bash
    --help: Display help and usage information.
    --key <key>: Set the encryption key (16 characters in hexadecimal format). Usually not needed.
    --all: Print all available readings as JSON.
```

## Setting Permissions

On Linux, you need to be able to access the USB HID device. For that, you can save the following udev rule to /etc/udev/rules.d/60-co2mon.rules:

```bash
ACTION=="add|change", SUBSYSTEMS=="usb", ATTRS{idVendor}=="04d9", ATTRS{idProduct}=="a052", MODE:="0666"
```
Then reload the rules and trigger them:

```bash
udevadm control --reload
udevadm trigger
```
  
Note that the udev rule above makes the device accessible to every local user.

## Quick Start

Run the program:

```bash
./co2mon
```
  
##  Protocol Details

The USB HID protocol is not documented, but is a superset of [this one][co2meters] and was [reverse-engineered][had] [before][revspace].

The implementation was inspired by [this one][co2mon].

[co2meters]: https://co2meters.com/Documentation/Other/AN_RAD_0301_USB_Communications_Revised8.pdf
[co2mon]: https://github.com/dmage/co2mon/
[had]: https://hackaday.io/project/5301/
[revspace]: https://revspace.nl/CO2MeterHacking
  
##  License

This project is licensed under either of

- Apache License, Version 2.0, ([LICENSE-APACHE](https://github.com/lnicola/co2mon/blob/master/LICENSE-APACHE) or [http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0))
- MIT license ([LICENSE-MIT](https://github.com/lnicola/co2mon/blob/master/LICENSE-MIT) or [http://opensource.org/licenses/MIT](http://opensource.org/licenses/MIT))

at your option.
