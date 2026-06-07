# 3DS-NetScan

> ⚠️ **Proyecto hecho por las risas.** Funciona, pero tiene fallos con toda seguridad. Úsalo solo en tu propia red y con responsabilidad. El autor no se hace responsable de ningún uso indebido.

Un escáner de red que corre nativamente en una **Nintendo 3DS** con CFW. Escanea hosts, puertos y captura banners de servicios usando la WiFi de la consola. 

---

## ¿Qué hace?

- Descubre hosts activos en la red local
- Escanea puertos TCP de hosts específicos
- Captura banners de servicios (versión del servidor, software, etc.)
- Guarda los resultados en la SD con timestamp
- Todo configurable desde la propia consola sin tocar el código

---

## Requisitos

- Nintendo 3DS / 2DS / New 3DS con **CFW** (Luma3DS o similar)
- **devkitPro** con libctru para compilar
- Conexión WiFi activa en la consola
- Tarjeta SD con algo de espacio libre

### Compilar

```bash
make
```

Copia el `.3dsx` a la SD y ejecútalo desde el Homebrew Launcher.

---

## Controles

| Botón | Acción |
|-------|--------|
| `A` | Escaneo completo (discovery + puertos de todos los hosts) |
| `B` | Solo buscar hosts activos |
| `Y` | Escanear IP específica (sin banner) |
| `X` | Escanear IP específica (con banner) |
| `R` | Configurar IP base de la red |
| `L` | Configurar rango de IPs a escanear |
| `SELECT` | Activar/desactivar puertos extra |
| `START` | Cancelar escaneo en curso / Salir |

En los menús de configuración:
- `UP/DOWN` — cambiar valor
- `LEFT/RIGHT` — moverse entre campos
- `A` — guardar
- `B` — cancelar

---

## Funciones

### Descubrimiento de hosts (`B`)
Recorre el rango configurado y prueba una lista de puertos sonda para detectar si hay alguien respondiendo. En cuanto un host responde a cualquier puerto sonda, se marca como activo y se pasa al siguiente.

**Puertos sonda:** `22, 80, 443, 8443, 1883`  
**Timeout:** 300ms por puerto (150ms para MQTT)

Equivalente en nmap:
```bash
nmap -sn --open -p 22,80,443,8443,1883 192.168.1.0/24
```

---

### Escaneo de puertos (`Y` / `X`)
Prueba todos los puertos configurados contra una IP concreta. La versión con banner (`X`) además intenta obtener información del servicio que está escuchando.

**Timeout por puerto:** 400ms

Equivalente en nmap:
```bash
# Sin banner
nmap -p 21,22,23,25,53,80,... 192.168.1.1

# Con banner
nmap -sV -p 21,22,23,25,53,80,... 192.168.1.1
```

---

### Escaneo completo (`A`)
Primero hace el discovery de hosts y luego escanea los puertos de cada host encontrado. Al terminar muestra un resumen final.

Equivalente en nmap:
```bash
nmap -sV 192.168.1.0/24
```

---

### Banner grabbing
Para puertos HTTP/HTTPS manda una petición `HEAD /` y extrae la cabecera `Server:`.  
Para SSH, FTP, Telnet, RTSP y otros espera el banner que el servicio manda al conectar.

Equivalente en nmap:
```bash
nmap -sV --version-intensity 5 -p 80,443,22 192.168.1.1
```

---

### Guardado en SD
Todos los resultados se guardan automáticamente en:
```
/3ds/scanner/resultados.txt
```
Con timestamp, IP, puertos abiertos y banners capturados. El archivo se acumula entre sesiones (modo append).

Ejemplo de salida:
```
=== [2026-06-08 01:40:25] 192.168.1.1 ===
[OPEN] 22 SSH [SSH-2.0-dropbear_2014.63]
[OPEN] 53 DNS
[OPEN] 8443 HTTPS [ZTE web server 1.0 ZTE corp 2015.]
[OPEN] 52869 UPnP
----------------------------------------
```

---

### Configurar IP base (`R`)
Permite cambiar los tres primeros octetos de la red a escanear. Por defecto `192.168.1.X`. Útil si tu red es `10.0.0.X`, `172.16.X.X`, etc.

### Configurar rango (`L`)
Permite escanear solo una parte del rango en lugar del /24 completo. Opciones en bloques de 50: 1-50, 51-100, 101-150, 151-200, 201-254.

### Puertos extra (`SELECT`)
Permite activar puertos adicionales que no están en la lista base. Se pueden marcar/desmarcar individualmente o todos a la vez.

---

## Puertos base (siempre escaneados)

| Puerto | Servicio |
|--------|---------|
| 21 | FTP |
| 22 | SSH |
| 23 | Telnet |
| 25 | SMTP |
| 53 | DNS |
| 80 | HTTP |
| 139 | NetBIOS |
| 443 | HTTPS |
| 445 | SMB |
| 554 | RTSP |
| 631 | CUPS (impresoras) |
| 993 | IMAPS |
| 995 | POP3S |
| 1883 | MQTT |
| 3306 | MySQL |
| 3389 | RDP |
| 5000 | Synology DSM |
| 5900 | VNC |
| 8008 | HTTP |
| 8009 | AJP13 |
| 8080 | HTTP |
| 8443 | HTTPS |
| 9000 | CSListener |
| 9080 | GLRPC |
| 9100 | JetDirect (impresoras) |
| 52869 | UPnP |

---

## Puertos extra (activables desde SELECT)

### Acceso remoto
| Puerto | Servicio |
|--------|---------|
| 2222 | SSH alternativo |
| 3389 | RDP |
| 5800 | VNC HTTP |
| 5900 | VNC |

