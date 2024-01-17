/*
 * ATmega_GLCD_TextFont
 * http://electronicwings.com
 */
#define F_CPU 8000000UL

#include <avr/io.h> /* Include AVR std. library file */
#include <stdio.h>  /* Include std i/o library file */
#include <stdlib.h>
#include <util/delay.h> /* Include delay header file */

/* Define CPU clock Freq 8MHz */
#define Data_Port PORTC

#define RS PA2 /* Define control pins */
#define RW PA3
#define EN PD6
#define CS1 PB0
#define CS2 PB1
#define RST PD7

#define TotalPage 8

void glcd_Init(void);
void GLCD_Command(char Command);
void GLCD_Data(char Data);
void printCursor(unsigned char x, unsigned char y, unsigned char on);
void GLCD_Change(int x);
void glcd_SetCursor(uint8_t x, uint8_t y);

void adc_init();

uint32_t readTouchX(void);
uint32_t readTouchY(void);

void clearBuffer();
void renderBuffer();
void drawPixel(uint8_t x, uint8_t y);
void fillRound(uint8_t x, uint8_t y, uint8_t radius);
void drawCircle(uint8_t cx, uint8_t cy, uint8_t radius);
void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2);

uint8_t fullRotate(uint8_t value);

uint64_t buffer[64];

int main(void) {
    glcd_Init(); /* Initialize GLCD */
    adc_init();
    _delay_ms(2);
    GLCD_Change(1);
    clearBuffer();

    int velX = 1;
    int velY = 2;
    int x = 6;
    int y = 30;

    while (1) {
        clearBuffer();

        drawLine(0, 0, 0, 63);

        for (int8_t i = 0; i < 3; i++)
            for (int8_t j = 0; j < 3; j++) {
                drawCircle(16 + i * 16, 16 + j * 16, 5);
                fillRound(16 + i * 16, 16 + j * 16, 1);
            }

        uint32_t mouseX = readTouchX();
        uint32_t mouseY = readTouchY();

        fillRound(mouseX, mouseY, 5);

        if (x - 4 <= 0 || x + 4 >= 63) velX *= -1;
        if (y - 4 <= 0 || y + 4 >= 63) velY *= -1;

        x += velX;
        y += velY;

        drawLine(0, 32, x, y);
        fillRound(0, 32, 3);
        fillRound(x, y, 4);

        renderBuffer();
        _delay_us(50);
    }

    return 0;
}

void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);

    int8_t sx = x1 < x2 ? 1 : -1;
    int8_t sy = y1 < y2 ? 1 : -1;
    int8_t err = dx - dy;

    while (x1 != x2 || y1 != y2) {
        drawPixel(x1, y1);

        int16_t e2 = err * 2;

        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }

        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }

    drawPixel(x2, y2);
}

void drawCircle(uint8_t cx, uint8_t cy, uint8_t radius) {
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        drawPixel(cx + x, cy + y);
        drawPixel(cx - x, cy + y);
        drawPixel(cx + x, cy - y);
        drawPixel(cx - x, cy - y);

        drawPixel(cx + y, cy + x);
        drawPixel(cx - y, cy + x);
        drawPixel(cx + y, cy - x);
        drawPixel(cx - y, cy - x);

        y++;
        err += 1 + 2 * y;

        if (2 * (err - x) + 1 > 0) {
            x--;
            err += 1 - 2 * x;
        }
    }
}

void fillRound(uint8_t x, uint8_t y, uint8_t radius) {
    for (int8_t i = -radius; i < radius; i++)
        for (int8_t j = -radius; j < radius; j++)
            if (i * i + j * j < radius * radius)
                drawPixel(x + i, y + j);
}

void drawPixel(uint8_t x, uint8_t y) {
    uint64_t temp = buffer[y] | ((uint64_t)(1) << x);
    buffer[y] = temp;
}

