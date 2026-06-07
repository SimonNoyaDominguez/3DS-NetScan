#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>

#define TIMEOUT_PUERTO 400
#define TIMEOUT_SONDA 300
#define TIMEOUT_MQTT 150
#define BANNER_MAX 200
#define MAX_PUERTOS 80
#define MAX_RESULTADOS 200

char NETWORK_BASE[16] = "192.168.1.";
int octetos[4] = {192, 168, 1, 0};
int rango_inicio = 1;
int rango_fin = 254;
char mi_ip[16] = "0.0.0.0";

int puertos_base[] = {
    21, 22, 23, 25, 53, 80, 139, 443, 445, 554, 631, 993, 995,
    3306, 3389, 5900, 8080, 8443, 9100, 1883, 5000,
    8008, 8009, 9000, 9080, 52869
};
int num_base = sizeof(puertos_base) / sizeof(puertos_base[0]);

typedef struct {
    int puerto;
    const char* nombre;
    int activo;
} PuertoExtra;

PuertoExtra puertos_extra[] = {
    {8080, "HTTP-8080", 0}, {9999, "Prueba", 0},
    {2222, "SSH-Alt", 0}, {8081, "HTTP-Alt2", 0}, {8444, "HTTPS-Alt2", 0},
    {27017, "MongoDB", 0}, {6379, "Redis", 0}, {5432, "PostgreSQL", 0},
    {1433, "MSSQL", 0}, {5800, "VNC-http", 0}, {8089, "Splunk", 0},
    {9200, "Elasticsearch", 0}, {15672, "RabbitMQ", 0}, {9092, "Kafka", 0},
    {110, "POP3", 0}, {143, "IMAP", 0}, {465, "SMTPS", 0}, {587, "SMTP", 0},
    {515, "LPD", 0}, {548, "AFP", 0}, {8200, "DLNA", 0}, {1400, "Sonos", 0},
    {49152, "UPnP", 0}, {2049, "NFS", 0}, {5001, "Synology-HTTPS", 0},
    {8086, "QNAP", 0}, {9091, "Transmission", 0}, {51413, "Transmission", 0},
    {8554, "RTSP-Alt", 0}, {37777, "Dahua", 0}, {34567, "HiSilicon", 0},
    {34599, "HiSilicon-Alt", 0}, {8000, "Hikvision", 0}, {18080, "TP-Link", 0},
    {8883, "MQTT-TLS", 0}, {4567, "Shelly", 0}, {8123, "HomeAssistant", 0},
    {8888, "Jupyter", 0}, {55443, "Xiaomi", 0}, {6668, "Tuya", 0},
    {4000, "Framework", 0}, {7000, "AirPlay", 0}, {9090, "Prometheus", 0},
    {3000, "Grafana", 0}, {8161, "ActiveMQ", 0}
};
int num_extra = sizeof(puertos_extra) / sizeof(puertos_extra[0]);

int puertos_activos[MAX_PUERTOS];
int num_puertos_activos = 0;

char hosts_activos[100][16];
int total_hosts = 0;
int escaneando = 1;

void actualizar_puertos_activos() {
    num_puertos_activos = 0;
    for (int i = 0; i < num_base; i++) {
        puertos_activos[num_puertos_activos++] = puertos_base[i];
    }
    for (int i = 0; i < num_extra; i++) {
        if (puertos_extra[i].activo) {
            puertos_activos[num_puertos_activos++] = puertos_extra[i].puerto;
        }
    }
}

// ============================================
// GUARDAR RESULTADOS EN LA SD
// ============================================

