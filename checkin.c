/**
 * Monitor de Ocupação de Prédio com OLED, Botões e Servidor HTTP via Wi‑Fi
 *
 * Funcionalidades:
 *  - A interface web (HTTP) permite selecionar um andar (0 a 4) e enviar ações ("add", "remove", "clear")
 *    para atualizar a ocupação.
 *  - O display OLED mostra o status do andar selecionado (ex.: "Terreo: X pessoas" ou "Andar N: X pessoas").
 *  - Botões físicos (GPIO 5 e 6) permitem alterar a seleção de andar.
 *  - LEDs RGB (GPIO 13, 11, 12) indicam se o andar está vazio (vermelho) ou ocupado (verde).
 *  - O Wi‑Fi é inicializado em modo Access Point com servidores DHCP/DNS e um servidor HTTP responde às requisições.
 */

 #include "pico/stdlib.h"
 #include "hardware/i2c.h"
 #include "pico/binary_info.h"
 
 // Wi‑Fi e lwIP
 #include "pico/cyw43_arch.h"
 #include "lwip/tcp.h"
 #include "lwip/inet.h"
 #include "dhcpserver/dhcpserver.h"
 #include "dnsserver/dnsserver.h"
 
 // Drivers do display OLED – baseados na API ssd1306_t
 #include "ssd1306.h"       // Contém definições, comandos e protótipos para o SSD1306
 #include "ssd1306_i2c.h"   // Implementação via I2C
 #include "ssd1306_font.h"  // Fonte utilizada pelo display
 
 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 #include <ctype.h>
 #include <stdbool.h>
 #include <time.h>
 
 // ─── DEFINIÇÕES DE HARDWARE ──────────────────────────────────────────────
 #define LED_R_PIN 13
 #define LED_G_PIN 11
 #define LED_B_PIN 12
 
 #define BUTTON_A 5   // Botão A: decrementa andar
 #define BUTTON_B 6   // Botão B: incrementa andar
 
 // Configurações do display OLED (128×64)
 #define SSD1306_WIDTH    128
 #define SSD1306_HEIGHT   64
 #ifndef SSD1306_I2C_ADDR
   #define SSD1306_I2C_ADDR _u(0x3C)
 #endif
 #ifndef SSD1306_I2C_CLK
   #define SSD1306_I2C_CLK 400
 #endif
 
 // I2C utilizado (para Pico W, usamos i2c_default)
 #define I2C_PORT i2c_default
 
 // Porta do servidor HTTP
 #define HTTP_PORT 80
 
 // Se não definido, define a autenticação WPA2 (valor 4 conforme exemplo)
 #ifndef CYW43_AUTH_WPA2_AES_PSK
   #define CYW43_AUTH_WPA2_AES_PSK 4
 #endif
 
 // Configurações de ocupação
 #define NUM_FLOORS     5
 #define MAX_OCCUPANCY  50
 
 // ─── VARIÁVEIS GLOBAIS ─────────────────────────────────────────────────────
 static int occupancy[NUM_FLOORS] = {0, 0, 0, 0, 0};  // Ocupação por andar
 static int selected_floor = 0;  // Andar atualmente selecionado
 
 // Instância global do display OLED
 static ssd1306_t oled_instance;
 
 // ─── FUNÇÕES AUXILIARES ─────────────────────────────────────────────────────
 
 // Extrai os parâmetros "floor" e "action" da query string da requisição HTTP
 static void parse_query_params(const char *request_line, char *floor_str, size_t floor_len,
                                  char *action, size_t action_len) {
     floor_str[0] = '\0';
     action[0] = '\0';
     const char *p = strstr(request_line, "floor=");
     if (p) {
         p += strlen("floor=");
         size_t i = 0;
         while (*p && *p != '&' && *p != ' ' && i < floor_len - 1) {
             floor_str[i++] = *p++;
         }
         floor_str[i] = '\0';
     }
     p = strstr(request_line, "action=");
     if (p) {
         p += strlen("action=");
         size_t i = 0;
         while (*p && *p != '&' && *p != ' ' && i < action_len - 1) {
             action[i++] = *p++;
         }
         action[i] = '\0';
     }
 }
 
 // Atualiza os LEDs RGB conforme a ocupação do andar selecionado
 void update_led_status(void) {
     if (occupancy[selected_floor] > 0) {
         gpio_put(LED_R_PIN, 0);
         gpio_put(LED_G_PIN, 1);
         gpio_put(LED_B_PIN, 0);
     } else {
         gpio_put(LED_R_PIN, 1);
         gpio_put(LED_G_PIN, 0);
         gpio_put(LED_B_PIN, 0);
     }
 }
 
 // Atualiza o display OLED com o status do andar selecionado
 void update_oled_display(void) {
     char buf[64];
     ssd1306_clear(&oled_instance);
     if (selected_floor == 0)
         snprintf(buf, sizeof(buf), "Terreo: %d pessoas", occupancy[selected_floor]);
     else
         snprintf(buf, sizeof(buf), "Andar %d: %d pessoas", selected_floor, occupancy[selected_floor]);
     ssd1306_draw_string(&oled_instance, 0, 0, 1, buf);
     ssd1306_show(&oled_instance);
 }
 
 // Lê o estado de um botão (botões com pull‑up: retorna 1 se pressionado)
 int read_button(uint pin) {
     return (gpio_get(pin) == 0);
 }
 
 // Atualiza a seleção de andar via botões
 void update_floor_selection(void) {
     if (read_button(BUTTON_B)) {
         selected_floor = (selected_floor + 1) % NUM_FLOORS;
         update_oled_display();
         update_led_status();
         sleep_ms(300); // debounce
     }
     if (read_button(BUTTON_A)) {
         selected_floor = (selected_floor - 1 + NUM_FLOORS) % NUM_FLOORS;
         update_oled_display();
         update_led_status();
         sleep_ms(300); // debounce
     }
 }
 
 // Atualiza a ocupação (pode ser chamada via HTTP)
 void update_occupancy(const char *floor_str, const char *action) {
     int floor = atoi(floor_str);
     if (floor < 0 || floor >= NUM_FLOORS) return;
     selected_floor = floor;
     if (strcmp(action, "add") == 0) {
         if (occupancy[floor] < MAX_OCCUPANCY)
             occupancy[floor]++;
     } else if (strcmp(action, "remove") == 0) {
         if (occupancy[floor] > 0)
             occupancy[floor]--;
     } else if (strcmp(action, "clear") == 0) {
         occupancy[floor] = 0;
     }
     printf("Andar %d: nova ocupacao = %d\n", floor, occupancy[floor]);
     update_led_status();
     update_oled_display();
 }
 
 // Gera a página HTML que exibe um formulário para modificar a ocupação e uma tabela com o status de todos os andares
 void create_html_page(char *buffer, size_t buffer_size) {
     char body[2048] = "";
     strcat(body, "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Monitor de Ocupacao</title>");
     strcat(body, "<style>table, th, td { border: 1px solid black; border-collapse: collapse; padding: 8px; }</style>");
     strcat(body, "<meta http-equiv=\"Cache-Control\" content=\"no-store\"/>");
     strcat(body, "</head><body>");
     strcat(body, "<h1>Monitor de Ocupacao do Predio</h1>");
     
     // Formulário
     strcat(body, "<form action=\"/\" method=\"GET\">");
     strcat(body, "<label for=\"floor\">Selecione o Andar:</label>");
     strcat(body, "<select name=\"floor\" id=\"floor\">");
     for (int i = 0; i < NUM_FLOORS; i++) {
         char option[64];
         if (i == 0)
             snprintf(option, sizeof(option), "<option value=\"%d\" %s>Terreo</option>", i, (i==selected_floor)?"selected":"");
         else
             snprintf(option, sizeof(option), "<option value=\"%d\" %s>Andar %d</option>", i, (i==selected_floor)?"selected":"", i);
         strcat(body, option);
     }
     strcat(body, "</select><br/><br/>");
     strcat(body, "<input type=\"submit\" name=\"action\" value=\"add\"> ");
     strcat(body, "<input type=\"submit\" name=\"action\" value=\"remove\"> ");
     strcat(body, "<input type=\"submit\" name=\"action\" value=\"clear\"> ");
     strcat(body, "</form>");
     
     // Tabela com o status de todos os andares
     strcat(body, "<h2>Status dos Andares</h2>");
     strcat(body, "<table>");
     strcat(body, "<tr><th>Andar</th><th>Ocupacao</th></tr>");
     for (int i = 0; i < NUM_FLOORS; i++) {
         char row[128];
         if (i == 0)
             snprintf(row, sizeof(row), "<tr><td>Terreo</td><td>%d pessoas</td></tr>", occupancy[i]);
         else
             snprintf(row, sizeof(row), "<tr><td>Andar %d</td><td>%d pessoas</td></tr>", i, occupancy[i]);
         strcat(body, row);
     }
     strcat(body, "</table>");
     
     strcat(body, "</body></html>");
     
     snprintf(buffer, buffer_size,
              "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\nConnection: close\r\n\r\n%s",
              body);
 }
  
 /* ─── FUNÇÕES DO SERVIDOR HTTP ───────────────────────────────────────────── */
  
 // Callback chamada após o envio completo da resposta HTTP (fecha a conexão)
 static err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
     printf("Resposta enviada, fechando conexao.\n");
     tcp_arg(tpcb, NULL);
     tcp_recv(tpcb, NULL);
     tcp_sent(tpcb, NULL);
     err_t err = tcp_close(tpcb);
     if (err != ERR_OK) {
         printf("Erro ao fechar conexao (err=%d), abortando.\n", err);
         tcp_abort(tpcb);
     }
     return ERR_OK;
 }
  
 // Callback do HTTP: processa a requisição GET e atualiza a ocupação se os parâmetros estiverem presentes
 static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
     if (p == NULL) {
          tcp_close(tpcb);
          return ERR_OK;
     }
     char request[1024] = {0};
     size_t copy_len = (p->tot_len < sizeof(request)-1) ? p->tot_len : sizeof(request)-1;
     memcpy(request, p->payload, copy_len);
     request[copy_len] = '\0';
     tcp_recved(tpcb, p->tot_len);
  
     char line[256] = {0};
     char *token = strtok(request, "\r\n");
     if (token) {
         strncpy(line, token, sizeof(line)-1);
     } else {
         pbuf_free(p);
         return ERR_OK;
     }
     if (strncmp(line, "GET", 3) != 0) {
         pbuf_free(p);
         return ERR_OK;
     }
  
     char floor_str[8] = "";
     char action[16] = "";
     parse_query_params(line, floor_str, sizeof(floor_str), action, sizeof(action));
     if (floor_str[0] != '\0') {
         selected_floor = atoi(floor_str);
     }
     if (action[0] != '\0') {
         update_occupancy(floor_str, action);
     }
  
     char response[2048] = {0};
     create_html_page(response, sizeof(response));
     err_t write_err = tcp_write(tpcb, response, strlen(response), TCP_WRITE_FLAG_COPY);
     if (write_err == ERR_OK) {
         tcp_sent(tpcb, sent_callback);
         tcp_output(tpcb);
     } else {
         printf("Erro ao escrever a resposta (err=%d), fechando conexao.\n", write_err);
         tcp_close(tpcb);
     }
     pbuf_free(p);
     return ERR_OK;
 }
  
 static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
     tcp_recv(newpcb, http_callback);
     return ERR_OK;
 }
  
 static void start_http_server(void) {
     struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
     if (!pcb) {
         printf("Erro ao criar PCB\n");
         return;
     }
     if (tcp_bind(pcb, IP_ANY_TYPE, HTTP_PORT) != ERR_OK) {
         printf("Erro ao ligar o servidor na porta %d\n", HTTP_PORT);
         return;
     }
     pcb = tcp_listen(pcb);
     tcp_accept(pcb, connection_callback);
     printf("Servidor HTTP rodando na porta %d...\n", HTTP_PORT);
 }
  
 /* ─── FUNÇÃO PRINCIPAL ───────────────────────────────────────────────────── */
 int main() {
     stdio_init_all();
     sleep_ms(10000);  // Aguarda 10s para estabilidade
     printf("Iniciando sistema de monitoramento\n");
  
     /* Inicializa o Wi‑Fi */
     if (cyw43_arch_init()) {
         printf("Erro ao inicializar o Wi-Fi\n");
         return 1;
     }
     const char *ap_ssid = "BitDog";
     const char *ap_pass = "12345678";
     cyw43_arch_enable_ap_mode(ap_ssid, ap_pass, CYW43_AUTH_WPA2_AES_PSK);
     printf("Access Point iniciado com sucesso. SSID: %s\n", ap_ssid);
  
     // Configuração de IP estático para o AP
     ip4_addr_t gw, mask;
     IP4_ADDR(ip_2_ip4(&gw), 192, 168, 4, 1);
     IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);
     dhcp_server_t dhcp_server;
     dhcp_server_init(&dhcp_server, &gw, &mask);
     dns_server_t dns_server;
     dns_server_init(&dns_server, &gw);
     printf("Wi-Fi no modo AP iniciado!\n");
  
     /* Configura os LEDs RGB */
     gpio_init(LED_R_PIN); gpio_set_dir(LED_R_PIN, GPIO_OUT);
     gpio_init(LED_G_PIN); gpio_set_dir(LED_G_PIN, GPIO_OUT);
     gpio_init(LED_B_PIN); gpio_set_dir(LED_B_PIN, GPIO_OUT);
     update_led_status();
  
     /* Configura os botões */
     gpio_init(BUTTON_A); gpio_set_dir(BUTTON_A, GPIO_IN); gpio_pull_up(BUTTON_A);
     gpio_init(BUTTON_B); gpio_set_dir(BUTTON_B, GPIO_IN); gpio_pull_up(BUTTON_B);
  
     /* Inicializa o I2C para o display OLED */
     i2c_init(I2C_PORT, SSD1306_I2C_CLK * 1000);
     gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
     gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
     gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
     gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
  
     /* Inicializa o display OLED (128x64) */
     if (!ssd1306_init_bm(&oled_instance, SSD1306_WIDTH, SSD1306_HEIGHT, false, SSD1306_I2C_ADDR, I2C_PORT)) {
         printf("Erro ao inicializar o OLED\n");
         return 1;
     }
     ssd1306_config(&oled_instance);
     update_oled_display();
  
     /* Inicia o servidor HTTP */
     start_http_server();
  
     /* Loop principal: Processa tarefas do Wi-Fi e atualiza a seleção via botões */
     while (true) {
     #if PICO_CYW43_ARCH_POLL
          cyw43_arch_poll();
          cyw43_arch_wait_for_work_until(make_timeout_time_ms(100));
     #else
          sleep_ms(100);
     #endif
          update_floor_selection();
     }
  
     cyw43_arch_deinit();
     return 0;
 }
 