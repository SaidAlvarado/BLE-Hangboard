# BLE Hangboard Project

This project aims to design a portable climbing hangboard with BLE capabilities.
The hangboard works as a digital portable scale. It reports trhough BLE how many kg of force you're applying while you hang from it.


## Technologies.

This project is built using:
- nRF52833 microcontroller
- RIOT-OS as a RTOS.
- Nimble as the BLE stack.

## Getting Started

Follow these instructions to flash a test application to an nRF52840dk. This test aplplication acts as a BLE device and advertises a Weight measurement Service that sends random values.

They assume you're working in Linux.

1. Make sure you have these tools installed TODO

2. Clone the repository (if you haven't already!), and initialize the RIOT-OS submodule folder.  Then, install several dependencies.
```bash
git clone https://github.com/SaidAlvarado/Udacity-DeepRL-Project_2_Continuous_Control.git
cd BLE-hangboard
git submodule init
```

3. Connect an nRF52840dk to an USB port in your computer.

4. Move into the test example folder. and flash the application
```bash
cd examples/5-ble_weigh_scale
make flash
make term
```

This starts a terminal that what the BLE device is doing.

5. Use nRF-connect App for android to interact with it TODO: Add pictures


