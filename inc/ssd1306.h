#ifndef SSD1306_H
#define SSD1306_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"

// Tamanho do display – ajuste para 128×32 ou 128×64 conforme seu display:
#ifndef SSD1306_WIDTH
    #define SSD1306_WIDTH 128
#endif

#ifndef SSD1306_HEIGHT
    #define SSD1306_HEIGHT 64
#endif

// Endereço I2C do display (geralmente 0x3C)
#define SSD1306_I2C_ADDR 0x3C

// Frequência do clock I2C (em kHz)
#ifndef SSD1306_I2C_CLK
    #define SSD1306_I2C_CLK 400
#endif

// Comandos do SSD1306 (conforme datasheet)
#define SSD1306_SET_MEM_MODE        0x20
#define SSD1306_SET_COL_ADDR        0x21
#define SSD1306_SET_PAGE_ADDR       0x22
#define SSD1306_SET_HORIZ_SCROLL    0x26
#define SSD1306_SET_SCROLL          0x2E
#define SSD1306_SET_DISP_START_LINE 0x40
#define SSD1306_SET_CONTRAST        0x81
#define SSD1306_SET_CHARGE_PUMP     0x8D
#define SSD1306_SET_SEG_REMAP       0xA0
#define SSD1306_SET_ENTIRE_ON       0xA4
#define SSD1306_SET_ALL_ON          0xA5
#define SSD1306_SET_NORM_DISP       0xA6
#define SSD1306_SET_INV_DISP        0xA7
#define SSD1306_SET_MUX_RATIO       0xA8
#define SSD1306_SET_DISP            0xAE
#define SSD1306_SET_COM_OUT_DIR     0xC0
#define SSD1306_SET_DISP_OFFSET     0xD3
#define SSD1306_SET_DISP_CLK_DIV    0xD5
#define SSD1306_SET_PRECHARGE       0xD9
#define SSD1306_SET_COM_PIN_CFG     0xDA
#define SSD1306_SET_VCOM_DESEL      0xDB

// Cálculo de páginas e tamanho do buffer
#define SSD1306_PAGE_HEIGHT         8
#define SSD1306_NUM_PAGES           (SSD1306_HEIGHT / SSD1306_PAGE_HEIGHT)
#define SSD1306_BUF_LEN             (SSD1306_NUM_PAGES * SSD1306_WIDTH + 1)

// Bytes de controle para I2C
#define SSD1306_CONTROL_CMD         0x80
#define SSD1306_CONTROL_DATA        0x40

// Estrutura que guarda a configuração do display
typedef struct {
    uint8_t width;         // Largura em pixels
    uint8_t height;        // Altura em pixels
    uint8_t pages;         // Número de páginas (height / 8)
    uint8_t address;       // Endereço I2C
    i2c_inst_t *i2c_port;  // Instância do I2C (geralmente i2c_default)
    bool external_vcc;     // Se usa alimentação externa (normalmente false)
    uint8_t *ram_buffer;   // Buffer de vídeo (tamanho: (pages * width) + 1)
    size_t bufsize;        // Tamanho do buffer (incluindo 1 byte de controle)
    uint8_t port_buffer[2];// Buffer auxiliar para comandos
} ssd1306_t;

/**
 * @brief Inicializa o display em modo bitmap.
 * @param p Ponteiro para a instância do display.
 * @param width Largura em pixels.
 * @param height Altura em pixels.
 * @param external_vcc Se o display usa alimentação externa.
 * @param address Endereço I2C do display.
 * @param i2c_port Instância do I2C.
 * @return true se inicializou com sucesso, false caso contrário.
 */
bool ssd1306_init_bm(ssd1306_t *p, uint8_t width, uint8_t height, bool external_vcc, uint8_t address, i2c_inst_t *i2c_port);

/**
 * @brief Envia a sequência de configuração para o display.
 * @param p Ponteiro para a instância do display.
 */
void ssd1306_config(ssd1306_t *p);

/**
 * @brief Limpa o buffer de vídeo (zera os pixels).
 * @param p Ponteiro para a instância do display.
 */
void ssd1306_clear(ssd1306_t *p);

/**
 * @brief Envia o buffer de vídeo para o display (atualiza a tela).
 * @param p Ponteiro para a instância do display.
 */
void ssd1306_show(ssd1306_t *p);

/**
 * @brief Desenha uma string usando a fonte embutida.
 * @param p Ponteiro para a instância do display.
 * @param x Posição horizontal em pixels.
 * @param y Posição vertical em pixels.
 * @param scale Fator de escala (1 = tamanho normal).
 * @param s String a ser desenhada.
 */
void ssd1306_draw_string(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const char *s);

#endif // SSD1306_H
