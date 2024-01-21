/* Define CPU clock Freq 8MHz */
#define F_CPU 8000000UL

#include <avr/interrupt.h> /* Include interrupts for USART*/
#include <avr/io.h>        /* Include AVR std. library file */
#include <stdio.h>         /* Include std i/o library file */
#include <stdlib.h>
#include <util/delay.h> /* Include delay header file */

// #define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

#define Data_Port PORTC

#define RS PA2 /* Define control pins */
#define RW PA3
#define EN PD6
#define CS1 PB0
#define CS2 PB1
#define RST PD7

// Application States
typedef enum {
    LOCKED,
    UNLOCKED,
    CHANGING_PASSWORD,
    ENTERING_NEW_PASSWORD
} State;

typedef struct {
    uint8_t x;
    uint8_t y;
} Coordinate;

Coordinate patternCoordinates[] = {
    {16, 16},
    {32, 16},
    {48, 16},
    {16, 32},
    {32, 32},
    {48, 32},
    {16, 48},
    {32, 48},
    {48, 48}};

// Global Variables
uint64_t buffer[64];  // Frame buffer to render the display. Holds 64 64-bit integers that represents each pixel on the GLCD.
State state = LOCKED;

uint8_t patternIndex = 0;
uint8_t pattern[9] = {0, 3, 6, 7, 8, 9, 9, 9, 9};         // Indices of points on `patternCoordinates` array. 9 means NULL or empty.
uint8_t patternSize = 5;                                  // Hidden pattern size.
uint8_t enteredPattern[9] = {9, 9, 9, 9, 9, 9, 9, 9, 9};  // User entered pattern.
uint8_t enteredPatternSize = 0;                           // User entered pattern size. Incremented on every point match.

void glcdInit(void);                                                   // Sets up GLCD display to receive data and command.
void glcdCommand(char Command);                                        // Sends command to GLCD.
void glcdData(char Data);                                              // Sends data through D0-D7.
void glcdChange(int x);                                                // Sets the currently rendered side of GLCD.
void glcdSetCursor(uint8_t x, uint8_t y);                              // Sets cursor to the beginning of the page. Utilized to render frame buffer.
void printCursor(unsigned char x, unsigned char y, unsigned char on);  // Prints 8-bit data to current cursor location. Used in rendering of the frame buffer.

void adcInit();             // Initialize analog to digital converter. Used to read the analog touch data.
uint32_t readTouchX(void);  // Returns x coordinate of the touch.
uint32_t readTouchY(void);  // Returns y coordinate of the touch.

void clearBuffer();                                             // Resets the frame buffer to be all 0s.
void renderBuffer();                                            // Renders what is in the frame buffer.
void drawPixel(uint8_t x, uint8_t y);                           // Sets the entered bit of the frame buffer.
void fillRound(uint8_t x, uint8_t y, uint8_t radius);           // Draws (sets bits) a filled round.
void drawCircle(uint8_t cx, uint8_t cy, uint8_t radius);        // Draws a border circle.
void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2);  // Draws a line between two points.

uint8_t fullRotate(uint8_t value);  // 180deg rotates the given 8-bit integer (11110010 -> 01001111).

void changePassword();                        // Sets the `pattern` to `enteredPattern`.
uint8_t checkPattern();                       // Checks if `pattern` matches to `enteredPattern`.
uint8_t getCoordinateId(uint32_t, uint32_t);  // Given x and y, return the id based on `patternCoordinates` (16,16) -> 0.
void drawEnteredPattern();                    // Renders the lines of entered pattern.
void resetPattern(void);                      // Resets `enteredPattern`.
uint8_t checkIndex(uint8_t x, uint8_t y);     // Checks whether a given point is in the `patternCoordinates`. (48,32) -> true.

void uartInit(long USART_BAUDRATE);       // Initialized USART interface.
void uartSendChar(char ch);               // Sends a single character through UART.
void uartSendString(char *str);           // Sends a whole string (char[]) using `uartSendChar`.
void printPoint(uint16_t x, uint16_t y);  // Prints a point using UART. Mainly for debugging purposes.

/**
 * Interrupt vector to receive and react to user entered data.
 */
ISR(USART_RXC_vect) {
    if (UDR == 'c' || UDR == 'C') {
        state = CHANGING_PASSWORD;
        uartSendString("\n\rPlease enter the current password to change password.");
    } else if ((UDR == 'q' || UDR == 'Q') && state == UNLOCKED) {
        state = LOCKED;
        uartSendString("\n\rExiting home. See you again.");
    } else
        uartSendString("\n\rPlease enter a valid parameter.");
}