void guardar_resultados(const char *ip, char *puertos_abiertos[], int num) {
    // Crear directorio si no existe
    mkdir("/3ds", 0777);
    mkdir("/3ds/scanner", 0777);
    
    FILE *f = fopen("/3ds/scanner/resultados.txt", "a");
    if (!f) return;

    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char fecha[64];
    strftime(fecha, sizeof(fecha), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(f, "\n=== [%s] %s ===\n", fecha, ip);
    for (int i = 0; i < num; i++) {
        fprintf(f, "%s\n", puertos_abiertos[i]);
    }
    fprintf(f, "----------------------------------------\n");
    fclose(f);
}

// ============================================
// HOST RESPONDE
// ============================================

#undef EINPROGRESS
#define EINPROGRESS 119

int host_responde(const char *ip, int puerto, int timeout_ms) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;

    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) flags = 0;
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(puerto);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    errno = 0;
    connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    if (errno != EINPROGRESS) {
        close(sock);
        return 0;
    }

    int intentos = timeout_ms / 10;
    if (intentos < 3) intentos = 3;

    for (int i = 0; i < intentos; i++) {
        svcSleepThread(10000000LL);

        fd_set wset, eset;
        FD_ZERO(&wset);
        FD_ZERO(&eset);
        FD_SET(sock, &wset);
        FD_SET(sock, &eset);
        struct timeval tv = {0, 0};

        int ret = select(sock + 1, NULL, &wset, &eset, &tv);

        if (ret > 0) {
            int conectado = FD_ISSET(sock, &wset);
            close(sock);
            if (conectado) return 1;
            return 0;
        }

        hidScanInput();
        if (hidKeysDown() & KEY_START) {
            escaneando = 0;
            close(sock);
            return 0;
        }
    }
    close(sock);
    return 0;
}

int host_activo(const char *ip) {
    int puertos_fiables[] = {22, 80, 443, 8443};
    for (int i = 0; i < 4; i++) {
        if (!escaneando) return 0;
        if (host_responde(ip, puertos_fiables[i], TIMEOUT_SONDA)) return 1;
    }
    if (!escaneando) return 0;
    if (host_responde(ip, 1883, TIMEOUT_MQTT)) return 1;
    return 0;
}

// ============================================
// PUERTO ABIERTO
// ============================================

int puerto_abierto(const char *ip, int puerto, int timeout_ms) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;
    
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = timeout_ms * 1000;
    setsockopt(sock, SOL_SOCKET, 0x1006, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, 0x1007, &timeout, sizeof(timeout));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(puerto);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    int abierto = (result == 0);
    
    close(sock);
    return abierto;
}

// ============================================
// BANNER GRABBER
// ============================================

const char* obtener_banner_http_https(const char *ip, int puerto, int timeout_ms, char *banner) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return NULL;
    
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = timeout_ms * 1000;
    setsockopt(sock, SOL_SOCKET, 0x1006, &timeout, sizeof(timeout));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(puerto);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        char http_request[] = "HEAD / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
        send(sock, http_request, strlen(http_request), 0);
        
        char buffer[BANNER_MAX];
        int recibido = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (recibido > 0) {
            buffer[recibido] = '\0';
            char *server = strstr(buffer, "Server:");
            if (!server) server = strstr(buffer, "server:");
            if (server) {
                server += 7;
                while (*server == ' ' || *server == '\t') server++;
                char *end = strstr(server, "\r\n");
                if (!end) end = strstr(server, "\n");
                if (end) {
                    int len = end - server;
                    if (len > 45) len = 45;
                    strncpy(banner, server, len);
                    banner[len] = '\0';
                    close(sock);
                    return banner;
                }
            }
        }
    }
    close(sock);
    return NULL;
}

const char* obtener_banner_generico(const char *ip, int puerto, int timeout_ms, char *banner) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return NULL;
    
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = timeout_ms * 1000;
    setsockopt(sock, SOL_SOCKET, 0x1006, &timeout, sizeof(timeout));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(puerto);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        char buffer[BANNER_MAX];
        int recibido = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (recibido > 0) {
            buffer[recibido] = '\0';
            int j = 0;
            for (int i = 0; i < recibido && j < BANNER_MAX - 1; i++) {
                if (buffer[i] >= 32 && buffer[i] <= 126) {
                    banner[j++] = buffer[i];
                } else if (buffer[i] == '\n') break;
            }
            banner[j] = '\0';
            close(sock);
            return banner;
        }
    }
    close(sock);
    return NULL;
}

