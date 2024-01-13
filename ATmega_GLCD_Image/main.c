#include <avr/io.h>
#include <util/delay.h>

// GLCD commands
#define GLCD_CMD_COLUMN_ADDR_SET_LOW   0x00
#define GLCD_CMD_COLUMN_ADDR_SET_HIGH  0x10
#define GLCD_CMD_PAGE_ADDR_SET         0xB0
#define GLCD_CMD_START_LINE_SET        0x40
#define GLCD_CMD_ADC_NORMAL            0xA0
#define GLCD_CMD_ADC_REVERSE           0xA1
#define GLCD_CMD_DISPLAY_NORMAL        0xA6
#define GLCD_CMD_DISPLAY_REVERSE       0xA7
#define GLCD_CMD_DISPLAY_OFF           0xAE
#define GLCD_CMD_DISPLAY_ON            0xAF
#define TOUCH_X_ADC_CHANNEL 0
#define TOUCH_Y_ADC_CHANNEL 1
#define NO_TOUCH_THRESHOLD 50


// Function prototypes
void glcd_command(uint8_t cmd);
void glcd_data(uint8_t data);
void glcd_set_position(uint8_t column, uint8_t page);
void glcd_clear(void);
void drawPixel(uint8_t, uint8_t);
void fillRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height);
void drawRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height);
void fillRound(uint8_t x, uint8_t y, uint8_t radius);
uint16_t readTouchX(void);
uint16_t readTouchY(void);
void resetIndex(void);
uint8_t checkIndex(uint8_t x, uint8_t y);
void openDoors(void);
void moveNextPoint(void);

// Global Variables
uint8_t patternIndex = 0;
uint8_t pattern[] = {0, 1, 2, 4, 6, 7, 8};
uint8_t patternSize = 7;

typedef struct {
	uint8_t x;
    uint8_t y;
} Coordinate;

Coordinate patternCoordinates[] = {
	{16,16}, 
	{16,32}, 
	{16,48},
	{32, 16},
	{32,32}, 
	{32, 48},
	{48,16}, 
	{48,32}, 
	{48,48}
};

int main(void) {
	// Set up SPI as Master
	DDRB |= (1 << DDB2) | (1 << DDB1) | (1 << DDB0);
	SPCR |= (1 << MSTR) | (1 << SPR0);
	SPCR |= (1 << SPE);

	// Set up GLCD control pins
	DDRA |= (1 << PA2) | (1 << PA3); // RS and RW as outputs
	DDRD |= (1 << PD6) | (1 << PD7); // EN and RST as outputs
	DDRB |= (1 << PB0) | (1 << PB1); // CS1 and CS2 as outputs

	// Initialize GLCD
	PORTD |= (1 << PD7); // Set RST high (disable reset)
	_delay_ms(100);
	PORTD &= ~(1 << PD7); // Set RST low (enable reset)
	_delay_ms(100);
	PORTD |= (1 << PD7); // Set RST high (disable reset)

	glcd_command(GLCD_CMD_DISPLAY_OFF);          // Display off
	glcd_command(GLCD_CMD_ADC_NORMAL);           // ADC normal
	glcd_command(GLCD_CMD_DISPLAY_NORMAL);       // Display normal
	glcd_command(GLCD_CMD_DISPLAY_ON);           // Display on

	// Clear the GLCD
	glcd_clear();

	while (1) {
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				drawPixel(16*(i+1), 16*(j+1));
				
				
		uint8_t x = readTouchX();
		uint8_t y = readTouchY();
		
		fillRound(x, y, 5);
		
		if (x == 65 || y == 65) {
			resetIndex();
			continue;
		}
		
		uint8_t valid = checkIndex(x, y);
		
		if (valid) {
			if (patternIndex == patternSize - 1) openDoors();
			else moveNextPoint();	
		}
			
			
			
		
		
	}
}

void resetIndex(void) {
	patternIndex = 0;
}

uint8_t checkIndex(uint8_t x, uint8_t y) {
	Coordinate point = patternCoordinates[patternIndex];
	if (point.x == x && point.y == y)
		return 1;
	
	return 0;
}