int main(void) {
    uartInit(9600);
    adcInit();
    glcdInit();
    sei();

    _delay_ms(2);
    glcdChange(1);

    int velX = 1;
    int velY = 2;
    int x = 6;
    int y = 30;

    uartSendString("\n\rWelcome to smart home service. You can change password by entering (c): ");

    while (1) {
        clearBuffer();

        drawLine(0, 0, 0, 63);

        uint32_t mouseX = readTouchX();
        uint32_t mouseY = readTouchY();

        fillRound(mouseX, mouseY, 1);

        if (state == UNLOCKED) {
            if (x - 4 <= 0 || x + 4 >= 63) velX *= -1;
            if (y - 4 <= 0 || y + 4 >= 63) velY *= -1;

            x += velX;
            y += velY;

            drawLine(0, 32, x, y);
            drawLine(63, 32, x, y);
            fillRound(x, y, 4);
        } else {
            for (int8_t i = 0; i < 3; i++)
                for (int8_t j = 0; j < 3; j++) {
                    drawCircle(16 + i * 16, 16 + j * 16, 5);
                    fillRound(16 + i * 16, 16 + j * 16, 1);
                }

            if (mouseX == 0) {
                uint8_t valid = checkPattern();
                if (state == CHANGING_PASSWORD) {
                    if (valid) {
                        state = ENTERING_NEW_PASSWORD;
                        uartSendString("\n\rEnter your new password.");
                        resetPattern();
                    } else {
                        if (enteredPatternSize > 0) uartSendString("\n\rPassword is not correct. Try again.");
                        resetPattern();
                    }
                } else if (state == LOCKED) {
                    if (valid) {
                        state = UNLOCKED;
                        uartSendString("\n\rWelcome to your home.");
                        resetPattern();
                    } else {
                        if (enteredPatternSize > 0) uartSendString("\n\rPassword is not correct. Try again.");
                        resetPattern();
                    }
                } else if (state == ENTERING_NEW_PASSWORD) {
                    if (enteredPatternSize > 3) {
                        state = LOCKED;
                        changePassword();
                        resetPattern();
                    } else if (enteredPatternSize > 0) {
                        resetPattern();
                        uartSendString("\n\rThe new password should be consisting of at least 4 different dots. Try a longer pattern.");
                    } else
                        resetPattern();
                }

            } else {
                uint8_t valid = checkIndex(mouseX, mouseY);
                if (valid) {
                    enteredPattern[enteredPatternSize] = getCoordinateId(mouseX, mouseY);
                    enteredPatternSize++;
                }

                if (enteredPatternSize > 0) {
                    drawEnteredPattern();
                    Coordinate c = patternCoordinates[enteredPattern[enteredPatternSize - 1]];
                    drawLine(c.x, c.y, mouseX, mouseY);
                    fillRound(mouseX, mouseY, 2);
                }
            }
        }

        renderBuffer();
        _delay_ms(5);
    }

    return 0;
}

void changePassword() {
    for (uint8_t i = 0; i < 9; i++) {
        pattern[i] = enteredPattern[i];
    }

    patternSize = enteredPatternSize;

    uartSendString("\n\rCongratulations! You changed your password. Please enter it to open doors.");
}

void drawEnteredPattern() {
    if (enteredPatternSize == 0 || enteredPatternSize == 1) return;

    for (uint8_t i = 0; i < enteredPatternSize - 1; i++) {
        Coordinate c1 = patternCoordinates[enteredPattern[i]];
        Coordinate c2 = patternCoordinates[enteredPattern[i + 1]];
        drawLine(c1.x, c1.y, c2.x, c2.y);
    }
}

uint8_t getCoordinateId(uint32_t x, uint32_t y) {
    for (uint8_t i = 0; i < 9; i++) {
        Coordinate c = patternCoordinates[i];
        if (c.x == x && c.y == y) return i;
    }

    return 9;
}

void resetPattern() {
    enteredPatternSize = 0;
    for (uint8_t i = 0; i < 9; i++) {
        enteredPattern[i] = 9;
    }
}

uint8_t checkIndex(uint8_t x, uint8_t y) {
    for (uint8_t i = 0; i < 9; i++) {
        Coordinate c = patternCoordinates[i];

        for (uint8_t j = 0; j < enteredPatternSize; j++) {
            Coordinate enteredC = patternCoordinates[enteredPattern[j]];

            if (enteredC.x == c.x && enteredC.y == c.y) goto outer;
        }

        if (c.x == x && c.y == y) return 1;
    outer:;
    }

    return 0;
}

