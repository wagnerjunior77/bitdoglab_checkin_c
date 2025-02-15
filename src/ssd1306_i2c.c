#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "ssd1306_i2c.h"
#include "ssd1306_font.h"

#ifndef count_of
#define count_of(x) (sizeof(x) / sizeof((x)[0]))
#endif

void calc_render_area_buflen(struct render_area *area) {
    area->buflen = (area->end_col - area->start_col + 1) * (area->end_page - area->start_page + 1);
}

void SSD1306_send_cmd(uint8_t cmd) {
    uint8_t buffer[2] = { SSD1306_CONTROL_CMD, cmd };
    i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, buffer, 2, false);
}

void SSD1306_send_cmd_list(uint8_t *buf, int num) {
    for (int i = 0; i < num; i++) {
        SSD1306_send_cmd(buf[i]);
    }
}

void SSD1306_send_buf(uint8_t *buf, int buflen) {
    uint8_t *temp_buf = malloc(buflen + 1);
    if (!temp_buf) return;
    temp_buf[0] = SSD1306_CONTROL_DATA;
    memcpy(temp_buf + 1, buf, buflen);
    i2c_write_blocking(i2c_default, SSD1306_I2C_ADDR, temp_buf, buflen + 1, false);
    free(temp_buf);
}

void SSD1306_scroll(bool on) {
    uint8_t cmds[] = {
        SSD1306_SET_HORIZ_SCROLL | 0x00,
        0x00, // dummy
        0x00, // start page 0
        0x00, // time interval
        0x03, // end page (ajuste se necessário)
        0x00, // dummy
        0xFF, // dummy
        SSD1306_SET_SCROLL | (on ? 0x01 : 0)
    };
    SSD1306_send_cmd_list(cmds, count_of(cmds));
}

void render(uint8_t *buf, struct render_area *area) {
    uint8_t cmds[] = {
        SSD1306_SET_COL_ADDR,
        area->start_col,
        area->end_col,
        SSD1306_SET_PAGE_ADDR,
        area->start_page,
        area->end_page
    };
    SSD1306_send_cmd_list(cmds, count_of(cmds));
    SSD1306_send_buf(buf, area->buflen);
}

// Funções da nossa API para o driver SSD1306

bool ssd1306_init_bm(ssd1306_t *p, uint8_t width, uint8_t height, bool external_vcc, uint8_t address, i2c_inst_t *i2c_port) {
    if (!p) return false;
    p->width = width;
    p->height = height;
    p->pages = height / SSD1306_PAGE_HEIGHT;
    p->address = address;
    p->i2c_port = i2c_port;
    p->external_vcc = external_vcc;
    p->bufsize = (p->pages * p->width) + 1;
    p->ram_buffer = calloc(p->bufsize, sizeof(uint8_t));
    if (!p->ram_buffer) return false;
    p->ram_buffer[0] = SSD1306_CONTROL_DATA;
    return true;
}

void ssd1306_config(ssd1306_t *p) {
    uint8_t cmds[18];
    int i = 0;
    cmds[i++] = SSD1306_SET_DISP;               // Display off
    cmds[i++] = SSD1306_SET_MEM_MODE;           // Memory mode
    cmds[i++] = 0x00;                           // Horizontal addressing
    cmds[i++] = SSD1306_SET_DISP_START_LINE;    // Start line 0
    cmds[i++] = SSD1306_SET_SEG_REMAP | 0x01;     // Segment remap
    cmds[i++] = SSD1306_SET_MUX_RATIO;          // Multiplex ratio
    cmds[i++] = p->height - 1;
    cmds[i++] = SSD1306_SET_COM_OUT_DIR | 0x08;   // COM scan direction
    cmds[i++] = SSD1306_SET_DISP_OFFSET;          // Display offset
    cmds[i++] = 0x00;                           // No offset
    cmds[i++] = SSD1306_SET_COM_PIN_CFG;          // COM pin configuration
    cmds[i++] = (p->height == 32) ? 0x02 : 0x12;  // Depende do modelo
    cmds[i++] = SSD1306_SET_DISP_CLK_DIV;         // Clock divide ratio
    cmds[i++] = 0x80;
    cmds[i++] = SSD1306_SET_PRECHARGE;            // Precharge period
    cmds[i++] = 0xF1;
    cmds[i++] = SSD1306_SET_VCOM_DESEL;           // VCOMH deselect level
    cmds[i++] = 0x30;
    cmds[i++] = SSD1306_SET_CONTRAST;             // Contrast
    cmds[i++] = 0xFF;
    cmds[i++] = SSD1306_SET_ENTIRE_ON;            // Entire display on (follow RAM)
    // Envia a lista de comandos
    SSD1306_send_cmd_list(cmds, i);
    // Liga o display
    SSD1306_send_cmd(SSD1306_SET_DISP | 0x01);
}

void ssd1306_clear(ssd1306_t *p) {
    memset(p->ram_buffer + 1, 0, p->bufsize - 1);
}

void ssd1306_show(ssd1306_t *p) {
    uint8_t cmds[6];
    cmds[0] = SSD1306_SET_COL_ADDR;
    cmds[1] = 0;
    cmds[2] = p->width - 1;
    cmds[3] = SSD1306_SET_PAGE_ADDR;
    cmds[4] = 0;
    cmds[5] = p->pages - 1;
    SSD1306_send_cmd_list(cmds, 6);
    SSD1306_send_buf(p->ram_buffer + 1, p->bufsize - 1);
}

// Funções de desenho de texto usando a fonte embutida
// (Ignoramos o parâmetro scale para simplificar)
static void WriteChar(ssd1306_t *p, int16_t x, int16_t y, char ch) {
    if (x > p->width - 8 || y > p->height - 8)
        return;
    y = y / 8;
    ch = toupper(ch);
    int idx;
    if (ch >= 'A' && ch <= 'Z') {
        idx = ch - 'A' + 1;
    } else if (ch >= '0' && ch <= '9') {
        idx = ch - '0' + 27;
    } else {
        idx = 0;
    }
    int fb_idx = y * p->width + x + 1; // +1 p/ pular o byte de controle
    for (int i = 0; i < 8; i++) {
        p->ram_buffer[fb_idx++] = font[idx * 8 + i];
    }
}

void ssd1306_draw_string(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const char *s) {
    while (*s) {
        WriteChar(p, x, y, *s++);
        x += 8 * scale;
    }
}
