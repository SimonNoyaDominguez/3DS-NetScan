# 3DS-NetScan

> ⚠️ **Project made for fun.** It works, but it almost certainly has bugs. Use it only on your own network and responsibly. The author is not responsible for any misuse.

A network scanner that runs natively on a **Nintendo 3DS** with CFW. It scans hosts, ports, and grabs service banners using the console’s WiFi connection.

---

## What does it do?

* Discovers active hosts on the local network
* Scans TCP ports on specific hosts
* Grabs service banners (server version, software, etc.)
* Saves results to the SD card with timestamps
* Fully configurable directly from the console without editing the code

---

## Requirements

* Nintendo 3DS / 2DS / New 3DS with **CFW** (Luma3DS or similar)
* **devkitPro** with libctru for compilation
* Active WiFi connection on the console
* SD card with some free space

### Build

```bash
make
```

Copy the `.3dsx` file to the SD card and launch it from the Homebrew Launcher.

---

## Controls

| Button   | Action                                         |
| -------- | ---------------------------------------------- |
| `A`      | Full scan (discovery + port scan on all hosts) |
| `B`      | Discover active hosts only                     |
| `Y`      | Scan a specific IP (no banner grabbing)        |
| `X`      | Scan a specific IP (with banner grabbing)      |
| `R`      | Configure network base IP                      |
| `L`      | Configure IP scan range                        |
| `SELECT` | Enable/disable extra ports                     |
| `START`  | Cancel current scan / Exit                     |

In configuration menus:

* `UP/DOWN` — change value
* `LEFT/RIGHT` — move between fields
* `A` — save
* `B` — cancel

---

## Releases

### v1.0 - Full basic functionality

**Included files:**

* `scanner.3dsx` - Main executable
* `scanner.elf` - Intermediate file (optional)

**Installation:**

1. Copy `scanner.3dsx` to the `/3ds/` folder on your SD card
2. (Optional) Create the folder `/3ds/scanner/` and place it there
3. Open Homebrew Launcher and run it

**First use:**

* Default base IP is `192.168.1.X`
* Press `R` to change it if your network is different
* Press `B` to discover active hosts
* Press `A` for a full scan

**Note:** Results are saved to `sd:/3ds/scanner/results.txt`

---

## Features

### Host Discovery (`B`)

Scans the configured range and tests a list of probe ports to determine whether a host is responding. As soon as a host responds on any probe port, it is marked as active and the scanner moves on to the next one.

**Probe ports:** `22, 80, 443, 8443, 1883`
**Timeout:** 300ms per port (150ms for MQTT)

Equivalent nmap command:

```bash
nmap -sn --open -p 22,80,443,8443,1883 192.168.1.0/24
```

---

### Port Scanning (`Y` / `X`)

Tests all configured ports against a specific IP address. The banner version (`X`) also attempts to retrieve information from the listening service.

**Timeout per port:** 400ms

Equivalent nmap command:

```bash
# Without banner grabbing
nmap -p 21,22,23,25,53,80,... 192.168.1.1

# With banner grabbing
nmap -sV -p 21,22,23,25,53,80,... 192.168.1.1
```

---

### Full Scan (`A`)

First performs host discovery, then scans the ports of every discovered host. A final summary is displayed when finished.

Equivalent nmap command:

```bash
nmap -sV 192.168.1.0/24
```

---

### Banner Grabbing

For HTTP/HTTPS ports it sends a `HEAD /` request and extracts the `Server:` header.
For SSH, FTP, Telnet, RTSP, and others, it waits for the banner sent automatically by the service upon connection.

Equivalent nmap command:

```bash
nmap -sV --version-intensity 5 -p 80,443,22 192.168.1.1
```

---

### SD Card Logging

All results are automatically saved to:

```
/3ds/scanner/results.txt
```

Including timestamps, IPs, open ports, and captured banners. The file is appended between sessions.

Example output:

```txt
=== [2026-06-08 01:40:25] 192.168.1.1 ===
[OPEN] 22 SSH [SSH-2.0-dropbear_2014.63]
[OPEN] 53 DNS
[OPEN] 8443 HTTPS [ZTE web server 1.0 ZTE corp 2015.]
[OPEN] 52869 UPnP
----------------------------------------
```

---

### Configure Base IP (`R`)

Allows changing the first three octets of the network to scan. Default is `192.168.1.X`. Useful if your network uses `10.0.0.X`, `172.16.X.X`, etc.

### Configure Range (`L`)

Allows scanning only part of the subnet instead of the full `/24`. Available ranges are grouped in blocks of 50: 1-50, 51-100, 101-150, 151-200, 201-254.

### Extra Ports (`SELECT`)

Allows enabling additional ports not included in the base list. Ports can be toggled individually or all at once.

---

## Base Ports (always scanned)

| Port  | Service              |
| ----- | -------------------- |
| 21    | FTP                  |
| 22    | SSH                  |
| 23    | Telnet               |
| 25    | SMTP                 |
| 53    | DNS                  |
| 80    | HTTP                 |
| 139   | NetBIOS              |
| 443   | HTTPS                |
| 445   | SMB                  |
| 554   | RTSP                 |
| 631   | CUPS (printers)      |
| 993   | IMAPS                |
| 995   | POP3S                |
| 1883  | MQTT                 |
| 3306  | MySQL                |
| 3389  | RDP                  |
| 5000  | Synology DSM         |
| 5900  | VNC                  |
| 8008  | HTTP                 |
| 8009  | AJP13                |
| 8080  | HTTP                 |
| 8443  | HTTPS                |
| 9000  | CSListener           |
| 9080  | GLRPC                |
| 9100  | JetDirect (printers) |
| 52869 | UPnP                 |