const char* servicio(int puerto) {
    for (int i = 0; i < num_extra; i++) {
        if (puertos_extra[i].puerto == puerto) return puertos_extra[i].nombre;
    }
    switch(puerto) {
        case 21: return "FTP"; case 22: return "SSH"; case 23: return "Telnet";
        case 25: return "SMTP"; case 53: return "DNS"; case 80: return "HTTP";
        case 139: return "NetBIOS"; case 143: return "IMAP"; case 443: return "HTTPS";
        case 445: return "SMB"; case 465: return "SMTPS"; case 515: return "LPD";
        case 548: return "AFP"; case 554: return "RTSP"; case 587: return "SMTP";
        case 631: return "CUPS"; case 993: return "IMAPS"; case 995: return "POP3S";
        case 1400: return "Sonos"; case 1883: return "MQTT"; case 2049: return "NFS";
        case 3000: return "Grafana"; case 3306: return "MySQL"; case 3389: return "RDP";
        case 34567: return "HiSilicon"; case 34599: return "HiSilicon-Alt";
        case 37777: return "Dahua"; case 4000: return "Framework";
        case 4567: return "Shelly"; case 5000: return "Synology"; case 5001: return "Synology-HTTPS";
        case 51413: return "Transmission"; case 5800: return "VNC-http"; case 5900: return "VNC";
        case 6668: return "Tuya"; case 7000: return "AirPlay"; case 8000: return "Hikvision";
        case 8008: return "HTTP"; case 8009: return "AJP13"; case 8080: return "HTTP";
        case 8086: return "QNAP"; case 8089: return "Splunk"; case 8123: return "HomeAssistant";
        case 8161: return "ActiveMQ"; case 8200: return "DLNA"; case 8443: return "HTTPS";
        case 8554: return "RTSP-Alt"; case 8883: return "MQTT-TLS"; case 8888: return "Jupyter";
        case 9000: return "CSListener"; case 9080: return "GLRPC"; case 9090: return "Prometheus";
        case 9091: return "Transmission"; case 9092: return "Kafka"; case 9100: return "JetDirect";
        case 9200: return "Elasticsearch"; case 15672: return "RabbitMQ"; case 18080: return "TP-Link";
        case 27017: return "MongoDB"; case 49152: return "UPnP"; case 52869: return "UPnP";
        case 55443: return "Xiaomi"; case 6379: return "Redis"; case 5432: return "PostgreSQL";
        case 1433: return "MSSQL"; case 2222: return "SSH-Alt"; case 8444: return "HTTPS-Alt2";
        default: return "?";
    }
}

// ============================================
// GESTOR DE PUERTOS EXTRA
// ============================================

void gestionar_puertos_extra() {
    int seleccion = 0, y_offset = 0, max_visible = 14, configurando = 1, primera_vez = 1;
    actualizar_puertos_activos();
    
    while (configurando) {
        if (primera_vez) { printf("\x1b[2J"); primera_vez = 0; }
        printf("\x1b[1;1H");
        printf("==========================================\n");
        printf("     PUERTOS EXTRA (SELECT para marcar)\n");
        printf("==========================================\n");
        printf("Puertos base activos: %d\n", num_base);
        int extra_count = 0;
        for (int i = 0; i < num_extra; i++) if (puertos_extra[i].activo) extra_count++;
        printf("Puertos extra activos: %d\n", extra_count);
        printf("Total puertos a escanear: %d\n\n", num_base + extra_count);
        
        if (seleccion >= y_offset + max_visible) y_offset = seleccion - max_visible + 1;
        if (seleccion < y_offset) y_offset = seleccion;
        int end = y_offset + max_visible;
        if (end > num_extra) end = num_extra;
        
        for (int i = y_offset; i < end; i++) {
            if (i == seleccion) {
                printf(puertos_extra[i].activo ? ">> [X] %d - %s\n" : ">> [ ] %d - %s\n", puertos_extra[i].puerto, puertos_extra[i].nombre);
            } else {
                printf(puertos_extra[i].activo ? "   [X] %d - %s\n" : "   [ ] %d - %s\n", puertos_extra[i].puerto, puertos_extra[i].nombre);
            }
        }
        printf("\nCONTROLES: UP/DOWN - Mover | A - Marcar | X - Todos | Y - Limpiar | B - Volver\n");
        
        gfxFlushBuffers(); gfxSwapBuffers();
        hidScanInput();
        u32 k = hidKeysDown();
        
        if (k & KEY_DUP && seleccion > 0) seleccion--;
        else if (k & KEY_DDOWN && seleccion < num_extra - 1) seleccion++;
        else if (k & KEY_A) { puertos_extra[seleccion].activo = !puertos_extra[seleccion].activo; actualizar_puertos_activos(); }
        else if (k & KEY_X) { for (int i = 0; i < num_extra; i++) puertos_extra[i].activo = 1; actualizar_puertos_activos(); }
        else if (k & KEY_Y) { for (int i = 0; i < num_extra; i++) puertos_extra[i].activo = 0; actualizar_puertos_activos(); }
        else if (k & KEY_B) configurando = 0;
        gspWaitForVBlank();
    }
    printf("\x1b[2J");
}

