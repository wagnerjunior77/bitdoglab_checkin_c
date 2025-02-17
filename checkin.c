/**
 * Monitor de Ocupação de Prédio com OLED, Botões, Servidor HTTP via Wi‑Fi
 * e Matriz de LED 5x5 WS2812.
 *
 * Funcionalidades:
 *  - A interface web (HTTP) permite selecionar um andar (0 a 4) e enviar ações 
 *    ("add", "remove", "clear", "set" e "clear_all") para atualizar a ocupação.
 *    Quando a ação for "set", o usuário pode informar (via caixa de texto) quantas pessoas colocar.
 *    Quando a ação for "clear_all", todos os andares serão zerados.
 *  - O display OLED mostra o status do andar selecionado (ex.: "Terreo: X pessoas" 
 *    ou "Andar N: X pessoas").
 *  - Botões físicos (GPIO 5 e 6) permitem alterar a seleção de andar.
 *  - LEDs RGB individuais (GPIO 13, 11, 12) indicam se o andar está vazio (vermelho)
 *    ou ocupado (verde).
 *  - A matriz de LED WS2812 (25 LEDs, 5x5) mostra, para cada andar, a proporção da ocupação
 *    (usando 50 pessoas como referência – cada LED equivale a 10 pessoas).
 *  - O Wi‑Fi é inicializado em modo Access Point com servidores DHCP/DNS e um servidor HTTP
 *    responde às requisições.
 */

 #include "pico/stdlib.h"
 #include "hardware/i2c.h"
 #include "hardware/pio.h"
 #include "ws2812.pio.h"      // Cabeçalho gerado a partir do ws2812.pio
 #include "pico/binary_info.h"
 #include "pico/cyw43_arch.h"
 #include "lwip/tcp.h"
 #include "lwip/inet.h"
 #include "dhcpserver/dhcpserver.h"
 #include "dnsserver/dnsserver.h"
 
 // Drivers do display OLED – API baseada em ssd1306_t (BitDogLab)
 #include "ssd1306.h"       // Declarações, comandos e protótipos para o SSD1306
 #include "ssd1306_i2c.h"   // Implementação via I2C
 #include "ssd1306_font.h"  // Fonte utilizada pelo display
 
 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 #include <ctype.h>
 #include <stdbool.h>
 #include <time.h>
 
 /* ─── DEFINIÇÕES DE HARDWARE ───────────────────────────────────────────── */
 // LEDs RGB individuais
 #define LED_R_PIN 13
 #define LED_G_PIN 11
 #define LED_B_PIN 12
 
 // Botões para seleção de andar
 #define BUTTON_A 5   // decrementa andar
 #define BUTTON_B 6   // incrementa andar
 
 // Configurações do display OLED (128x64)
 #define SSD1306_WIDTH    128
 #define SSD1306_HEIGHT   64
 #ifndef SSD1306_I2C_ADDR
   #define SSD1306_I2C_ADDR 0x3C
 #endif
 #ifndef SSD1306_I2C_CLK
   #define SSD1306_I2C_CLK 400000
 #endif
 
 // Para o OLED, usamos I2C1 com pinos 14 (SDA) e 15 (SCL)
 #define I2C_PORT i2c1
 #define I2C_SDA  14
 #define I2C_SCL  15
 
 // Configurações da matriz de LED WS2812 (5x5)
 #define WS2812_PIN 7
 #define IS_RGBW false
 
 // Porta do servidor HTTP
 #define HTTP_PORT 80
 
 #ifndef CYW43_AUTH_WPA2_AES_PSK
   #define CYW43_AUTH_WPA2_AES_PSK 4
 #endif
 
 // Configurações de ocupação
 #define NUM_FLOORS     5
 #define MAX_OCCUPANCY  50  // controle via botões/HTTP
 // Para a matriz: 50 pessoas = linha completa de 5 LEDs (cada LED equivale a 10 pessoas)
 
 /* ─── VARIÁVEIS GLOBAIS ───────────────────────────────────────────── */
 static int occupancy[NUM_FLOORS] = {0, 0, 0, 0, 0};
 static int selected_floor = 0;
 
 // Objeto global para o display OLED
 ssd1306_t disp;
 
 // Variáveis para a matriz de LED WS2812
 PIO pio_ws;
 uint sm_ws;
 uint offset_ws;
 
 /* ─── FUNÇÕES AUXILIARES PARA PARÂMETROS HTTP ──────────────────────────*/
 // Função genérica para extrair um parâmetro da query string
 static void parse_param(const char *request_line, const char *key, char *dest, size_t dest_size) {
     dest[0] = '\0';
     const char *p = strstr(request_line, key);
     if (p) {
         p += strlen(key);
         size_t i = 0;
         while (*p && *p != '&' && *p != ' ' && i < dest_size - 1) {
             dest[i++] = *p++;
         }
         dest[i] = '\0';
     }
 }
 
 /* Protótipos */
 static void parse_query_params(const char *request_line, char *floor_str, size_t floor_len, char *action, size_t action_len, char *value_str, size_t value_len);
 void update_led_matrix(void);
 
 /* ─── FUNÇÕES AUXILIARES ───────────────────────────────────────────── */
 // Configura o display OLED e os pinos I2C
 void setup_display() {
     i2c_init(I2C_PORT, SSD1306_I2C_CLK);
     gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
     gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
     gpio_pull_up(I2C_SDA);
     gpio_pull_up(I2C_SCL);
 
     disp.external_vcc = false;
     if (!ssd1306_init(&disp, SSD1306_WIDTH, SSD1306_HEIGHT, SSD1306_I2C_ADDR, I2C_PORT)) {
          printf("Erro ao inicializar o OLED\n");
     }
     ssd1306_clear(&disp);
 }
 
 // Função para mostrar uma mensagem no OLED
 void mostrar_mensagem(char *str, uint32_t x, uint32_t y, bool should_clear) {
     if (should_clear) {
          ssd1306_clear(&disp);
     }
     sleep_ms(50);
     ssd1306_draw_string(&disp, x, y, 1, str);
     ssd1306_show(&disp);
 }
 
 // Atualiza os LEDs RGB individuais conforme a ocupação do andar selecionado
 void update_led_status(void) {
    if (occupancy[selected_floor] == 0) {
         gpio_put(LED_R_PIN, 1);  // acende o LED vermelho
    } else {
         gpio_put(LED_R_PIN, 0);  // apaga o LED vermelho
    }
    // Desliga os outros LEDs (verde e azul)
    gpio_put(LED_G_PIN, 0);
    gpio_put(LED_B_PIN, 0);
}
 
 // Atualiza o display OLED com o status do andar selecionado
 void update_oled_display(void) {
     char buf[64];
     ssd1306_clear(&disp);
     if (selected_floor == 0)
          snprintf(buf, sizeof(buf), "Terreo: %d pessoas", occupancy[selected_floor]);
     else
          snprintf(buf, sizeof(buf), "Andar %d: %d pessoas", selected_floor, occupancy[selected_floor]);
     ssd1306_draw_string(&disp, 0, 0, 1, buf);
     ssd1306_show(&disp);
 }
 
 // Lê o estado de um botão (com pull‑up: retorna 1 se pressionado)
 int read_button(uint pin) {
     return (gpio_get(pin) == 0);
 }
 
 // Atualiza a seleção de andar via botões
 void update_floor_selection(void) {
     if (read_button(BUTTON_B)) {
          selected_floor = (selected_floor + 1) % NUM_FLOORS;
          update_oled_display();
          update_led_status();
          update_led_matrix();
          sleep_ms(300); // debounce
     }
     if (read_button(BUTTON_A)) {
          selected_floor = (selected_floor - 1 + NUM_FLOORS) % NUM_FLOORS;
          update_oled_display();
          update_led_status();
          update_led_matrix();
          sleep_ms(300); // debounce
     }
 }
 
 // Atualiza a ocupação; suporta ações "add", "remove", "clear", "set" e "clear_all"
 // Quando a ação for "set", usa o valor passado em value_str.
 void update_occupancy(const char *floor_str, const char *action, const char *value_str) {
     if (strcmp(action, "clear_all") == 0) {
          for (int i = 0; i < NUM_FLOORS; i++) {
               occupancy[i] = 0;
          }
     } else {
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
          } else if (strcmp(action, "set") == 0) {
               occupancy[floor] = atoi(value_str);
          }
     }
     printf("Andar %d: nova ocupacao = %d\n", selected_floor, occupancy[selected_floor]);
     update_led_status();
     update_oled_display();
     update_led_matrix();
 }
 
 /* Função para extrair parâmetros da query string.
    Extrai os parâmetros "floor", "action" e "value". */
 static void parse_query_params(const char *request_line, char *floor_str, size_t floor_len,
                                  char *action, size_t action_len, char *value_str, size_t value_len) {
     parse_param(request_line, "floor=", floor_str, floor_len);
     parse_param(request_line, "action=", action, action_len);
     parse_param(request_line, "value=", value_str, value_len);
 }
 
 /* ─── GERA A PÁGINA HTML ───────────────────────────────────────────── */
 void create_html_page(char *buffer, size_t buffer_size) {
     char body[2048] = "";
     strcat(body, "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Monitor de Ocupacao</title>");
     strcat(body, "<style>table, th, td { border: 1px solid black; border-collapse: collapse; padding: 8px; }</style>");
     strcat(body, "<meta http-equiv=\"Cache-Control\" content=\"no-store\"/>");
     strcat(body, "</head><body>");
     strcat(body, "<h1>Monitor de Ocupacao do Predio</h1>");
     
     // Formulário para modificar a ocupação
     strcat(body, "<form action=\"/\" method=\"GET\">");
     strcat(body, "<label for=\"floor\">Selecione o Andar:</label>");
     strcat(body, "<select name=\"floor\" id=\"floor\">");
     for (int i = 0; i < NUM_FLOORS; i++) {
          char option[64];
          if(i == 0)
               snprintf(option, sizeof(option), "<option value=\"%d\" %s>Terreo</option>", i, (i==selected_floor) ? "selected" : "");
          else
               snprintf(option, sizeof(option), "<option value=\"%d\" %s>Andar %d</option>", i, (i==selected_floor) ? "selected" : "", i);
          strcat(body, option);
     }
     strcat(body, "</select><br/><br/>");
     strcat(body, "<input type=\"submit\" name=\"action\" value=\"add\"> ");
     strcat(body, "<input type=\"submit\" name=\"action\" value=\"remove\"> ");
     strcat(body, "<input type=\"submit\" name=\"action\" value=\"clear\"> ");
     strcat(body, "<input type=\"submit\" name=\"action\" value=\"clear_all\"> <br/><br/>");
     strcat(body, "Ou defina a ocupacao: <input type=\"text\" name=\"value\" placeholder=\"Numero\"> ");
     strcat(body, "<input type=\"submit\" name=\"action\" value=\"set\">");
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
 
 /* ─── FUNÇÕES DO SERVIDOR HTTP ───────────────────────────────────────── */
 // Callback chamada após envio completo da resposta HTTP (fecha a conexão)
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
  
 // Callback HTTP: processa a requisição GET e atualiza a ocupação se os parâmetros estiverem presentes
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
     char value_str[8] = "";
     parse_query_params(line, floor_str, sizeof(floor_str), action, sizeof(action), value_str, sizeof(value_str));
     if (floor_str[0] != '\0' && strcmp(action, "clear_all") != 0) {
          selected_floor = atoi(floor_str);
     }
     if (action[0] != '\0') {
          update_occupancy(floor_str, action, value_str);
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
  
 /* ─── FUNÇÕES PARA A MATRIZ DE LED WS2812 ───────────────────────────── */
 // Envia a cor para um LED WS2812 (dados no formato GRB, deslocados 8 bits à esquerda)
 static inline void put_pixel(PIO pio, uint sm, uint32_t pixel_grb) {
     pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
 }
 // Converte componentes R, G, B para um único valor de 24 bits (formato GRB)
 static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
     return ((uint32_t)r << 8) | ((uint32_t)g << 16) | (uint32_t)b;
 }
 // Atualiza a matriz de LED WS2812 (5x5)
// Cada LED equivale a 10 pessoas.

void update_led_matrix(void) {
    uint32_t pixels[25];
    for (int floor = 0; floor < NUM_FLOORS; floor++) {
        uint8_t leds_lit = 0;
        // Se há ocupação entre 1 e 9, acende apenas 1 LED (light blue).
        if (occupancy[floor] > 0 && occupancy[floor] < 10) {
            leds_lit = 1;
        } else if (occupancy[floor] >= 10) {
            leds_lit = occupancy[floor] / 10;
            if (leds_lit > 5) leds_lit = 5;
        }
        for (int col = 0; col < 5; col++) {
            int index;
            // Para pisos pares, inverte a ordem dos LEDs na linha.
            if (floor % 2 == 0) {
                index = floor * 5 + (4 - col);
            } else {
                index = floor * 5 + col;
            }
            if (occupancy[floor] > 0 && occupancy[floor] < 10) {
                // Apenas o primeiro LED (na ordem definida) aceso em verde.
                if (col == 0)
                    pixels[index] = urgb_u32(0, 5, 0); // green
                else
                    pixels[index] = urgb_u32(0, 0, 0);
            } else {
                // Para ocupações ≥10, acende os primeiros 'leds_lit' LEDs em vermelho.
                if (col < leds_lit)
                    pixels[index] = urgb_u32(5, 0, 0); // vermelho
                else
                    pixels[index] = urgb_u32(0, 0, 0);
            }
        }
    }
    // Envia os 25 pixels para a cadeia WS2812
    for (int i = 0; i < 25; i++) {
         put_pixel(pio_ws, sm_ws, pixels[i]);
    }
    sleep_us(50);
}
  
 /* ─── FUNÇÃO PRINCIPAL ───────────────────────────────────────────── */
 int main() {
     stdio_init_all();
     sleep_ms(10000);  // Aguarda 10s para estabilidade
     printf("Iniciando sistema!\n");
  
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
  
     /* Configura os LEDs RGB individuais */
     gpio_init(LED_R_PIN); gpio_set_dir(LED_R_PIN, GPIO_OUT);
     gpio_init(LED_G_PIN); gpio_set_dir(LED_G_PIN, GPIO_OUT);
     gpio_init(LED_B_PIN); gpio_set_dir(LED_B_PIN, GPIO_OUT);
     update_led_status();
  
     /* Configura os botões */
     gpio_init(BUTTON_A); gpio_set_dir(BUTTON_A, GPIO_IN); gpio_pull_up(BUTTON_A);
     gpio_init(BUTTON_B); gpio_set_dir(BUTTON_B, GPIO_IN); gpio_pull_up(BUTTON_B);
  
     /* Inicializa o I2C para o display OLED (usando I2C1, SDA=14, SCL=15) */
     i2c_init(I2C_PORT, SSD1306_I2C_CLK);
     gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
     gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
     gpio_pull_up(I2C_SDA);
     gpio_pull_up(I2C_SCL);
     setup_display();
  
     /* Teste inicial: exibe um texto de teste por 5 segundos */
     mostrar_mensagem("Iniciando sistema!", 0, 0, true);
     sleep_ms(5000);
     // Após o teste, exibe o status inicial (ocupação 0)
     update_oled_display();
  
     /* Inicializa a matriz de LED WS2812 via PIO */
     pio_ws = pio0;
     sm_ws = pio_claim_unused_sm(pio_ws, true);
     offset_ws = pio_add_program(pio_ws, &ws2812_program);
     ws2812_program_init(pio_ws, sm_ws, offset_ws, WS2812_PIN, 800000, IS_RGBW);
     update_led_matrix();
  
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
 