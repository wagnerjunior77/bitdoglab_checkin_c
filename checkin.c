/**
 * Código para monitoramento de ocupação de um prédio de 5 andares
 * utilizando o RP2040 (Pico W) com servidor HTTP via lwIP.
 *
 * A interface HTML permite:
 *  - Selecionar um andar (0 a 4) por meio de um dropdown.
 *  - Clicar em "add", "remove" ou "clear" para modificar a ocupação do andar selecionado.
 *  - Exibir, logo abaixo, o status (quantas pessoas) do andar selecionado.
 *
 * O LED RGB é atualizado: fica verde se o andar selecionado tiver pelo menos 1 pessoa,
 * ou vermelho se estiver vazio.
 */

 #include "pico/cyw43_arch.h"
 #include "pico/stdlib.h"
 #include "lwip/tcp.h"
 #include "lwip/inet.h"   
 #include "dhcpserver.h"  // Certifique-se de que estes arquivos estão no include path
 #include "dnsserver.h"
 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 #include <ctype.h>
 #include <stdbool.h>
 #include <time.h>
 
 // ─── CONFIGURAÇÕES DE HARDWARE ──────────────────────────────────────────────
 #define LED_R_PIN 13
 #define LED_G_PIN 11
 #define LED_B_PIN 12
 
 #define HTTP_PORT 80
 
 // ─── CONFIGURAÇÕES DE OCUPAÇÃO ──────────────────────────────────────────────
 #define NUM_FLOORS 5       // Andares 0 a 4
 #define MAX_OCCUPANCY 50   // Limite de ocupação por andar
 
 static int occupancy[NUM_FLOORS] = {0, 0, 0, 0, 0};  // Contagem de pessoas por andar
 static int selected_floor = 0;  // Andar atualmente selecionado
 
 // ─── FUNÇÕES AUXILIARES ───────────────────────────────────────────────────────
 
 /**
  * Extrai os parâmetros "floor" e "action" da query string da requisição.
  */
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
 
 /**
  * Atualiza o LED RGB de acordo com a ocupação do andar selecionado.
  * - Se occupancy[selected_floor] > 0: LED verde.
  * - Se occupancy[selected_floor] == 0: LED vermelho.
  */
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
 
 /**
  * Atualiza a ocupação do andar especificado (via parâmetro "floor")
  * conforme a ação (add, remove ou clear). Também atualiza o andar selecionado.
  */
 void update_occupancy(const char *floor_str, const char *action) {
     int floor = atoi(floor_str);
     if (floor < 0 || floor >= NUM_FLOORS) return;
     
     // Atualiza o andar selecionado para efeito do status e LED
     selected_floor = floor;
     
     if (strcmp(action, "add") == 0) {
         if (occupancy[floor] < MAX_OCCUPANCY) occupancy[floor]++;
     } else if (strcmp(action, "remove") == 0) {
         if (occupancy[floor] > 0) occupancy[floor]--;
     } else if (strcmp(action, "clear") == 0) {
         occupancy[floor] = 0;
     }
     printf("Andar %d: nova ocupação = %d\n", floor, occupancy[floor]);
     update_led_status();
 }
 
 /**
  * Gera a página HTML. A página apresenta:
  *  - Um formulário com um dropdown para selecionar o andar.
  *  - Botões para as ações: add, remove e clear.
  *  - Um parágrafo que exibe a ocupação atual do andar selecionado.
  */
 void create_html_page(char *buffer, size_t buffer_size) {
     char body[2048] = "";
     strcat(body, "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Monitor de Ocupação</title>");
     strcat(body, "<meta http-equiv=\"Cache-Control\" content=\"no-store\"/>");
     strcat(body, "</head><body>");
     strcat(body, "<h1>Monitor de Ocupação do Prédio</h1>");
     
     // Formulário
     strcat(body, "<form action=\"/\" method=\"GET\">");
     strcat(body, "<label for=\"floor\">Selecione o Andar:</label>");
     strcat(body, "<select name=\"floor\" id=\"floor\">");
     for (int i = 0; i < NUM_FLOORS; i++) {
         char option[64];
         if (i == selected_floor) {
             snprintf(option, sizeof(option), "<option value=\"%d\" selected>Andar %d</option>", i, i);
         } else {
             snprintf(option, sizeof(option), "<option value=\"%d\">Andar %d</option>", i, i);
         }
         strcat(body, option);
     }
     strcat(body, "</select><br/><br/>");
     strcat(body, "<input type=\"submit\" name=\"action\" value=\"add\"> ");
     strcat(body, "<input type=\"submit\" name=\"action\" value=\"remove\"> ");
     strcat(body, "<input type=\"submit\" name=\"action\" value=\"clear\"> ");
     strcat(body, "</form>");
     
     // Status do andar selecionado
     char status_line[128];
     snprintf(status_line, sizeof(status_line), "<p>Andar %d: %d pessoas</p>", selected_floor, occupancy[selected_floor]);
     strcat(body, status_line);
     
     strcat(body, "</body></html>");
     
     snprintf(buffer, buffer_size,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/html; charset=UTF-8\r\n"
              "Connection: close\r\n\r\n%s", body);
 }
  
 // ─── Callback para envio completo – fecha a conexão ───────
 static err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
     printf("Resposta enviada, fechando conexão.\n");
     tcp_arg(tpcb, NULL);
     tcp_recv(tpcb, NULL);
     tcp_sent(tpcb, NULL);
     err_t err = tcp_close(tpcb);
     if (err != ERR_OK) {
         printf("Erro ao fechar conexão (err=%d), abortando.\n", err);
         tcp_abort(tpcb);
     }
     return ERR_OK;
 }
  
 // ─── IMPLEMENTAÇÃO DO SERVIDOR HTTP ─────────────────────────────────────────────
 static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
     if (p == NULL) {
          tcp_close(tpcb);
          return ERR_OK;
     }
     
     char request[1024] = {0};
     size_t copy_len = (p->tot_len < sizeof(request) - 1) ? p->tot_len : sizeof(request) - 1;
     memcpy(request, p->payload, copy_len);
     request[copy_len] = '\0';
     
     // Libera os bytes recebidos para liberar o "window"
     tcp_recved(tpcb, p->tot_len);
     
     // Obtenha a primeira linha da requisição
     char line[256] = {0};
     char *token = strtok(request, "\r\n");
     if (token) {
         strncpy(line, token, sizeof(line) - 1);
     } else {
         pbuf_free(p);
         return ERR_OK;
     }
     
     // Processa apenas requisições que começam com "GET"
     if (strncmp(line, "GET", 3) != 0) {
         pbuf_free(p);
         return ERR_OK;
     }
     
     // Extrai os parâmetros "floor" e "action", se presentes
     char floor_str[8] = "";
     char action[16] = "";
     parse_query_params(line, floor_str, sizeof(floor_str), action, sizeof(action));
     
     // Se floor for especificado, atualiza o andar selecionado
     if (floor_str[0] != '\0') {
         selected_floor = atoi(floor_str);
     }
     // Se action for fornecida, atualiza a ocupação do andar
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
         printf("Erro ao escrever a resposta (err=%d), fechando conexão.\n", write_err);
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
  
 // ─── FUNÇÃO PRINCIPAL ───────────────────────────────────────────────────────────
 int main() {
     stdio_init_all();
     sleep_ms(10000);
     printf("Iniciando servidor HTTP\n");
     
     // Inicializa o Wi-Fi
     if (cyw43_arch_init()) {
         printf("Erro ao inicializar o Wi-Fi\n");
         return 1;
     }
     
     // --- (Sem sincronização de horário neste exemplo) ---
     
     // Ativa o modo Access Point
     const char *ap_ssid = "BitDog";
     const char *ap_pass = "12345678";  // senha com pelo menos 8 caracteres
     cyw43_arch_enable_ap_mode(ap_ssid, ap_pass, CYW43_AUTH_WPA2_AES_PSK);
     printf("Access Point iniciado com sucesso. SSID: %s\n", ap_ssid);
     
     // Configure IP estático para o AP: gateway 192.168.4.1, máscara 255.255.255.0
     ip4_addr_t gw, mask;
     IP4_ADDR(ip_2_ip4(&gw), 192, 168, 4, 1);
     IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);
     
     // Inicializa o servidor DHCP para fornecer IPs aos clientes
     dhcp_server_t dhcp_server;
     dhcp_server_init(&dhcp_server, &gw, &mask);
     
     // Inicializa o servidor DNS (opcional)
     dns_server_t dns_server;
     dns_server_init(&dns_server, &gw);
     
     printf("Wi-Fi no modo AP iniciado!\n");
     
     // Configura os pinos do LED RGB
     gpio_init(LED_R_PIN);
     gpio_set_dir(LED_R_PIN, GPIO_OUT);
     gpio_init(LED_G_PIN);
     gpio_set_dir(LED_G_PIN, GPIO_OUT);
     gpio_init(LED_B_PIN);
     gpio_set_dir(LED_B_PIN, GPIO_OUT);
     update_led_status();
     
     // Inicia o servidor HTTP
     start_http_server();
     
     // Loop principal: mantém o Wi-Fi ativo e processa DHCP/DNS
     while (true) {
 #if PICO_CYW43_ARCH_POLL
         cyw43_arch_poll();
         cyw43_arch_wait_for_work_until(make_timeout_time_ms(100));
 #else
         sleep_ms(100);
 #endif
     }
     
     cyw43_arch_deinit();
     return 0;
 }
 