// ============================================
// CONFIGURAR IP BASE
// ============================================

void actualizar_ip_base() { snprintf(NETWORK_BASE, sizeof(NETWORK_BASE), "%d.%d.%d.", octetos[0], octetos[1], octetos[2]); }

void configurar_ip_base() {
    int seleccion = 0, configurando = 1;
    printf("\x1b[2J");
    while (configurando) {
        printf("\x1b[1;1H");
        printf("==========================================\n");
        printf("     CONFIGURAR IP BASE\n");
        printf("==========================================\n\n");
        printf("IP actual: %d.%d.%d.X\n", octetos[0], octetos[1], octetos[2]);
        printf("\n");
        printf(seleccion == 0 ? ">> [%d] . %d . %d . X\n" : "   [%d] . %d . %d . X\n", octetos[0], octetos[1], octetos[2]);
        printf(seleccion == 1 ? ">>  %d . [%d] . %d . X\n" : "   %d . %d . %d . X\n", octetos[0], octetos[1], octetos[2]);
        printf(seleccion == 2 ? ">>  %d . %d . [%d] . X\n" : "   %d . %d . %d . X\n", octetos[0], octetos[1], octetos[2]);
        printf("\nCONTROLES: UP/DOWN | LEFT/RIGHT | A-Guardar | B-Cancelar\n");
        
        gfxFlushBuffers(); gfxSwapBuffers();
        hidScanInput();
        u32 k = hidKeysDown();
        
        if (k & KEY_DUP && octetos[seleccion] < 254) octetos[seleccion]++;
        else if (k & KEY_DDOWN && octetos[seleccion] > 1) octetos[seleccion]--;
        else if (k & KEY_DRIGHT && seleccion < 2) seleccion++;
        else if (k & KEY_DLEFT && seleccion > 0) seleccion--;
        else if (k & KEY_A) configurando = 0;
        else if (k & KEY_B) { octetos[0] = 192; octetos[1] = 168; octetos[2] = 1; actualizar_ip_base(); configurando = 0; }
        actualizar_ip_base();
        gspWaitForVBlank();
    }
    printf("\x1b[2J");
}

// ============================================
// CONFIGURAR RANGO
// ============================================

void configurar_rango() {
    int inicios[] = {1, 51, 101, 151, 201};
    int fines[]   = {50, 100, 150, 200, 254};
    int num_opciones = 5;

    int idx_inicio = 0;
    int idx_fin = num_opciones - 1;
    for (int i = 0; i < num_opciones; i++) {
        if (inicios[i] == rango_inicio) idx_inicio = i;
        if (fines[i] == rango_fin) idx_fin = i;
    }

    int seleccion = 0;
    int configurando = 1;
    int primera_vez = 1;

    while (configurando) {
        if (primera_vez) { printf("\x1b[2J"); primera_vez = 0; }

        printf("\x1b[1;1H");
        printf("==========================================\n");
        printf("     CONFIGURAR RANGO DE IPS\n");
        printf("==========================================\n\n");
        printf("IP base: %s\n\n", NETWORK_BASE);

        printf("--- INICIO ---\n");
        for (int i = 0; i < num_opciones; i++) {
            if (seleccion == 0 && i == idx_inicio)
                printf(">> %s%d\n", NETWORK_BASE, inicios[i]);
            else
                printf("   %s%d\n", NETWORK_BASE, inicios[i]);
        }

        printf("\n--- FIN ---\n");
        for (int i = 0; i < num_opciones; i++) {
            if (seleccion == 1 && i == idx_fin)
                printf(">> %s%d\n", NETWORK_BASE, fines[i]);
            else
                printf("   %s%d\n", NETWORK_BASE, fines[i]);
        }

        printf("\nRango actual: %s%d - %s%d\n", 
               NETWORK_BASE, inicios[idx_inicio],
               NETWORK_BASE, fines[idx_fin]);
        printf("LEFT/RIGHT inicio/fin | UP/DOWN elegir | A guardar | B cancelar\n");

        gfxFlushBuffers(); gfxSwapBuffers();
        hidScanInput();
        u32 k = hidKeysDown();

        if (k & KEY_DLEFT) seleccion = 0;
        if (k & KEY_DRIGHT) seleccion = 1;

        if (k & KEY_DUP) {
            if (seleccion == 0 && idx_inicio > 0) idx_inicio--;
            else if (seleccion == 1 && idx_fin > 0) idx_fin--;
        }
        if (k & KEY_DDOWN) {
            if (seleccion == 0 && idx_inicio < num_opciones - 1) idx_inicio++;
            else if (seleccion == 1 && idx_fin < num_opciones - 1) idx_fin++;
        }

        if (inicios[idx_inicio] >= fines[idx_fin]) {
            if (seleccion == 0 && idx_inicio < num_opciones - 1) idx_fin = idx_inicio + 1;
            else if (seleccion == 1 && idx_fin > 0) idx_inicio = idx_fin - 1;
        }

        if (k & KEY_A) {
            rango_inicio = inicios[idx_inicio];
            rango_fin = fines[idx_fin];
            configurando = 0;
        }
        if (k & KEY_B) {
            rango_inicio = 1;
            rango_fin = 254;
            configurando = 0;
        }

        gspWaitForVBlank();
    }
    printf("\x1b[2J");
}