void renderBuffer() {
    for (uint8_t y = 0; y < 64; y += 8)
        for (uint8_t x = 0; x < 64; x++) {
            uint8_t cursorBuffer = 0;

            for (uint8_t i = 0; i < 8; i++) {
                uint64_t bufferString = buffer[y + i];
                uint8_t val = (bufferString >> x) & (uint64_t)(1);

                cursorBuffer = cursorBuffer << 1;
                cursorBuffer |= val;
            }

            printCursor(x, y, fullRotate(cursorBuffer));
        }
}

void clearBuffer() {
    for (uint8_t i = 0; i < 64; i++)
        buffer[i] = (uint64_t)0;
}

uint8_t fullRotate(uint8_t value) {
    uint8_t result = 0;
    uint8_t remainder = value;

    for (uint8_t i = 0; i < 8; i++) {
        result = result << 1;
        result |= (remainder >> i) & 1;

        remainder = value;
    }

    return result;
}

uint32_t readTouchX(void) {
    ADMUX = (1 << REFS0) | PA0;
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
        ;

    if (ADC < 50) return 65;

    uint32_t val = (ADC * 142) / 1023;

    return val;
}

uint32_t readTouchY(void) {
    ADMUX = (1 << REFS0) | PA1;
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
        ;

    if (ADC < 50) return 65;

    uint32_t val = (ADC * 142) / 1023;

    return val;
}

void printCursor(unsigned char x, unsigned char y, unsigned char on) {
    unsigned char page = y / 8;
    unsigned char column = x % 64;

    glcd_SetCursor(page, column);

    GLCD_Data(on);  // After going to display section, the shift amount determines where to draw.
}

void GLCD_Command(char Command) /* GLCD command function */
{
    Data_Port = Command; /* Copy command on data pin */
    PORTA &= ~(1 << RS); /* Make RS LOW for command register*/
    PORTA &= ~(1 << RW); /* Make RW LOW for write operation */
    PORTD |= (1 << EN);  /* HIGH-LOW transition on Enable */
    _delay_us(5);
    PORTD &= ~(1 << EN);
    _delay_us(5);
}

void GLCD_Data(char Data) /* GLCD data function */
{
    Data_Port = Data;    /* Copy data on data pin */
    PORTA |= (1 << RS);  /* Make RS HIGH for data register */
    PORTA &= ~(1 << RW); /* Make RW LOW for write operation */
    PORTD |= (1 << EN);  /* HIGH-LOW transition on Enable */
    _delay_us(5);
    PORTD &= ~(1 << EN);
    _delay_us(5);
}
void GLCD_Change(int screen) {
    // false = left screen, true = right screen

    PORTA = 0x0C;
    PORTD = 0x80;
    if (screen == 1)
        PORTB = 0x01;
    else
        PORTB = 0x02;

    PORTD |= (1 << 7);
    GLCD_Command(0x3E);  /* Display OFF */
    GLCD_Command(0x42);  /* Set Y address (column=0) */
    GLCD_Command(0xB8);  /* Set x address (page=0) */
    GLCD_Command(0xC0);  /* Set z address (start line=0) */
    GLCD_Command(0x3F);  // Display ON
}

void glcd_Init() {
    // Set the direction of control and data pins
    DDRA |= (1 << RS) | (1 << RW);
    DDRD |= (1 << EN) | (1 << RST);
    DDRB |= (1 << CS1) | (1 << CS2);
    DDRC = 0xFF;  // Assuming the data port is on PORTC

    _delay_ms(20);

    // Initialize control pin states
    PORTA = 0x00;  // Clear RS and RW
    PORTD = 0x00;  // Clear EN and RST
    PORTB = 0x00;  // Clear CS1 and CS2

    // Apply initialization sequence
    _delay_ms(20);
    GLCD_Command(0x3E);  /* Display OFF */
    GLCD_Command(0x42);  /* Set Y address (column=0) */
    GLCD_Command(0xB8);  /* Set x address (page=0) */
    GLCD_Command(0xC0);  /* Set z address (start line=0) */
    GLCD_Command(0x3F);  // Display ON
}

void adc_init() {
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    DDRA &= ~(1 << PA0) & ~(1 << PA1);
}

void glcd_SetCursor(uint8_t x, uint8_t y) {
    GLCD_Command(0xb8 + x);
    GLCD_Command(0x40 + y);
}