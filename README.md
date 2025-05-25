# Satellite Rotator 🌌

![GitHub release](https://img.shields.io/github/release/juanca1218/satelliterotator.svg)

Welcome to the **Satellite Rotator** project! This repository provides a solution for using an ESP32-S3 microcontroller to emulate a GS-232 satellite rotator. Whether you are a ham radio enthusiast or a satellite tracking hobbyist, this project aims to enhance your experience with reliable and efficient satellite tracking.

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Configuration](#configuration)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)
- [Releases](#releases)

## Introduction

The Satellite Rotator project leverages the capabilities of the ESP32-S3 to create a powerful and flexible satellite rotator controller. By emulating the GS-232 protocol, this project allows you to control a pan-tilt mechanism for tracking satellites. This opens up a world of possibilities for satellite communication and tracking.

## Features

- **GS-232 Emulation**: Fully supports the GS-232 protocol for seamless communication with rotator hardware.
- **ESP32-S3 Integration**: Utilizes the ESP32-S3's processing power and connectivity features.
- **User-Friendly Interface**: Easy setup and configuration through a web interface.
- **Real-Time Tracking**: Accurate satellite tracking using Gpredict and other tracking software.
- **Pan-Tilt Control**: Control the rotation of your satellite dish or antenna smoothly and precisely.

## Hardware Requirements

To get started with the Satellite Rotator project, you will need the following hardware:

- **ESP32-S3 Development Board**: This microcontroller serves as the brain of the operation.
- **BNO055 Sensor**: For accurate orientation tracking.
- **Pan-Tilt Mechanism**: This can be a servo-based system for controlling the direction of your antenna.
- **Power Supply**: Ensure you have a suitable power source for your components.
- **Connecting Wires**: For making connections between the ESP32-S3 and other components.

## Software Requirements

You will need the following software to set up and run the Satellite Rotator:

- **Arduino IDE**: This is where you will write and upload your code to the ESP32-S3.
- **Gpredict**: A satellite tracking software that works well with the GS-232 protocol.
- **ESP32 Board Package**: Make sure to install the ESP32 board package in your Arduino IDE.

## Installation

1. **Clone the Repository**: Start by cloning this repository to your local machine.
   ```bash
   git clone https://github.com/juanca1218/satelliterotator.git
   ```

2. **Open in Arduino IDE**: Launch the Arduino IDE and open the cloned project.

3. **Install Libraries**: Make sure to install any required libraries for the project. You can find these in the `lib` folder.

4. **Configure Your Board**: In the Arduino IDE, select the ESP32-S3 board from the board manager.

5. **Upload the Code**: Connect your ESP32-S3 to your computer and upload the code.

6. **Connect the Hardware**: Wire up your pan-tilt mechanism and BNO055 sensor to the ESP32-S3.

## Usage

Once everything is set up, you can start using the Satellite Rotator:

1. **Connect to Wi-Fi**: The ESP32-S3 will need to connect to your local Wi-Fi network. Make sure to configure your Wi-Fi settings in the code.

2. **Access the Web Interface**: Open a web browser and enter the IP address of your ESP32-S3 to access the user interface.

3. **Control the Rotator**: Use the interface to control the pan-tilt mechanism and track satellites in real-time.

## Configuration

You may need to adjust some settings in the code for optimal performance:

- **Wi-Fi Credentials**: Update the SSID and password for your Wi-Fi network.
- **Servo Calibration**: If your pan-tilt mechanism requires calibration, adjust the servo settings in the code.
- **Tracking Settings**: Configure Gpredict to communicate with your ESP32-S3 for accurate satellite tracking.

## Contributing

We welcome contributions to the Satellite Rotator project! If you have ideas for improvements or new features, please feel free to fork the repository and submit a pull request. Here are some ways you can contribute:

- **Bug Reports**: If you find any bugs, please report them in the issues section.
- **Feature Requests**: Suggest new features or enhancements.
- **Documentation**: Help improve the documentation for better clarity.

## License

This project is licensed under the MIT License. Feel free to use and modify the code as you see fit, but please maintain the original license.

## Contact

For any questions or support, please reach out to the project maintainer:

- **Email**: [juanca1218@example.com](mailto:juanca1218@example.com)
- **GitHub**: [juanca1218](https://github.com/juanca1218)

## Releases

To download the latest release, visit the [Releases](https://github.com/juanca1218/satelliterotator/releases) section. You will find compiled binaries and installation instructions there.

---

Thank you for your interest in the Satellite Rotator project! We hope you enjoy building and using your satellite tracking system.