// ============================================
// OBTENER IP DE LA 3DS
// ============================================

void obtener_mi_ip() {
    struct in_addr ip, netmask, broadcast;
    if (SOCU_GetIPInfo(&ip, &netmask, &broadcast) == 0) {
        strncpy(mi_ip, inet_ntoa(ip), sizeof(mi_ip) - 1);
        mi_ip[sizeof(mi_ip) - 1] = '\0';
    } else {
        strcpy(mi_ip, "No IP");
    }
}

// ============================================
// DIBUJAR MENU
// ============================================

void dibujar_menu() {
    int extra_count = 0;
    for (int i = 0; i < num_extra; i++) if (puertos_extra[i].activo) extra_count++;
    
    printf("\x1b[1;1H");
    printf("==========================================\n");
    printf("      3DS NETWORK SCANNER\n");
    printf("==========================================\n\n");
    printf("Mi IP: %s\n", mi_ip);
    printf("Red: %s%d-%d\n", NETWORK_BASE, rango_inicio, rango_fin);
    printf("Puertos base: %d\n", num_base);
    printf("Puertos extra: %d activos\n", extra_count);
    printf("Total escaneo: %d puertos\n\n", num_base + extra_count);
    printf("  A - Escaneo COMPLETO\n");
    printf("  B - Solo buscar HOSTS\n");
    printf("  Y - Escanear IP (SIN banner)\n");
    printf("  X - Escanear IP (CON banner)\n");
    printf("  R - CONFIGURAR IP BASE\n");
    printf("  L - CONFIGURAR RANGO\n");
    printf("  SELECT - PUERTOS EXTRA\n");
    printf("  START - CANCELAR / Salir\n");
    printf("\nResultados guardados en /3ds/scanner/resultados.txt\n");
}

// ============================================
// ESCANEAR RED
// ============================================

void escanear_red() {
    escaneando = 1;
    total_hosts = 0;
    
    printf("\x1b[2J\x1b[1;1H");
    printf("==========================================\n");
    printf("     BUSCANDO HOSTS EN %s%d-%d\n", NETWORK_BASE, rango_inicio, rango_fin);
    printf("==========================================\n");
    printf("Puertos sonda: 22,80,443,8443,1883\n");
    printf("Timeout: 300ms (1883: 150ms)\n");
    printf("Presiona START para CANCELAR\n\n");
    
    for (int i = rango_inicio; i <= rango_fin && escaneando; i++) {
        char ip[20];
        snprintf(ip, sizeof(ip), "%s%d", NETWORK_BASE, i);
        
        printf("\x1b[5;1H\x1b[K");
        printf("IP: %s (%d-%d)", ip, i, rango_fin);
        printf("\x1b[6;1H\x1b[K");
        printf("Hosts encontrados: %d", total_hosts);
        gfxFlushBuffers(); gfxSwapBuffers();
        
        if (host_activo(ip)) {
            strcpy(hosts_activos[total_hosts], ip);
            total_hosts++;
            printf("\x1b[%d;1H\x1b[K", 8 + total_hosts);
            printf("[OK] %s", ip);
            gfxFlushBuffers(); gfxSwapBuffers();
        }
        
        hidScanInput();
        if (hidKeysDown() & KEY_START) {
            escaneando = 0;
            printf("\x1b[24;1H\x1b[K");
            printf("ESCANEO CANCELADO");
            gfxFlushBuffers(); gfxSwapBuffers();
            break;
        }
        gspWaitForVBlank();
    }
    
    if (escaneando) {
        printf("\x1b[24;1H\x1b[K");
        printf("Completado. %d hosts activos", total_hosts);
        gfxFlushBuffers(); gfxSwapBuffers();
    }
}

