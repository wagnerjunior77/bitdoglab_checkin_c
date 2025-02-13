/**
 * Código modificado para AP com DHCP/DNS no Pico W – com filtragem aprimorada
 * para reduzir a sobrecarga causada por requisições indesejadas.
 */

 #include "pico/cyw43_arch.h"
 #include "pico/stdlib.h"
 #include "lwip/tcp.h"
 
 #include "dhcpserver.h"   // Certifique-se de que estes arquivos estão no include path
 #include "dnsserver.h"
 
 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 #include <time.h>
 #include <ctype.h>
 #include <stdbool.h>
 
 // ─── CONFIGURAÇÕES DE HARDWARE ──────────────────────────────────────────────
 #define LED_R_PIN 13
 #define LED_G_PIN 11
 #define LED_B_PIN 12
 
 #define HTTP_PORT 80
 
 // ─── CONFIGURAÇÕES DO HISTÓRICO ──────────────────────────────────────────────
 #define MAX_LOG_ENTRIES 20
 
 static char log_entries[MAX_LOG_ENTRIES][64];
 static int log_index = 0;
 
 // Usuários válidos: JOAO, MARIA, CARLOS, VISITANTE
 static bool user_present[4] = { false, false, false, false };
 static const char* valid_users[4] = { "JOAO", "MARIA", "CARLOS", "VISITANTE" };
 
 // ─── FUNÇÕES AUXILIARES ───────────────────────────────────────────────────────
 
 // Extrai o valor de um parâmetro da requisição GET (procura em uma linha)
 bool extract_parameter(const char *line, const char *key, char *value, size_t max_len) {
     const char *start = strstr(line, key);
     if (!start) return false;
     start += strlen(key);
     if (*start != '=') return false;
     start++; // pula o '='
     size_t i = 0;
     while (*start && *start != '&' && *start != ' ' && i < max_len - 1) {
         value[i++] = *start++;
     }
     value[i] = '\0';
     return true;
 }
 
 // Atualiza o LED RGB: verde se houver algum usuário presente; vermelho caso contrário
 void update_led_status(void) {
     bool any = false;
     for (int i = 0; i < 4; i++) {
         if (user_present[i]) { any = true; break; }
     }
     if (any) {
         // Verde: R=0, G=1, B=0
         gpio_put(LED_R_PIN, 0);
         gpio_put(LED_G_PIN, 1);
         gpio_put(LED_B_PIN, 0);
     } else {
         // Vermelho: R=1, G=0, B=0
         gpio_put(LED_R_PIN, 1);
         gpio_put(LED_G_PIN, 0);
         gpio_put(LED_B_PIN, 0);
     }
 }
 
 // Alterna o check-in/check-out do usuário
 void toggle_checkin(const char *user) {
     char user_upper[16];
     strncpy(user_upper, user, sizeof(user_upper));
     user_upper[sizeof(user_upper) - 1] = '\0';
     for (char *p = user_upper; *p; ++p) {
         *p = toupper((unsigned char)*p);
     }
     int index = -1;
     for (int i = 0; i < 4; i++) {
         if (strcmp(user_upper, valid_users[i]) == 0) {
             index = i;
             break;
         }
     }
     if (index == -1) return; // Usuário inválido
 
     time_t now = time(NULL);
     struct tm *tm_info = localtime(&now);
     char timestamp[9];
     if (tm_info != NULL) {
         snprintf(timestamp, sizeof(timestamp), "%02d:%02d:%02d",
                  tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
     } else {
         snprintf(timestamp, sizeof(timestamp), "00:00:00");
     }
 
     if (user_present[index]) {
         user_present[index] = false;
         snprintf(log_entries[log_index], sizeof(log_entries[log_index]),
                  "%s saiu às %s", valid_users[index], timestamp);
         printf("%s saiu às %s\n", valid_users[index], timestamp);
     } else {
         user_present[index] = true;
         snprintf(log_entries[log_index], sizeof(log_entries[log_index]),
                  "%s entrou às %s", valid_users[index], timestamp);
         printf("%s entrou às %s\n", valid_users[index], timestamp);
     }
     log_index = (log_index + 1) % MAX_LOG_ENTRIES;
     update_led_status();
 }
 
 // Limpa o histórico e reseta o estado de presença
 void clear_log(void) {
     for (int i = 0; i < 4; i++) {
         user_present[i] = false;
     }
     for (int i = 0; i < MAX_LOG_ENTRIES; i++) {
         log_entries[i][0] = '\0';
     }
     log_index = 0;
     printf("Histórico de presença limpo!\n");
     update_led_status();
 }
 
 // Cria a página HTML de resposta
 void create_html_page(char *buffer, size_t buffer_size) {
     int present_count = 0;
     for (int i = 0; i < 4; i++) {
         if (user_present[i]) present_count++;
     }
     char log_html[512] = "";
     char *entries[5];
     int count = 0;
     for (int i = 0; i < MAX_LOG_ENTRIES; i++) {
         if (log_entries[i][0] != '\0') {
             entries[count++] = log_entries[i];
             if (count > 5) {
                 for (int j = 0; j < count - 1; j++) {
                     entries[j] = entries[j + 1];
                 }
                 count = 5;
             }
         }
     }
     for (int i = 0; i < count; i++) {
         strcat(log_html, "<li>");
         strcat(log_html, entries[i]);
         strcat(log_html, "</li>");
     }
     snprintf(buffer, buffer_size,
              "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
              "<!DOCTYPE html>"
              "<html>"
              "<head><meta charset=\"UTF-8\"><title>BitDogLab - Check-in</title></head>"
              "<body>"
              "<h2>Registro de Presença</h2>"
              "<p>Total de pessoas presentes: %d</p>"
              "<form action=\"/\" method=\"GET\">"
              "  <button name=\"user\" value=\"Joao\">João</button>"
              "  <button name=\"user\" value=\"Maria\">Maria</button>"
              "  <button name=\"user\" value=\"Carlos\">Carlos</button>"
              "  <button name=\"user\" value=\"Visitante\">Visitante</button>"
              "</form>"
              "<form action=\"/\" method=\"GET\">"
              "  <button name=\"clear\" value=\"true\">Limpar Histórico</button>"
              "</form>"
              "<h3>Histórico de Check-in</h3>"
              "<ul>%s</ul>"
              "</body>"
              "</html>\r\n",
              present_count, log_html);
 }
  
 // ─── IMPLEMENTAÇÃO DO SERVIDOR HTTP ─────────────────────────────────────────────
 
 static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
     if (p == NULL) {
          tcp_close(tpcb);
          return ERR_OK;
     }
     
     char request[1024] = {0};
     size_t copy_len = p->tot_len < sizeof(request) - 1 ? p->tot_len : sizeof(request) - 1;
     memcpy(request, p->payload, copy_len);
     request[copy_len] = '\0';
     
     // Obtenha a primeira linha da requisição
     char line[256] = {0};
     char *token = strtok(request, "\r\n");
     if (token) {
         strncpy(line, token, sizeof(line)-1);
     } else {
         pbuf_free(p);
         return ERR_OK;
     }
     
     // Filtra: processa apenas requisições que comecem com "GET"
     if (strncmp(line, "GET", 3) != 0) {
         pbuf_free(p);
         return ERR_OK;
     }
     
     // Verifique se a requisição é exatamente para a página principal ("GET / HTTP/1.1")
     // ou se contém os parâmetros "user=" ou "clear="
     if ( (strstr(line, "user=") == NULL && strstr(line, "clear=") == NULL) &&
          (strcmp(line, "GET / HTTP/1.1") != 0 && strcmp(line, "GET /") != 0) ) {
         // Para outras requisições, envia resposta 404 Not Found e fecha a conexão.
         const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
         tcp_write(tpcb, not_found, strlen(not_found), TCP_WRITE_FLAG_COPY);
         pbuf_free(p);
         return ERR_OK;
     }
     
     // Se a requisição contém "clear=true", limpe o log
     if (strstr(line, "clear=true") != NULL) {
          clear_log();
     } else {
          // Se contém "user=", extraia o parâmetro
          char user_param[16] = {0};
          if (extract_parameter(line, "user", user_param, sizeof(user_param))) {
              toggle_checkin(user_param);
          }
     }
     
     char response[2048] = {0};
     create_html_page(response, sizeof(response));
     tcp_write(tpcb, response, strlen(response), TCP_WRITE_FLAG_COPY);
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
     
     // Configura o modo Access Point (AP) com SSID e senha
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
     
     // Inicializa o servidor DNS (opcional, para redirecionar domínios)
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
     
     // Loop principal: mantenha o Wi-Fi ativo e processe o DHCP/DNS
     while (true) {
 #if PICO_CYW43_ARCH_POLL
         cyw43_arch_poll();
         cyw43_arch_wait_for_work_until(make_timeout_time_ms(100));
 #else
         sleep_ms(100);
 #endif
     }
     
     // Nunca alcançado neste exemplo:
     // dhcp_server_deinit(&dhcp_server);
     // dns_server_deinit(&dns_server);
     cyw43_arch_deinit();
     return 0;
 }
 