# ESP32 Greenhouse IoT Controller

An ESP32-based controller for monitoring greenhouse conditions, controlling relays, storing configuration locally, and exchanging data with a remote backend over HTTPS.

> This repository is a portfolio version of the project. Private backend URLs, Wi-Fi credentials, and the original server CA certificate are intentionally not included.

## Features

- ESP32-based embedded controller
- Wi-Fi connectivity
- HTTPS communication with a backend API
- JSON-based data exchange
- SHT20 temperature and humidity measurement
- BH1750 ambient light measurement
- Ultrasonic water-level measurement
- DS1307 real-time clock
- NTP time synchronization with RTC fallback
- EEPROM-based configuration storage
- Scheduled pump and lighting control
- Threshold-based environmental error detection
- Remote Wi-Fi configuration update

## Hardware / Software

| Category | Technology |
| --- | --- |
| MCU | ESP32 |
| Framework | Arduino |
| Language | C++ |
| Connectivity | Wi-Fi / HTTPS |
| Sensor bus | I2C |
| Sensors | SHT20, BH1750, ultrasonic |
| RTC | DS1307 |
| Local storage | EEPROM emulation |

## Project Structure

```text
esp32-greenhouse-controller/
├── src/
│   └── main.cpp
├── platformio.ini
├── README.md
├── .gitignore
└── LICENSE
```

All application source code is intentionally kept in a single `main.cpp` file to make the project easy to inspect and understand.

## Build

This project is structured for PlatformIO.

1. Install PlatformIO.
2. Edit the configuration constants at the top of `src/main.cpp`.
3. Fill in the required configuration values.
4. Select the appropriate ESP32 board in `platformio.ini` if your hardware differs from the example configuration.
5. Build and upload the firmware.

## What This Project Demonstrates

- Embedded C++ development on ESP32
- I2C peripheral and sensor integration
- Wi-Fi connectivity
- HTTPS client communication
- JSON serialization and parsing
- Persistent configuration storage
- RTC and network time synchronization
- Periodic task scheduling using `millis()`
- Relay control logic
- Environmental threshold monitoring

## Portfolio Note

This repository focuses on the firmware side of the greenhouse controller. Hardware schematics, PCB files, and backend implementation can be added separately if they are appropriate for public release.

## License

MIT License. See `LICENSE` for details.