// ============================================
// ESCANEAR PUERTOS SIN BANNER
// ============================================

void escanear_puertos_sin_banner(const char *ip) {
    actualizar_puertos_activos();
    escaneando = 1;
    
    printf("\x1b[2J\x1b[1;1H");
    printf("==========================================\n");
    printf("     ESCANEANDO %s\n", ip);
    printf("==========================================\n");
    printf("Puertos a escanear: %d\n", num_puertos_activos);
    printf("Presiona START para CANCELAR\n\n");
    
    int encontrados = 0;
    char *resultados[MAX_RESULTADOS];
    int num_resultados = 0;
    
    for (int i = 0; i < MAX_RESULTADOS; i++) {
        resultados[i] = malloc(256);
        resultados[i][0] = '\0';
    }
    
    for (int i = 0; i < num_puertos_activos && escaneando; i++) {
        printf("\x1b[5;1H\x1b[K");
        printf("Puerto %d... (%d/%d)", puertos_activos[i], i+1, num_puertos_activos);
        gfxFlushBuffers(); gfxSwapBuffers();
        
        if (puerto_abierto(ip, puertos_activos[i], TIMEOUT_PUERTO)) {
            encontrados++;
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "[ABIERTO] %d - %s", puertos_activos[i], servicio(puertos_activos[i]));
            strcpy(resultados[num_resultados++], buffer);
            printf("\x1b[%d;1H\x1b[K", 7 + encontrados);
            printf("%s", buffer);
            gfxFlushBuffers(); gfxSwapBuffers();
        }
        
        hidScanInput();
        if (hidKeysDown() & KEY_START) { escaneando = 0; break; }
        gspWaitForVBlank();
    }
    
    if (escaneando && encontrados == 0) {
        printf("\x1b[7;1H\x1b[K");
        printf("No hay puertos abiertos");
    }
    
    // Guardar resultados en SD
    if (encontrados > 0) {
        guardar_resultados(ip, resultados, num_resultados);
    }
    
    for (int i = 0; i < MAX_RESULTADOS; i++) free(resultados[i]);
    
    printf("\n\n\x1b[24;1HPresiona B para volver...");
    gfxFlushBuffers(); gfxSwapBuffers();
    while (1) {
        hidScanInput();
        if (hidKeysDown() & KEY_B) break;
        gspWaitForVBlank();
    }
}

// ============================================
// ESCANEAR PUERTOS CON BANNER
// ============================================