---

## Extra Ports (toggle with SELECT)

### Remote Access

| Port | Service         |
| ---- | --------------- |
| 2222 | Alternative SSH |
| 3389 | RDP             |
| 5800 | VNC HTTP        |
| 5900 | VNC             |

### Web / HTTP

| Port | Service              |
| ---- | -------------------- |
| 8000 | Hikvision / HTTP     |
| 8081 | Alternative HTTP     |
| 8086 | QNAP                 |
| 8088 | HTTP                 |
| 8123 | Home Assistant       |
| 8161 | ActiveMQ             |
| 8444 | Alternative HTTPS    |
| 8554 | Alternative RTSP     |
| 8888 | Jupyter Notebook     |
| 9090 | Prometheus / Cockpit |

### Databases

| Port  | Service       |
| ----- | ------------- |
| 1433  | MSSQL         |
| 5432  | PostgreSQL    |
| 6379  | Redis         |
| 9200  | Elasticsearch |
| 27017 | MongoDB       |

### NAS / Storage

| Port  | Service          |
| ----- | ---------------- |
| 139   | NetBIOS          |
| 2049  | NFS              |
| 5001  | Synology HTTPS   |
| 9091  | Transmission     |
| 51413 | Transmission P2P |

### IP Cameras

| Port  | Service                             |
| ----- | ----------------------------------- |
| 554   | Standard RTSP                       |
| 8554  | Alternative RTSP                    |
| 8000  | Hikvision                           |
| 18080 | TP-Link cameras                     |
| 34567 | HiSilicon (generic Chinese cameras) |
| 34599 | Alternative HiSilicon               |
| 37777 | Dahua                               |

### IoT / Smarthome

| Port  | Service        |
| ----- | -------------- |
| 1883  | MQTT           |
| 4567  | Shelly         |
| 6668  | Tuya           |
| 8123  | Home Assistant |
| 8883  | MQTT with TLS  |
| 55443 | Xiaomi         |

### Multimedia

| Port | Service                |
| ---- | ---------------------- |
| 548  | AFP (Mac file sharing) |
| 1400 | Sonos                  |
| 3689 | DAAP / iTunes          |
| 7000 | AirPlay                |
| 8200 | DLNA / Serviio         |

### Messaging / Queues

| Port  | Service  |
| ----- | -------- |
| 9092  | Kafka    |
| 15672 | RabbitMQ |

### Email

| Port | Service |
| ---- | ------- |
| 110  | POP3    |
| 143  | IMAP    |
| 465  | SMTPS   |
| 587  | SMTP    |

### Printers

| Port | Service   |
| ---- | --------- |
| 515  | LPD       |
| 631  | CUPS      |
| 9100 | JetDirect |

---

## Device Type Detection by Open Ports

If you see these port combinations on a host, it is probably:

| Open Ports          | Likely Device                 |
| ------------------- | ----------------------------- |
| `554` + `37777`     | Dahua camera                  |
| `554` + `34567`     | Chinese IP camera (HiSilicon) |
| `554` + `8000`      | Hikvision camera              |
| `5000` + `5001`     | Synology NAS                  |
| `8086`              | QNAP NAS                      |
| `22` + `80` + `443` | Linux server                  |
| `1883`              | MQTT broker / home automation |
| `8123`              | Home Assistant                |
| `3389`              | Windows with Remote Desktop   |
| `9100`              | Network printer               |
| `8008` + `8009`     | Chromecast / Google device    |
| `53` + `80` + `443` | Router                        |

---

## Known Issues

* **Does not detect hosts without open TCP ports** — devices in standby or behind strict firewalls will not appear. Without ICMP/ping there is no reliable way to detect them using TCP scanning only.
* **False negatives on slow ports** — services taking longer than 400ms to respond may appear closed. This happens especially with overloaded databases.
* **Banner grabbing may block** — some services do not send banners automatically, causing `recv()` to wait until timeout. Mostly affects proprietary or badly configured services.
* **Socket limit** — the 3DS SDK has a limited number of simultaneous sockets. If sockets are not closed properly (crash, forced cancellation), the app may run out of sockets until restarted.
* **EINPROGRESS = 119** — on 3DS this errno value differs from standard POSIX (115). The code accounts for it, but different libctru versions may vary.
* **No UDP support** — UDP scanning on 3DS is practically unworkable. Without reliable ICMP port unreachable handling and controllable timeouts, results are inconsistent.
* **Blocking HTTP banners may slow scans** — if an HTTP server takes too long to respond to `HEAD /`, the scan pauses until timeout for that port.
* **The screen may overflow** — if many ports are open, results may scroll off-screen. Results are always saved to the SD card log file.

---

## Technical Limitations

* TCP only — no UDP support
* No OS detection (would require raw sockets, unavailable on 3DS)
* No SYN scan (same reason)
* Speed limited by the 3DS WiFi hardware and system scheduler
* No real multithreading — everything is sequential using non-blocking sockets

---

## Built With

* **libctru** — standard 3DS homebrew library
* **devkitARM** — compilation toolchain
* Standard POSIX sockets via `libctru/network`
* No external dependencies

---

## Legal Disclaimer

This project is intended **only for use on networks you own** or where you have explicit permission to perform testing. Scanning networks without authorization is illegal in most countries. The author is not responsible for how this tool is used.

---

*Made for fun with a 2011 Old 3DS and way too much free time.*