void moveNextPoint(void) {
	patternIndex++;
}

void openDoors(void) {
	// PWM
}


// Send command to GLCD
void glcd_command(uint8_t cmd) {
	PORTA &= ~(1 << PA2); // RS low for command
	PORTA &= ~(1 << PA3); // RW low for write
	PORTB &= ~(1 << PB0); // CS1 low to select GLCD
	PORTB &= ~(1 << PB1); // CS2 low to select GLCD
	SPDR = cmd; // Send command byte
	while (!(SPSR & (1 << SPIF))); // Wait for transmission to complete
	PORTB |= (1 << PB0) | (1 << PB1); // Set CS1 and CS2 high to end communication
}

// Send data to GLCD
void glcd_data(uint8_t data) {
	PORTA |= (1 << PA2); // RS high for data
	PORTA &= ~(1 << PA3); // RW low for write
	PORTB &= ~(1 << PB0); // CS1 low to select GLCD
	PORTB &= ~(1 << PB1); // CS2 low to select GLCD
	SPDR = data; // Send data byte
	while (!(SPSR & (1 << SPIF))); // Wait for transmission to complete
	PORTB |= (1 << PB0) | (1 << PB1); // Set CS1 and CS2 high to end communication
}

// Set GLCD position (column and page)
void glcd_set_position(uint8_t column, uint8_t page) {
	glcd_command(GLCD_CMD_COLUMN_ADDR_SET_LOW | (column & 0x0F));
	glcd_command(GLCD_CMD_COLUMN_ADDR_SET_HIGH | ((column >> 4) & 0x0F));
	glcd_command(GLCD_CMD_PAGE_ADDR_SET | (page & 0x0F));
}

// Clear the entire GLCD
void glcd_clear(void) {
	for (uint8_t page = 0; page < 8; ++page) {
		glcd_set_position(0, page);
		for (uint8_t column = 0; column < 64; ++column) {
			glcd_data(0x00);
		}
	}
}

// Function to read X-coordinate from the touch panel
uint16_t readTouchX(void) {
	ADMUX = (1 << REFS0) | TOUCH_X_ADC_CHANNEL; // Set ADC reference and channel for X-axis
	ADCSRA |= (1 << ADSC); // Start ADC conversion
	while (ADCSRA & (1 << ADSC)); // Wait for conversion to complete

	// Check for no touch condition
	if (ADC < NO_TOUCH_THRESHOLD) {
		return 65; // Return 0 or another suitable value for no touch
	}

	return ADC; // Return ADC value
}

// Function to read Y-coordinate from the touch panel
uint16_t readTouchY(void) {
	ADMUX = (1 << REFS0) | TOUCH_Y_ADC_CHANNEL; // Set ADC reference and channel for Y-axis
	ADCSRA |= (1 << ADSC); // Start ADC conversion
	while (ADCSRA & (1 << ADSC)); // Wait for conversion to complete

	// Check for no touch condition
	if (ADC < NO_TOUCH_THRESHOLD) {
		return 65; // Return 0 or another suitable value for no touch
	}

	return ADC; // Return ADC value
}



void drawPixel(uint8_t x, uint8_t y) {
	uint8_t page = y / 8; // Calculate the page
	uint8_t column = x;   // Assume one pixel corresponds to one column

	glcd_set_position(column, page);
	glcd_data(1 << (y % 8)); // Set the corresponding bit for the pixel in the data byte
}

void fillRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
	for (int i = 0; i < width; i++)
		for (int j = 0; j < height; j++) 
			drawPixel(x + i, y + j);
}

void drawRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
	for (int i = 0; i < width; i++)
		for (int j = 0; j < height; j++) 
			if (i == 0 || y == 0 || x == width - 1 || y == height - 1)
				drawPixel(x + i, y + j);	
}

void fillRound(uint8_t x, uint8_t y, uint8_t radius) {
	for (int16_t i = -radius; i <= radius; i++) 
		for (int16_t j = -radius; j <= radius; j++) 
			if (i * i + j * j <= radius * radius) 
				drawPixel(x + i, y + j);
}