void escanear_puertos_con_banner(const char *ip) {
    actualizar_puertos_activos();
    escaneando = 1;

    printf("\x1b[2J\x1b[1;1H");
    printf("==========================================\n");
    printf("     ESCANEANDO %s\n", ip);
    printf("==========================================\n");
    printf("Puertos: %d | Banner: ON\n", num_puertos_activos);
    printf("------------------------------------------\n");
    printf("Resultados:\n");
    
    int encontrados = 0;
    char banner[BANNER_MAX];
    int linea_actual = 7;
    char *resultados[MAX_RESULTADOS];
    int num_resultados = 0;
    
    for (int i = 0; i < MAX_RESULTADOS; i++) {
        resultados[i] = malloc(256);
        resultados[i][0] = '\0';
    }

    for (int i = 0; i < num_puertos_activos && escaneando; i++) {
        printf("\x1b[5;1H\x1b[K");
        printf("Progreso: %d/%d - Puerto %d", i+1, num_puertos_activos, puertos_activos[i]);
        gfxFlushBuffers(); gfxSwapBuffers();

        if (puerto_abierto(ip, puertos_activos[i], TIMEOUT_PUERTO)) {
            encontrados++;
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "[OPEN] %d %s", puertos_activos[i], servicio(puertos_activos[i]));

            banner[0] = '\0';
            
            if (puertos_activos[i] == 80 || puertos_activos[i] == 443 ||
                puertos_activos[i] == 8008 || puertos_activos[i] == 8080 ||
                puertos_activos[i] == 8443 || puertos_activos[i] == 5000 ||
                puertos_activos[i] == 5001 || puertos_activos[i] == 8086 ||
                puertos_activos[i] == 8123 || puertos_activos[i] == 8888 ||
                puertos_activos[i] == 9090) {
                obtener_banner_http_https(ip, puertos_activos[i], TIMEOUT_PUERTO, banner);
            }
            else if (puertos_activos[i] == 22 || puertos_activos[i] == 2222 ||
                     puertos_activos[i] == 23 || puertos_activos[i] == 25 ||
                     puertos_activos[i] == 554 || puertos_activos[i] == 8554 ||
                     puertos_activos[i] == 4567 || puertos_activos[i] == 37777 ||
                     puertos_activos[i] == 34567 || puertos_activos[i] == 8000 ||
                     puertos_activos[i] == 18080 || puertos_activos[i] == 7000) {
                obtener_banner_generico(ip, puertos_activos[i], TIMEOUT_PUERTO, banner);
            }
            
            if (banner[0] != '\0') {
                snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), " [%s]", banner);
            }
            
            strcpy(resultados[num_resultados++], buffer);
            printf("\x1b[%d;1H\x1b[K", linea_actual);
            printf("%s", buffer);
            linea_actual++;
            gfxFlushBuffers(); gfxSwapBuffers();
        }
        
        hidScanInput();
        if (hidKeysDown() & KEY_START) { escaneando = 0; break; }
        gspWaitForVBlank();
    }

    if (escaneando && encontrados == 0) {
        printf("\x1b[7;1H\x1b[K");
        printf("No hay puertos abiertos");
        linea_actual++;
    }
    
    // Guardar resultados en SD
    if (encontrados > 0) {
        guardar_resultados(ip, resultados, num_resultados);
    }
    
    for (int i = 0; i < MAX_RESULTADOS; i++) free(resultados[i]);

    printf("\x1b[5;1H\x1b[K");
    printf("Completado. %d puertos abiertos.", encontrados);
    printf("\x1b[%d;1HPresiona B para volver...", linea_actual + 2);
    gfxFlushBuffers(); gfxSwapBuffers();
    
    while (1) {
        hidScanInput();
        if (hidKeysDown() & KEY_B) break;
        gspWaitForVBlank();
    }
}

// ============================================
// SELECCIONAR IP
// ============================================

void seleccionar_ip(int con_banner) {
    printf("\x1b[2J\x1b[1;1H");
    printf("==========================================\n");
    printf(con_banner ? "     ESCANEAR IP (CON BANNER)\n" : "     ESCANEAR IP (SIN BANNER)\n");
    printf("==========================================\n\n");
    printf("Usa UP/DOWN para cambiar el numero\n");
    printf("X - Sumar 10 | Y - Restar 10\n");
    printf("Presiona A para escanear, B para cancelar\n\n");
    
    int num = 1;
    int seleccionando = 1;
    
    while (seleccionando) {
        char ip[20];
        snprintf(ip, sizeof(ip), "%s%d", NETWORK_BASE, num);
        printf("\x1b[7;1H\x1b[K");
        printf("IP: %s", ip);
        gfxFlushBuffers(); gfxSwapBuffers();
        
        hidScanInput();
        u32 k = hidKeysDown();
        
        if (k & KEY_DUP && num < rango_fin) num++;
        else if (k & KEY_DDOWN && num > rango_inicio) num--;
        else if (k & KEY_X && num + 10 <= rango_fin) num += 10;
        else if (k & KEY_Y && num - 10 >= rango_inicio) num -= 10;
        else if (k & KEY_A) {
            snprintf(ip, sizeof(ip), "%s%d", NETWORK_BASE, num);
            if (con_banner) escanear_puertos_con_banner(ip);
            else escanear_puertos_sin_banner(ip);
            seleccionando = 0;
        }
        else if (k & KEY_B) seleccionando = 0;
        gspWaitForVBlank();
    }
}