uint8_t checkPattern() {
    if (enteredPatternSize != patternSize) return 0;

    for (uint8_t i = 0; i < patternSize; i++) {
        if (pattern[i] != enteredPattern[i]) return 0;
    }

    return 1;
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

void adcInit() {
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    DDRA &= ~(1 << PA0);
    DDRA &= ~(1 << PA1);
}

uint32_t readTouchX(void) {
    ADMUX = (ADMUX & 0xf8) | PA0;
    _delay_us(20);
    PORTA &= (0 << RW);
    PORTA |= (1 << RS);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
        ;

    uint16_t reading = ADCL;
    reading += ADCH << 8;

    if (reading < 495) return 0;

    uint32_t val = (reading * 142) / 1023 + 1;

    return val;
}

uint32_t readTouchY(void) {
    ADMUX = (ADMUX & 0xf8) | PA1;
    _delay_us(20);
    PORTA &= (0 << RS);
    PORTA |= (1 << RW);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
        ;

    uint16_t reading = ADCL;
    reading += ADCH << 8;

    if (reading < 50) return 0;
    reading = (reading * 63) / (300 - 54);
    uint32_t val = 63 - reading + 15;

    return val;
}

void printCursor(unsigned char x, unsigned char y, unsigned char on) {
    unsigned char page = y / 8;
    unsigned char column = x % 64;

    glcdSetCursor(page, column);

    glcdData(on);  // After going to display section, the shift amount determines where to draw.
}

void glcdCommand(char Command) /* GLCD command function */
{
    Data_Port = Command; /* Copy command on data pin */
    PORTA &= ~(1 << RS); /* Make RS LOW for command register*/
    PORTA &= ~(1 << RW); /* Make RW LOW for write operation */
    PORTD |= (1 << EN);  /* HIGH-LOW transition on Enable */
    _delay_us(5);
    PORTD &= ~(1 << EN);
    _delay_us(5);
}

void glcdData(char Data) /* GLCD data function */
{
    Data_Port = Data;    /* Copy data on data pin */
    PORTA |= (1 << RS);  /* Make RS HIGH for data register */
    PORTA &= ~(1 << RW); /* Make RW LOW for write operation */
    PORTD |= (1 << EN);  /* HIGH-LOW transition on Enable */
    _delay_us(5);
    PORTD &= ~(1 << EN);
    _delay_us(5);
}
void glcdChange(int screen) {
    // false = left screen, true = right screen
    PORTA = 0xc;
    PORTD = 0x80;
    if (screen == 1)
        PORTB = 0x01;
    else
        PORTB = 0x02;

    glcdCommand(0x3E);  /* Display OFF */
    glcdCommand(0x42);  /* Set Y address (column=0) */
    glcdCommand(0xB8);  /* Set x address (page=0) */
    glcdCommand(0xC0);  /* Set z address (start line=0) */
    glcdCommand(0x3F);  // Display ON
}

void glcdInit() {
    // Set the direction of control and data pins
    DDRA = (1 << RS) | (1 << RW);
    DDRD |= (1 << EN) | (1 << RST);
    DDRB |= (1 << CS1) | (1 << CS2);
    DDRC = 0xFF;  // Assuming the data port is on PORTC

    _delay_ms(20);

    // Initialize control pin states
    PORTD = 0x00;  // Clear EN and RST
    PORTB = 0x00;  // Clear CS1 and CS2

    // Apply initialization sequence
    _delay_ms(20);
    glcdCommand(0x3E);  /* Display OFF */
    glcdCommand(0x42);  /* Set Y address (column=0) */
    glcdCommand(0xB8);  /* Set x address (page=0) */
    glcdCommand(0xC0);  /* Set z address (start line=0) */
    glcdCommand(0x3F);  // Display ON
}

void glcdSetCursor(uint8_t x, uint8_t y) {
    glcdCommand(0xb8 + x);
    glcdCommand(0x40 + y);
}

void uartInit(long USART_BAUDRATE) {
    UCSRB |= (1 << RXEN) | (1 << TXEN) | (1 << RXCIE);   /* Turn on transmission and reception */
    UCSRC |= (1 << URSEL) | (1 << UCSZ0) | (1 << UCSZ1); /* Use 8-bit character sizes */
    UBRRL = BAUD_PRESCALE;                               /* Load lower 8-bits of the baud rate value */
    UBRRH = (BAUD_PRESCALE >> 8);                        /* Load upper 8-bits*/
}

void uartSendChar(char ch) {
    while (!(UCSRA & (1 << UDRE)))
        ; /* Wait for empty transmit buffer*/
    UDR = ch;
}

void uartSendString(char *str) {
    unsigned char j = 0;

    while (str[j] != 0) {
        uartSendChar(str[j]);
        j++;
    }
}

void printPoint(uint16_t x, uint16_t y) {
    char xreading[sizeof(uint16_t) * 8 + 1];
    char yreading[sizeof(uint16_t) * 8 + 1];

    itoa(x, xreading, 10);
    itoa(y, yreading, 10);

    uartSendString("  (");
    uartSendString(xreading);
    uartSendString(", ");
    uartSendString(yreading);
    uartSendString(")  ");
}