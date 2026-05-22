# ImageDisplay

Custom firmware for the GeekMagic SmallTV-Ultra that fetches a remote JPEG and displays it full-screen on the 240x240 TFT.

## What it does

- Fetches a JPEG image from a configurable URL and renders it full-screen
- Refreshes at a configurable interval (30–3600 seconds, default 5 minutes)
- 2 themes: Dark, Light (affects setup/status screens)
- Adjustable brightness
- Web-based device manager (image URL, refresh interval, theme, brightness, OTA)
- WiFiManager captive portal for initial WiFi setup

Tip : use for example this project to generate a useful dashboard : https://github.com/sharkoz/claude-meter

## Hardware

- GeekMagic SmallTV-Ultra (ESP8266 + ST7789 240x240 IPS)

## Setup

### 1. Flash the firmware

Install [PlatformIO](https://platformio.org/install/cli).

**Option A: USB** (if you have serial access):
```
pio run -e esp8266 -t upload
```

**Option B: OTA from stock firmware** (no USB needed):

The full firmware (~566KB) exceeds the stock OTA partition (~520KB). Use the bootstrap firmware as a stepping stone:

1. Build the bootstrap: `pio run -e bootstrap`
2. Connect the device to WiFi using the stock firmware
3. Find its IP (check your router's DHCP leases or serial output)
4. Open `http://<device-ip>/update` and upload `.pio/build/bootstrap/firmware.bin` (~334KB)
5. The device reboots into bootstrap mode — connect to the **ImageDisplay** WiFi AP and configure WiFi
6. Open `http://<device-ip>` and upload the full firmware: `.pio/build/esp8266/firmware.bin`

After the first flash, subsequent updates go through the device manager at port 80 with no size limit.

### 2. Configure the device

On first boot (or when no image URL is configured), the display shows setup instructions:

1. Connect your phone/laptop to the **ImageDisplay** WiFi network
2. A captive portal opens automatically (or browse to **192.168.4.1**)
3. Select your WiFi network, enter the password, and paste an image URL
4. Click Save — the device connects and fetches the image

The image should be a JPEG, ideally 240×240 pixels.

## Device Manager

Open `http://<device-ip>` in a browser:

- **Brightness** slider (real-time, persisted)
- **Theme** selector (Dark / Light — affects setup/status screens)
- **Image URL** — the JPEG to display
- **Refresh interval** — how often to re-fetch the image (30–3600 seconds)
- **WiFi**: view SSID/IP, reset WiFi credentials
- **Firmware update**: upload `.bin` file
- **File manager**: upload/delete LittleFS files

## Development

### Project structure

```
├── platformio.ini          # Build configuration
├── src/
│   ├── main.cpp            # Entry point, setup/loop
│   ├── display.h/cpp       # TFT rendering
│   ├── image.h/cpp         # HTTPS fetch + JPEG decode
│   ├── config.h/cpp        # WiFi + config portal
│   ├── ota.h/cpp           # Web server + OTA + settings
│   └── bootstrap.cpp       # Minimal first-flash firmware
```

### Build commands

```bash
pio run -e esp8266              # build full firmware
pio run -e esp8266 -t upload    # flash via USB
pio run -e bootstrap            # build OTA bootstrap
pio device monitor              # serial monitor (115200 baud)
```

## Architecture

```
Remote server (any HTTPS host)
    |  JPEG over HTTPS (setInsecure, follows redirects)
    v
ESP8266 — fetches every N seconds
    |  TJpg_Decoder → pushImage
    v
ST7789 240x240 display
```

## Disclaimer

This is unofficial third-party firmware. Flashing it **replaces the stock firmware** and voids any warranty. I am not responsible for bricked devices, data loss, or any other damage. Flash at your own risk. USB serial flashing can usually recover the device.

## License

MIT