// ============================================
// ESCANEAR HOSTS ENCONTRADOS
// ============================================

void escanear_hosts_encontrados() {
    if (total_hosts == 0) {
        printf("\nNo hay hosts. Primero haz 'B - Solo hosts'\nPresiona B...");
        gfxFlushBuffers(); gfxSwapBuffers();
        while (1) { hidScanInput(); if (hidKeysDown() & KEY_B) break; gspWaitForVBlank(); }
        return;
    }
    
    for (int h = 0; h < total_hosts && escaneando; h++) {
        printf("\n\x1b[23;1H\x1b[K");
        printf("Escaneando %d/%d - START saltar", h+1, total_hosts);
        gfxFlushBuffers(); gfxSwapBuffers();
        escanear_puertos_sin_banner(hosts_activos[h]);
        if (!escaneando) break;
        for (int p = 0; p < 10; p++) { 
            hidScanInput(); 
            if (hidKeysDown() & KEY_START) { escaneando = 0; return; }
            gspWaitForVBlank(); 
        }
    }
}

// ============================================
// OPCIONES DEL MENU
// ============================================

void completo() {
    escanear_red();
    if (total_hosts == 0) {
        printf("\n\nNo hay hosts. Presiona B...");
        gfxFlushBuffers(); gfxSwapBuffers();
        while (1) { hidScanInput(); if (hidKeysDown() & KEY_B) break; gspWaitForVBlank(); }
        return;
    }
    escanear_hosts_encontrados();
    
    if (escaneando) {
        printf("\x1b[2J\x1b[1;1H");
        printf("==========================================\n");
        printf("           RESUMEN FINAL\n");
        printf("==========================================\n\n");
        for (int h = 0; h < total_hosts; h++) {
            printf("%s:\n", hosts_activos[h]);
            for (int i = 0; i < num_puertos_activos; i++) {
                if (puerto_abierto(hosts_activos[h], puertos_activos[i], TIMEOUT_PUERTO)) {
                    printf("  - %d %s\n", puertos_activos[i], servicio(puertos_activos[i]));
                }
            }
            printf("\n");
            gfxFlushBuffers(); gfxSwapBuffers();
        }
        printf("Presiona B para volver...");
        gfxFlushBuffers(); gfxSwapBuffers();
        while (1) { hidScanInput(); if (hidKeysDown() & KEY_B) break; gspWaitForVBlank(); }
    }
}

void solo_hosts() {
    escanear_red();
    printf("\n\nPresiona B para volver...");
    gfxFlushBuffers(); gfxSwapBuffers();
    while (1) { hidScanInput(); if (hidKeysDown() & KEY_B) break; gspWaitForVBlank(); }
}

// ============================================
// MAIN
// ============================================

int main() {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    
    u32 *soc_buffer = (u32*)memalign(0x1000, 0x100000);
    if (!soc_buffer) {
        printf("Error de memoria SOC\n");
        gfxExit();
        return -1;
    }
    socInit(soc_buffer, 0x100000);
    
    obtener_mi_ip();
    actualizar_puertos_activos();
    
    int run = 1;
    printf("\x1b[2J");
    
    while (run) {
        dibujar_menu();
        gfxFlushBuffers(); gfxSwapBuffers();
        
        hidScanInput();
        u32 k = hidKeysDown();
        
        if (k & KEY_A) { completo(); printf("\x1b[2J"); }
        else if (k & KEY_B) { solo_hosts(); printf("\x1b[2J"); }
        else if (k & KEY_Y) { seleccionar_ip(0); printf("\x1b[2J"); }
        else if (k & KEY_X) { seleccionar_ip(1); printf("\x1b[2J"); }
        else if (k & KEY_R) { configurar_ip_base(); printf("\x1b[2J"); }
        else if (k & KEY_L) { configurar_rango(); printf("\x1b[2J"); }
        else if (k & KEY_SELECT) { gestionar_puertos_extra(); printf("\x1b[2J"); }
        else if (k & KEY_START) run = 0;
        
        gspWaitForVBlank();
    }
    
    socExit();
    free(soc_buffer);
    gfxExit();
    return 0;
}