### Web / HTTP
| Puerto | Servicio |
|--------|---------|
| 8000 | Hikvision / HTTP |
| 8081 | HTTP alternativo |
| 8086 | QNAP |
| 8088 | HTTP |
| 8123 | Home Assistant |
| 8161 | ActiveMQ |
| 8444 | HTTPS alternativo |
| 8554 | RTSP alternativo |
| 8888 | Jupyter Notebook |
| 9090 | Prometheus / Cockpit |

### Bases de datos
| Puerto | Servicio |
|--------|---------|
| 1433 | MSSQL |
| 5432 | PostgreSQL |
| 6379 | Redis |
| 9200 | Elasticsearch |
| 27017 | MongoDB |

### NAS / Almacenamiento
| Puerto | Servicio |
|--------|---------|
| 139 | NetBIOS |
| 2049 | NFS |
| 5001 | Synology HTTPS |
| 9091 | Transmission |
| 51413 | Transmission P2P |

### Cámaras IP
| Puerto | Servicio |
|--------|---------|
| 554 | RTSP estándar |
| 8554 | RTSP alternativo |
| 8000 | Hikvision |
| 18080 | TP-Link cámaras |
| 34567 | HiSilicon (cámaras genéricas chinas) |
| 34599 | HiSilicon alternativo |
| 37777 | Dahua |

### IoT / Smarthome
| Puerto | Servicio |
|--------|---------|
| 1883 | MQTT |
| 4567 | Shelly |
| 6668 | Tuya |
| 8123 | Home Assistant |
| 8883 | MQTT con TLS |
| 55443 | Xiaomi |

### Multimedia
| Puerto | Servicio |
|--------|---------|
| 548 | AFP (carpetas Mac) |
| 1400 | Sonos |
| 3689 | DAAP / iTunes |
| 7000 | AirPlay |
| 8200 | DLNA / Serviio |

### Mensajería / Colas
| Puerto | Servicio |
|--------|---------|
| 9092 | Kafka |
| 15672 | RabbitMQ |

### Email
| Puerto | Servicio |
|--------|---------|
| 110 | POP3 |
| 143 | IMAP |
| 465 | SMTPS |
| 587 | SMTP |

### Impresoras
| Puerto | Servicio |
|--------|---------|
| 515 | LPD |
| 631 | CUPS |
| 9100 | JetDirect |

---

## Detección de tipo de dispositivo por puertos

Si ves esta combinación de puertos en un host, probablemente es:

| Puertos abiertos | Dispositivo probable |
|-----------------|---------------------|
| `554` + `37777` | Cámara Dahua |
| `554` + `34567` | Cámara IP china (HiSilicon) |
| `554` + `8000` | Cámara Hikvision |
| `5000` + `5001` | NAS Synology |
| `8086` | NAS QNAP |
| `22` + `80` + `443` | Servidor Linux |
| `1883` | Broker MQTT / domótica |
| `8123` | Home Assistant |
| `3389` | Windows con escritorio remoto |
| `9100` | Impresora en red |
| `8008` + `8009` | Chromecast / dispositivo Google |
| `53` + `80` + `443` | Router |

---

## Posibles fallos conocidos

- **No detecta hosts sin puertos TCP abiertos** — dispositivos en standby o con firewall total no aparecen. Sin ICMP/ping no hay forma de detectarlos con TCP scan.
- **Falsos negativos en puertos lentos** — servicios que tardan más de 400ms en responder pueden aparecer como cerrados. Ocurre especialmente con bases de datos bajo carga.
- **El banner grabber puede bloquearse** — algunos servicios no envían banner automáticamente y el `recv()` espera hasta timeout. Afecta principalmente a servicios propietarios o mal configurados.
- **Límite de sockets** — el SDK de 3DS tiene un número limitado de sockets simultáneos. Si se abren y no se cierran correctamente (crash, cancelación brusca), puede quedarse sin sockets hasta reiniciar la app.
- **EINPROGRESS = 119** — en 3DS este errno tiene valor diferente al estándar POSIX (115). Está corregido en el código pero si compilas con otra versión de libctru puede variar.
- **No hay UDP** — el scan UDP en 3DS es prácticamente inviable. Sin ICMP port unreachable fiable y sin timeouts controlables, los resultados son inconsistentes.
- **Los banners HTTP bloqueantes pueden causar lentitud** — si un servidor HTTP tarda en responder a `HEAD /`, el escaneo se pausa hasta timeout en ese puerto.
- **La pantalla puede llenarse** — si hay muchos puertos abiertos los resultados se van de pantalla. Los resultados siempre quedan en el archivo de la SD.

---

## Limitaciones técnicas

- Solo TCP — no hay UDP
- No hay detección de OS (requeriría raw sockets, no disponibles en 3DS)
- No hay SYN scan (mismo motivo)
- Velocidad limitada por la WiFi de la 3DS y el scheduler del sistema
- Sin multihilo real — todo es secuencial con sockets no bloqueantes

---

## Construido con

- **libctru** — librería estándar de homebrew para 3DS
- **devkitARM** — toolchain de compilación
- Sockets POSIX estándar via `libctru/network`
- Sin dependencias externas

---

## Aviso legal

Este proyecto es para uso **exclusivo en redes propias** o en las que tengas permiso explícito para hacer pruebas. Escanear redes ajenas sin autorización es ilegal en la mayoría de países. El autor no se hace responsable del uso que se haga de esta herramienta.

---

*Hecho por las risas con una 3DS Old del 2011 y demasiado tiempo libre.*
