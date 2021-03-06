#if BACKLIGHT_ENABLED

#include "zeal60.h"
#include "zeal_backlight.h"
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "i2c_master.h"
#include "progmem.h"
#include "config.h"
#include "zeal_eeprom.h"
#include "zeal_color.h"

#include "is31fl3731.h"

#define BACKLIGHT_EFFECT_MAX 11

zeal_backlight_config g_config = {
	.use_split_backspace = BACKLIGHT_USE_SPLIT_BACKSPACE,
	.use_split_left_shift = BACKLIGHT_USE_SPLIT_LEFT_SHIFT,
	.use_split_right_shift = BACKLIGHT_USE_SPLIT_RIGHT_SHIFT,
	.use_7u_spacebar = BACKLIGHT_USE_7U_SPACEBAR,
	.use_iso_enter = BACKLIGHT_USE_ISO_ENTER,
	.disable_hhkb_blocker_leds = BACKLIGHT_DISABLE_HHKB_BLOCKER_LEDS,
	.disable_when_usb_suspended = BACKLIGHT_DISABLE_WHEN_USB_SUSPENDED,
	.disable_after_timeout = BACKLIGHT_DISABLE_AFTER_TIMEOUT,
	.brightness = 255,
	.effect = 255, 
	.effect_speed = 0,
	.color_1 = { .h = 0, .s = 255, .v = 255 },
	.color_2 = { .h = 127, .s = 255, .v = 255 },
	.caps_lock_indicator = { .color = { .h = 0, .s = 0, .v = 255 }, .index = 255 },
	.layer_1_indicator = { .color = { .h = 0, .s = 0, .v = 255 }, .index = 255 },
	.layer_2_indicator = { .color = { .h = 0, .s = 0, .v = 255 }, .index = 255 },
	.layer_3_indicator = { .color = { .h = 0, .s = 0, .v = 255 }, .index = 255 },
	.alphas_mods = {
		BACKLIGHT_ALPHAS_MODS_ROW_0,
		BACKLIGHT_ALPHAS_MODS_ROW_1,
		BACKLIGHT_ALPHAS_MODS_ROW_2,
		BACKLIGHT_ALPHAS_MODS_ROW_3,
		BACKLIGHT_ALPHAS_MODS_ROW_4 }
};

bool g_suspend_state = false;
uint8_t g_indicator_state = 0;

// Global tick at 20 Hz
uint32_t g_tick = 0;

// Ticks since this key was last hit.
uint8_t g_key_hit[72];

uint32_t g_any_key_hit = 0;

#define ISSI_ADDR_1 0x74
#define ISSI_ADDR_2 0x76

const is31_led g_is31_leds[DRIVER_LED_TOTAL] = {
	{0, C2_1,  C3_1,  C4_1},  // LA0
	{0, C1_1,  C3_2, C4_2},   // LA1
	{0, C1_2,  C2_2, C4_3},   // LA2
	{0, C1_3,  C2_3, C3_3},   // LA3
	{0, C1_4,  C2_4, C3_4},   // LA4
	{0, C1_5,  C2_5, C3_5},   // LA5
	{0, C1_6,  C2_6, C3_6},   // LA6
	{0, C1_7,  C2_7, C3_7},   // LA7
	{0, C1_8,  C2_8, C3_8},   // LA8
	{0, C9_1,  C8_1,  C7_1},  // LA9
	{0, C9_2,  C8_2, C7_2},   // LA10
	{0, C9_3,  C8_3, C7_3},   // LA11
	{0, C9_4,  C8_4, C7_4},   // LA12
	{0, C9_5,  C8_5, C7_5},   // LA13
	{0, C9_6,  C8_6, C7_6},   // LA14
	{0, C9_7,  C8_7, C6_6},   // LA15
	{0, C9_8,  C7_7, C6_7},   // LA16
	{0, C8_8,  C7_8, C6_8},   // LA17

	{0, C2_9,  C3_9,  C4_9},  // LB0
	{0, C1_9,  C3_10, C4_10}, // LB1
	{0, C1_10, C2_10, C4_11}, // LB2
	{0, C1_11, C2_11, C3_11}, // LB3
	{0, C1_12, C2_12, C3_12}, // LB4
	{0, C1_13, C2_13, C3_13}, // LB5
	{0, C1_14, C2_14, C3_14}, // LB6
	{0, C1_15, C2_15, C3_15}, // LB7
	{0, C1_16, C2_16, C3_16}, // LB8
	{0, C9_9,  C8_9,  C7_9},  // LB9
	{0, C9_10, C8_10, C7_10}, // LB10
	{0, C9_11, C8_11, C7_11}, // LB11
	{0, C9_12, C8_12, C7_12}, // LB12
	{0, C9_13, C8_13, C7_13}, // LB13
	{0, C9_14, C8_14, C7_14}, // LB14
	{0, C9_15, C8_15, C6_14}, // LB15
	{0, C9_16, C7_15, C6_15}, // LB16
	{0, C8_16, C7_16, C6_16}, // LB17

	{1, C2_1,  C3_1,  C4_1},  // LC0
	{1, C1_1,  C3_2, C4_2},   // LC1
	{1, C1_2,  C2_2, C4_3},   // LC2
	{1, C1_3,  C2_3, C3_3},   // LC3
	{1, C1_4,  C2_4, C3_4},   // LC4
	{1, C1_5,  C2_5, C3_5},   // LC5
	{1, C1_6,  C2_6, C3_6},   // LC6
	{1, C1_7,  C2_7, C3_7},   // LC7
	{1, C1_8,  C2_8, C3_8},   // LC8
	{1, C9_1,  C8_1,  C7_1},  // LC9
	{1, C9_2,  C8_2, C7_2},   // LC10
	{1, C9_3,  C8_3, C7_3},   // LC11
	{1, C9_4,  C8_4, C7_4},   // LC12
	{1, C9_5,  C8_5, C7_5},   // LC13
	{1, C9_6,  C8_6, C7_6},   // LC14
	{1, C9_7,  C8_7, C6_6},   // LC15
	{1, C9_8,  C7_7, C6_7},   // LC16
	{1, C8_8,  C7_8, C6_8},   // LC17

	{1, C2_9,  C3_9,  C4_9},  // LD0
	{1, C1_9,  C3_10, C4_10}, // LD1
	{1, C1_10, C2_10, C4_11}, // LD2
	{1, C1_11, C2_11, C3_11}, // LD3
	{1, C1_12, C2_12, C3_12}, // LD4
	{1, C1_13, C2_13, C3_13}, // LD5
	{1, C1_14, C2_14, C3_14}, // LD6
	{1, C1_15, C2_15, C3_15}, // LD7
	{1, C1_16, C2_16, C3_16}, // LD8
	{1, C9_9,  C8_9,  C7_9},  // LD9
	{1, C9_10, C8_10, C7_10}, // LD10
	{1, C9_11, C8_11, C7_11}, // LD11
	{1, C9_12, C8_12, C7_12}, // LD12
	{1, C9_13, C8_13, C7_13}, // LD13
	{1, C9_14, C8_14, C7_14}, // LD14
	{1, C9_15, C8_15, C6_14}, // LD15
	{1, C9_16, C7_15, C6_15}, // LD16
	{1, C8_16, C7_16, C6_16}, // LD17
};

typedef struct Point {
	uint8_t x;
	uint8_t y;
} Point;

#ifdef CONFIG_ZEAL65
const Point g_map_led_to_point[72] PROGMEM = {
	// LA0..LA17
	{120,16}, {104,16}, {88,16}, {72,16}, {56,16}, {40,16}, {24,16}, {4,16}, {4,32},
	{128,0}, {112,0}, {96,0}, {80,0}, {64,0}, {48,0}, {32,0}, {16,0}, {0,0},
	// LB0..LB17
	{144,0}, {160,0}, {176,0}, {192,0}, {216,0}, {224,0}, {240,0}, {240,16}, {240,32},
	{136,16}, {152,16}, {168,16}, {184,16}, {200,16}, {220,16}, {240,48}, {240,64}, {224,64},
	// LC0..LC17
	{96,64}, {100,48}, {84,48}, {68,48}, {52,48}, {36,48}, {255,255}, {48,60}, {28,64},
	{108,32}, {92,32}, {76,32}, {60,32}, {44,32}, {28,32}, {20,44}, {10,48}, {4,64},
	// LD0..LD17
	{124,32}, {140,32}, {156,32}, {172,32}, {188,32}, {214,32}, {180,48}, {202,48}, {224,48},
	{116,48}, {132,48}, {148,48}, {164,48}, {255,255}, {144,60}, {164,64}, {188,64}, {208,64}
};
const Point g_map_led_to_point_polar[72] PROGMEM = {
	// LA0..LA17
	{64,128}, {75,132}, {84,145}, {91,164}, {97,187}, {102,213}, {105,242}, {109,255}, {128,247},
	{61,255}, {67,255}, {72,255}, {77,255}, {82,255}, {86,255}, {90,255}, {93,255}, {96,255},
	// LB0..LB17
	{56,255}, {51,255}, {46,255}, {42,255}, {37,255}, {35,255}, {32,255}, {19,255}, {0,255},
	{53,132}, {44,145}, {37,164}, {31,187}, {26,213}, {22,249}, {237,255}, {224,255}, {221,255},
	// LC0..LC17
	{184,255}, {179,135}, {170,149}, {163,169}, {157,193}, {153,220}, {255,255}, {167,255}, {165,255},
	{128,26}, {128,60}, {128,94}, {128,128}, {128,162}, {128,196}, {145,233}, {148,255}, {161,255},
	// LD0..LD17
	{0,9}, {0,43}, {0,77}, {0,111}, {0,145}, {255,201}, {224,181}, {230,217}, {235,255},
	{189,128}, {200,131}, {210,141}, {218,159}, {201,228}, {201,228}, {206,255}, {213,255}, {218,255}
};
#else
const Point g_map_led_to_point[72] PROGMEM = {
	// LA0..LA17
	{120,16}, {104,16}, {88,16}, {72,16}, {56,16}, {40,16}, {24,16}, {4,16}, {4,32},
	{128,0}, {112,0}, {96,0}, {80,0}, {64,0}, {48,0}, {32,0}, {16,0}, {0,0},
	// LB0..LB17
	{144,0}, {160,0}, {176,0}, {192,0}, {216,0}, {224,0}, {255,255}, {255,255}, {255,255},
	{136,16}, {152,16}, {168,16}, {184,16}, {200,16}, {220,16}, {255,255}, {255,255}, {255,255},
	// LC0..LC17
	{102,64}, {100,48}, {84,48}, {68,48}, {52,48}, {36,48}, {60,64}, {43,64}, {23,64},
	{108,32}, {92,32}, {76,32}, {60,32}, {44,32}, {28,32}, {20,48}, {2,48}, {3,64},
	// LD0..LD17
	{124,32}, {140,32}, {156,32}, {172,32}, {188,32}, {214,32}, {180,48}, {210,48}, {224,48},
	{116,48}, {132,48}, {148,48}, {164,48}, {144,64}, {161,64}, {181,64}, {201,64}, {221,64}
};
const Point g_map_led_to_point_polar[72] PROGMEM = {
	// LA0..LA17
	{58,129}, {70,129}, {80,139}, {89,157}, {96,181}, {101,208}, {105,238}, {109,255}, {128,247}, {58,255},
	{64,255}, {70,255}, {75,255}, {80,255}, {85,255}, {89,255}, {93,255}, {96,255},
	// LB0..LB17
	{53,255}, {48,255}, {43,255}, {39,255}, {34,255}, {32,255}, {255,255}, {255,255}, {255,255},
	{48,139}, {39,157}, {32,181}, {27,208}, {23,238}, {19,255}, {255,255}, {255,255}, {255,255},
	// LC0..LC17
	{188,255}, {183,131}, {173,143}, {165,163}, {159,188}, {154,216}, {172,252}, {170,255}, {165,255},
	{128,9}, {128,46}, {128,82}, {128,119}, {128,155}, {128,192}, {150,244}, {147,255}, {161,255},
	// LD0..LD17
	{0,27}, {0,64}, {0,101}, {0,137}, {0,174}, {255,233}, {228,201}, {235,255}, {237,255},
	{195,128}, {206,136}, {215,152}, {222,175}, {205,234}, {209,255}, {214,255}, {219,255}, {223,255}
};
#endif // ZEAL65_PROTO

void map_led_to_point( uint8_t index, Point *point )
{
	// Slightly messy way to get Point structs out of progmem.
	uint8_t *addr = (uint8_t*)&g_map_led_to_point[index];
	point->x = pgm_read_byte(addr);
	point->y = pgm_read_byte(addr+1);

	switch (index)
	{
		case 18+4: // LB4A
			if ( g_config.use_split_backspace )
				point->x -= 8;
			break;
		case 18+14: // LB14A
			if ( g_config.use_iso_enter )
				point->y += 8; // extremely pedantic
			break;
#ifndef ZEAL65_PROTO
		case 36+0: // LC0A
			if ( g_config.use_7u_spacebar )
				point->x += 10;
			break;
		case 36+6: // LC6A
			if ( g_config.use_7u_spacebar )
				point->x += 4;
			break;
#endif
		case 36+16: // LC16A
			if ( !g_config.use_split_left_shift )
				point->x += 8;
			break;
		case 54+5: // LD5A
			if ( !g_config.use_iso_enter )
				point->x -= 10;
			break;
		case 54+7: // LD7A
			if ( !g_config.use_split_right_shift )
				point->x -= 8;
			break;
	}
}

void map_led_to_point_polar( uint8_t index, Point *point )
{
	uint8_t *addr = (uint8_t*)&g_map_led_to_point_polar[index];
	point->x = pgm_read_byte(addr);
	point->y = pgm_read_byte(addr+1);
}

#ifdef CONFIG_ZEAL65
const uint8_t g_map_row_column_to_led[MATRIX_ROWS][MATRIX_COLS] PROGMEM = {
	{  0+17,  0+16,  0+15,  0+14,  0+13,  0+12,  0+11,  0+10,   0+9,  18+0,  18+1,  18+2,  18+3,  18+4,  18+6 },
	{   0+7,   0+6,   0+5,   0+4,   0+3,   0+2,   0+1,   0+0,  18+9, 18+10, 18+11, 18+12, 18+13, 18+14,  18+7 },
	{   0+8, 36+14, 36+13, 36+12, 36+11, 36+10,  36+9,  54+0,  54+1,  54+2,  54+3,  54+4,  54+5,  18+5,  18+8 },
	{ 36+16, 36+15,  36+5,  36+4,  36+3,  36+2,  36+1,  54+9, 54+10, 54+11, 54+12,  54+6,  54+7,  54+8, 18+15 },
	{ 36+17,  36+8,  36+7,   255,   255,   255,   255,  36+0,  255,  54+14, 54+15, 54+16, 54+17, 18+17, 18+16 }
};
#else
const uint8_t g_map_row_column_to_led[MATRIX_ROWS][MATRIX_COLS] PROGMEM = {
	{  0+17,  0+16,  0+15,  0+14,  0+13,  0+12,  0+11,  0+10,   0+9,  18+0,  18+1,  18+2,  18+3,  18+4 },
	{   0+7,   0+6,   0+5,   0+4,   0+3,   0+2,   0+1,   0+0,  18+9, 18+10, 18+11, 18+12, 18+13, 18+14 },
	{   0+8, 36+14, 36+13, 36+12, 36+11, 36+10,  36+9,  54+0,  54+1,  54+2,  54+3,  54+4,  54+5,  18+5 },
	{ 36+16, 36+15,  36+5,  36+4,  36+3,  36+2,  36+1,  54+9, 54+10, 54+11, 54+12,  54+6,  54+7,  54+8 },
	{ 36+17,  36+8,  36+7,  36+6,   255,   255,   255,  36+0,  255,  54+13, 54+14, 54+15, 54+16, 54+17 }
};
#endif

void map_row_column_to_led( uint8_t row, uint8_t column, uint8_t *led )
{
	*led = 255;
	if ( row < MATRIX_ROWS && column < MATRIX_COLS )
	{
		*led = pgm_read_byte(&g_map_row_column_to_led[row][column]);
	}
}

void backlight_update_pwm_buffers(void)
{
	IS31_update_pwm_buffers( ISSI_ADDR_1, ISSI_ADDR_2 );
	IS31_update_led_control_registers( ISSI_ADDR_1, ISSI_ADDR_2 );
}

void backlight_set_color( int index, uint8_t red, uint8_t green, uint8_t blue )
{
	IS31_set_color( index, red, green, blue );
}

void backlight_set_color_all( uint8_t red, uint8_t green, uint8_t blue )
{
	IS31_set_color_all( red, green, blue );
}

void backlight_set_key_hit(uint8_t row, uint8_t column)
{
	uint8_t led;
	map_row_column_to_led(row,column,&led);
	g_key_hit[led] = 0;

	g_any_key_hit = 0;
}

#define TIMER3_TOP 781

void backlight_timer_init(void)
{
	static uint8_t backlight_timer_is_init = 0;
	if ( backlight_timer_is_init )
	{
		return;
	}
	backlight_timer_is_init = 1;

	// Timer 3 setup
	TCCR3B = _BV(WGM32) | 			// CTC mode OCR3A as TOP
			 _BV(CS32) | _BV(CS30); // prescale by /1024
	// Set TOP value
	uint8_t sreg = SREG;
	cli();

	OCR3AH = (TIMER3_TOP >> 8) & 0xff;
	OCR3AL = TIMER3_TOP & 0xff;
	SREG = sreg;
}

void backlight_timer_enable(void)
{
	TIMSK3 |= _BV(OCIE3A);
}

void backlight_timer_disable(void)
{
	TIMSK3 &= ~_BV(OCIE3A);
}

void backlight_set_suspend_state(bool state)
{
	g_suspend_state = state;
}

void backlight_set_indicator_state(uint8_t state)
{
	g_indicator_state = state;
}

void backlight_effect_rgb_test(void)
{
	// Mask out bits 4 and 5
	// This 2-bit value will stay the same for 16 ticks.
	switch ( (g_tick & 0x30) >> 4 )
	{
		case 0:
		{
			backlight_set_color_all( 255, 0, 0 );
			break;
		}
		case 1:
		{
			backlight_set_color_all( 0, 255, 0 );
			break;
		}
		case 2:
		{
			backlight_set_color_all( 0, 0, 255 );
			break;
		}
		case 3:
		{
			backlight_set_color_all( 255, 255, 255 );
			break;
		}
	}
}

void backlight_effect_single_LED_test(void)
{
	static uint8_t color = 0; // 0,1,2 for R,G,B
	static uint8_t row = 0;
	static uint8_t column = 0;

	static uint8_t tick = 0;
	tick++;

	if ( tick > 2 )
	{
		tick = 0;
		column++;
	}
	if ( column > 14 )
	{
		column = 0;
		row++;
	}
	if ( row > 4 )
	{
		row = 0;
		color++;
	}
	if ( color > 2 )
	{
		color = 0;
	}

	uint8_t led;
	map_row_column_to_led( row, column, &led );
	backlight_set_color_all( 255, 255, 255 );
	backlight_test_led( led, color==0, color==1, color==2 );
}

// All LEDs off
void backlight_effect_all_off(void)
{
	backlight_set_color_all( 0, 0, 0 );
}

// Solid color
void backlight_effect_solid_color(void)
{
	HSV hsv = { .h = g_config.color_1.h, .s = g_config.color_1.s, .v = g_config.brightness };
	RGB rgb = hsv_to_rgb( hsv );
	backlight_set_color_all( rgb.r, rgb.g, rgb.b );
}

// alphas = color1, mods = color2
void backlight_effect_alphas_mods(void)
{
	RGB rgb1 = hsv_to_rgb( (HSV){ .h = g_config.color_1.h, .s = g_config.color_1.s, .v = g_config.brightness } );
	RGB rgb2 = hsv_to_rgb( (HSV){ .h = g_config.color_2.h, .s = g_config.color_2.s, .v = g_config.brightness } );

	for ( int row = 0; row < MATRIX_ROWS; row++ )
	{
		for ( int column = 0; column < MATRIX_COLS; column++ )
		{
			uint8_t index;
			map_row_column_to_led( row, column, &index );
			if ( index < 72 )
			{
				if ( ( g_config.alphas_mods[row] & (1<<column) ) == 0 )
				{
					backlight_set_color( index, rgb1.r, rgb1.g, rgb1.b );
				}
				else
				{
					backlight_set_color( index, rgb2.r, rgb2.g, rgb2.b );
				}
			}
		}
	}
}

void backlight_effect_gradient_up_down(void)
{
	int16_t h1 = g_config.color_1.h;
	int16_t h2 = g_config.color_2.h;
	int16_t deltaH = h2 - h1;

	// Take the shortest path between hues
	if ( deltaH > 127 )
	{
		deltaH -= 256;
	}
	else if ( deltaH < -127 )
	{
		deltaH += 256;
	}
	// Divide delta by 4, this gives the delta per row
	deltaH /= 4;

	int16_t s1 = g_config.color_1.s;
	int16_t s2 = g_config.color_2.s;
	int16_t deltaS = ( s2 - s1 ) / 4;

	HSV hsv = { .h = 0, .s = 255, .v = g_config.brightness };
	RGB rgb;
	Point point;
	for ( int i=0; i<72; i++ )
	{
		map_led_to_point( i, &point );
		// The y range will be 0..64, map this to 0..4
		uint8_t y = (point.y>>4);
		// Relies on hue being 8-bit and wrapping
		hsv.h = g_config.color_1.h + ( deltaH * y );
		hsv.s = g_config.color_1.s + ( deltaS * y );
		rgb = hsv_to_rgb( hsv );
		backlight_set_color( i, rgb.r, rgb.g, rgb.b );
	}
}

void backlight_effect_raindrops(bool initialize)
{
	int16_t h1 = g_config.color_1.h;
	int16_t h2 = g_config.color_2.h;
	int16_t deltaH = h2 - h1;
	deltaH /= 4;

	// Take the shortest path between hues
	if ( deltaH > 127 )
	{
		deltaH -= 256;
	}
	else if ( deltaH < -127 )
	{
		deltaH += 256;
	}

	int16_t s1 = g_config.color_1.s;
	int16_t s2 = g_config.color_2.s;
	int16_t deltaS = ( s2 - s1 ) / 4;

	HSV hsv;
	RGB rgb;

	// Change one LED every tick
	uint8_t led_to_change = ( g_tick & 0x000 ) == 0 ? rand() % 72 : 255;

	for ( int i=0; i<72; i++ )
	{
		// If initialize, all get set to random colors
		// If not, all but one will stay the same as before.
		if ( initialize || i == led_to_change )
		{
			hsv.h = h1 + ( deltaH * ( rand() & 0x03 ) );
			hsv.s = s1 + ( deltaS * ( rand() & 0x03 ) );
			// Override brightness with global brightness control
			hsv.v = g_config.brightness;;

			rgb = hsv_to_rgb( hsv );
			backlight_set_color( i, rgb.r, rgb.g, rgb.b );
		}
	}
}

void backlight_effect_cycle_all(void)
{
	uint8_t offset = ( g_tick << g_config.effect_speed ) & 0xFF;

	// Relies on hue being 8-bit and wrapping
	for ( int i=0; i<72; i++ )
	{
		uint16_t offset2 = g_key_hit[i]<<2;
		// stabilizer LEDs use spacebar hits
		if ( i == 36+6 || i == 54+13 || // LC6, LD13
				( g_config.use_7u_spacebar && i == 54+14 ) ) // LD14
		{
			offset2 = g_key_hit[36+0]<<2;
		}
		offset2 = (offset2<=63) ? (63-offset2) : 0;

		HSV hsv = { .h = offset+offset2, .s = 255, .v = g_config.brightness };
		RGB rgb = hsv_to_rgb( hsv );
		backlight_set_color( i, rgb.r, rgb.g, rgb.b );
	}
}

void backlight_effect_cycle_left_right(void)
{
	uint8_t offset = ( g_tick << g_config.effect_speed ) & 0xFF;
	HSV hsv = { .h = 0, .s = 255, .v = g_config.brightness };
	RGB rgb;
	Point point;
	for ( int i=0; i<72; i++ )
	{
		uint16_t offset2 = g_key_hit[i]<<2;
		// stabilizer LEDs use spacebar hits
		if ( i == 36+6 || i == 54+13 || // LC6, LD13
				( g_config.use_7u_spacebar && i == 54+14 ) ) // LD14
		{
			offset2 = g_key_hit[36+0]<<2;
		}
		offset2 = (offset2<=63) ? (63-offset2) : 0;

		map_led_to_point( i, &point );
		// Relies on hue being 8-bit and wrapping
		hsv.h = point.x + offset + offset2;
		rgb = hsv_to_rgb( hsv );
		backlight_set_color( i, rgb.r, rgb.g, rgb.b );
	}
}

void backlight_effect_cycle_up_down(void)
{
	uint8_t offset = ( g_tick << g_config.effect_speed ) & 0xFF;
	HSV hsv = { .h = 0, .s = 255, .v = g_config.brightness };
	RGB rgb;
	Point point;
	for ( int i=0; i<72; i++ )
	{
		uint16_t offset2 = g_key_hit[i]<<2;
		// stabilizer LEDs use spacebar hits
		if ( i == 36+6 || i == 54+13 || // LC6, LD13
				( g_config.use_7u_spacebar && i == 54+14 ) ) // LD14
		{
			offset2 = g_key_hit[36+0]<<2;
		}
		offset2 = (offset2<=63) ? (63-offset2) : 0;

		map_led_to_point( i, &point );
		// Relies on hue being 8-bit and wrapping
		hsv.h = point.y + offset + offset2;
		rgb = hsv_to_rgb( hsv );
		backlight_set_color( i, rgb.r, rgb.g, rgb.b );
	}
}

void backlight_effect_jellybean_raindrops( bool initialize )
{
	HSV hsv;
	RGB rgb;

	// Change one LED every tick
	uint8_t led_to_change = ( g_tick & 0x000 ) == 0 ? rand() % 72 : 255;

	for ( int i=0; i<72; i++ )
	{
		// If initialize, all get set to random colors
		// If not, all but one will stay the same as before.
		if ( initialize || i == led_to_change )
		{
			hsv.h = rand() & 0xFF;
			hsv.s = rand() & 0xFF;
			// Override brightness with global brightness control
			hsv.v = g_config.brightness;;

			rgb = hsv_to_rgb( hsv );
			backlight_set_color( i, rgb.r, rgb.g, rgb.b );
		}
	}
}

void backlight_effect_cycle_radial1(void)
{
	uint8_t offset = ( g_tick << g_config.effect_speed ) & 0xFF;
	HSV hsv = { .h = 0, .s = 255, .v = g_config.brightness };
	RGB rgb;
	Point point;
	for ( int i=0; i<72; i++ )
	{
		map_led_to_point_polar( i, &point );
		// Relies on hue being 8-bit and wrapping
		hsv.h = point.x + offset;
		hsv.s = point.y;
		rgb = hsv_to_rgb( hsv );
		backlight_set_color( i, rgb.r, rgb.g, rgb.b );
	}
}

void backlight_effect_cycle_radial2(void)
{
	uint8_t offset = ( g_tick << g_config.effect_speed ) & 0xFF;

	HSV hsv = { .h = 0, .s = g_config.color_1.s, .v = g_config.brightness };
	RGB rgb;
	Point point;
	for ( int i=0; i<72; i++ )
	{
		map_led_to_point_polar( i, &point );
		uint8_t offset2 = offset + point.x;
		if ( offset2 & 0x80 )
		{
			offset2 = ~offset2;
		}
		offset2 = offset2 >> 2;
		hsv.h = g_config.color_1.h + offset2;
		hsv.s = 127 + ( point.y >> 1 );
		rgb = hsv_to_rgb( hsv );
		backlight_set_color( i, rgb.r, rgb.g, rgb.b );
	}
}

void backlight_effect_custom(void)
{
	HSV hsv;
	RGB rgb;
	for ( int i=0; i<72; i++ )
	{
		backlight_get_key_color(i, &hsv);
		// Override brightness with global brightness control
		hsv.v = g_config.brightness;
		rgb = hsv_to_rgb( hsv );
		backlight_set_color( i, rgb.r, rgb.g, rgb.b );
	}
}

void backlight_effect_indicators_set_colors( uint8_t index, HSV hsv )
{
	RGB rgb = hsv_to_rgb( hsv );
	if ( index == 254 )
	{
		backlight_set_color_all( rgb.r, rgb.g, rgb.b );
	}
	else
	{
		backlight_set_color( index, rgb.r, rgb.g, rgb.b );

		// If the spacebar LED is the indicator,
		// do the same for the spacebar stabilizers
		if ( index == 36+0 ) // LC0
		{
#ifdef CONFIG_ZEAL65
			backlight_set_color( 36+7, rgb.r, rgb.g, rgb.b ); // LC7
			backlight_set_color( 54+14, rgb.r, rgb.g, rgb.b ); // LD14
#else
			backlight_set_color( 36+6, rgb.r, rgb.g, rgb.b ); // LC6
			backlight_set_color( 54+13, rgb.r, rgb.g, rgb.b ); // LD13
			if ( g_config.use_7u_spacebar )
			{
				backlight_set_color( 54+14, rgb.r, rgb.g, rgb.b ); // LD14
			}
#endif
		}
	}
}

void backlight_effect_indicators(void)
{
	if ( g_config.caps_lock_indicator.index != 255 &&
			( g_indicator_state & (1<<USB_LED_CAPS_LOCK) ) )
	{
		backlight_effect_indicators_set_colors( g_config.caps_lock_indicator.index, g_config.caps_lock_indicator.color );
	}

	if ( IS_LAYER_ON(3) )
	{
		if ( g_config.layer_3_indicator.index != 255 )
		{
			backlight_effect_indicators_set_colors( g_config.layer_3_indicator.index, g_config.layer_3_indicator.color );
		}
	}
	else if ( IS_LAYER_ON(2) )
	{
		if ( g_config.layer_2_indicator.index != 255 )
		{
			backlight_effect_indicators_set_colors( g_config.layer_2_indicator.index, g_config.layer_2_indicator.color );
		}
	}
	else if ( IS_LAYER_ON(1) )
	{
		if ( g_config.layer_1_indicator.index != 255 )
		{
			backlight_effect_indicators_set_colors( g_config.layer_1_indicator.index, g_config.layer_1_indicator.color );
		}
	}
}

ISR(TIMER3_COMPA_vect)
{
	static uint8_t startup_tick = 0;
	if ( startup_tick < 20 )
	{
		startup_tick++;
		return;
	}

	g_tick++;

	if ( g_any_key_hit < 0xFFFFFFFF )
	{
		g_any_key_hit++;
	}

	for ( int led = 0; led < 72; led++ )
	{
		if ( g_key_hit[led] < 255 )
		{
			g_key_hit[led]++;
		}
	}

	// Factory default magic value
	if ( g_config.effect == 255 )
	{
		backlight_effect_rgb_test();
		return;
	}

	bool suspend_backlight = ((g_suspend_state && g_config.disable_when_usb_suspended) ||
			(g_config.disable_after_timeout > 0 && g_any_key_hit > g_config.disable_after_timeout * 60 * 20));
	uint8_t effect = suspend_backlight ? 0 : g_config.effect;

	static uint8_t effect_last = 255;
	bool initialize = effect != effect_last;
	effect_last = effect;

	switch ( effect )
	{
		case 0:
			backlight_effect_all_off();
			break;
		case 1:
			backlight_effect_solid_color();
			break;
		case 2:
			backlight_effect_alphas_mods();
			break;
		case 3:
			backlight_effect_gradient_up_down();
			break;
		case 4:
			backlight_effect_raindrops( initialize );
			break;
		case 5:
			backlight_effect_cycle_all();
			break;
		case 6:
			backlight_effect_cycle_left_right();
			break;
		case 7:
			backlight_effect_cycle_up_down();
			break;
		case 8:
			backlight_effect_jellybean_raindrops( initialize );
			break;
		case 9:
			backlight_effect_cycle_radial1();
			break;
		case 10:
			backlight_effect_cycle_radial2();
			break;
		default:
			backlight_effect_custom();
			break;
	}

	if ( ! suspend_backlight )
	{
		backlight_effect_indicators();
	}
}

void backlight_set_indicator_index( uint8_t *index, uint8_t row, uint8_t column )
{
	if ( row >= MATRIX_ROWS )
	{
		*index = row;
	}
	else
	{
		map_row_column_to_led( row, column, index );
	}
}

void backlight_config_set_values(msg_backlight_config_set_values *values)
{
	bool needs_init = (
			g_config.use_split_backspace != values->use_split_backspace ||
			g_config.use_split_left_shift != values->use_split_left_shift ||
			g_config.use_split_right_shift != values->use_split_right_shift ||
			g_config.use_7u_spacebar != values->use_7u_spacebar ||
			g_config.use_iso_enter != values->use_iso_enter ||
			g_config.disable_hhkb_blocker_leds != values->disable_hhkb_blocker_leds );

	g_config.use_split_backspace = values->use_split_backspace;
	g_config.use_split_left_shift = values->use_split_left_shift;
	g_config.use_split_right_shift = values->use_split_right_shift;
	g_config.use_7u_spacebar = values->use_7u_spacebar;
	g_config.use_iso_enter = values->use_iso_enter;
	g_config.disable_hhkb_blocker_leds = values->disable_hhkb_blocker_leds;

	g_config.disable_when_usb_suspended = values->disable_when_usb_suspended;
	g_config.disable_after_timeout = values->disable_after_timeout;

	g_config.brightness = values->brightness;
	g_config.effect = values->effect;
	g_config.effect_speed = values->effect_speed;
	g_config.color_1 = values->color_1;
	g_config.color_2 = values->color_2;
	g_config.caps_lock_indicator.color = values->caps_lock_indicator_color;
	backlight_set_indicator_index( &g_config.caps_lock_indicator.index, values->caps_lock_indicator_row, values->caps_lock_indicator_column );
	g_config.layer_1_indicator.color = values->layer_1_indicator_color;
	backlight_set_indicator_index( &g_config.layer_1_indicator.index, values->layer_1_indicator_row, values->layer_1_indicator_column );
	g_config.layer_2_indicator.color = values->layer_2_indicator_color;
	backlight_set_indicator_index( &g_config.layer_2_indicator.index, values->layer_2_indicator_row, values->layer_2_indicator_column );
	g_config.layer_3_indicator.color = values->layer_3_indicator_color;
	backlight_set_indicator_index( &g_config.layer_3_indicator.index, values->layer_3_indicator_row, values->layer_3_indicator_column );

	if ( needs_init )
	{
		backlight_init_drivers();
	}
}

void backlight_config_set_alphas_mods( uint16_t *alphas_mods )
{
	for ( int i=0; i<5; i++ )
	{
		g_config.alphas_mods[i] = alphas_mods[i];
	}

	backlight_config_save();
}

void backlight_config_load(void)
{
	eeprom_read_block( &g_config, EEPROM_BACKLIGHT_CONFIG_ADDR, sizeof(zeal_backlight_config) );
}

void backlight_config_save(void)
{
	eeprom_update_block( &g_config, EEPROM_BACKLIGHT_CONFIG_ADDR, sizeof(zeal_backlight_config) );
}

void backlight_init_drivers(void)
{
	i2c_init();
	IS31_init( ISSI_ADDR_1 );
	IS31_init( ISSI_ADDR_2 );

	for ( int index = 0; index < 72; index++ )
	{

#ifdef CONFIG_ZEAL65
		bool enabled = !( ( index == 18+5 && !g_config.use_split_backspace ) || // LB5
						  ( index == 36+15 && !g_config.use_split_left_shift ) || // LC15
						  ( index == 54+8 && !g_config.use_split_right_shift ) || // LD8
						  ( index == 36+6 ) || // LC6
						  ( index == 54+13 ) ); // LD13
#else
#ifdef M60_A_PROTO
bool enabled = !(
// LB6 LB7 LB8 LB15 LB16 LB17 not present on M60-A
				  ( index == 18+6 ) || // LB6
				  ( index == 18+7 ) || // LB7
				  ( index == 18+8 ) || // LB8
				  ( index == 18+15 ) || // LB15
				  ( index == 18+16 ) || // LB16
				  ( index == 18+17 ) || // LB17
// HHKB blockers (LC17, LD17) and ISO extra keys (LC15,LD13) not present on M60-A
				  ( index == 36+17 ) || // LC17
				  ( index == 54+17 ) || // LD17
				  ( index == 36+15 ) || // LC15
				  ( index == 54+13 ) ); // LD13
#else
// LB6 LB7 LB8 LB15 LB16 LB17 not present on Zeal60
bool enabled = !( ( index == 18+5 && !g_config.use_split_backspace ) || // LB5
				  ( index == 36+15 && !g_config.use_split_left_shift ) || // LC15
				  ( index == 54+8 && !g_config.use_split_right_shift ) || // LD8
				  ( index == 54+13 && g_config.use_7u_spacebar ) || // LD13
				  ( index == 36+17 && g_config.disable_hhkb_blocker_leds ) || // LC17
				  ( index == 54+17 && g_config.disable_hhkb_blocker_leds ) ||  // LD17
				  ( index == 18+6 ) || // LB6
				  ( index == 18+7 ) || // LB7
				  ( index == 18+8 ) || // LB8
				  ( index == 18+15 ) || // LB15
				  ( index == 18+16 ) || // LB16
				  ( index == 18+17 ) ); // LB17
#endif
#endif

	IS31_set_led_control_register( index, enabled, enabled, enabled );
	}
	IS31_update_led_control_registers( ISSI_ADDR_1, ISSI_ADDR_2 );
		for ( int led=0; led<72; led++ )
		{
			g_key_hit[led] = 255;
		}
	}

uint8_t incrementup( uint8_t value, uint8_t step, uint8_t min, uint8_t max )
{
	int16_t new_value = value;
	new_value += step;
	return MIN( MAX( new_value, min ), max );
}

uint8_t decrementdown( uint8_t value, uint8_t step, uint8_t min, uint8_t max )
{
 	int16_t new_value = value;
 	new_value -= step;
 	return MIN( MAX( new_value, min ), max );
}


void backlight_effect_increase(void)
{
 	g_config.effect = incrementup( g_config.effect, 1, 0, BACKLIGHT_EFFECT_MAX );
 	backlight_config_save();
}

void backlight_effect_decrease(void)
{
 	g_config.effect = decrementdown( g_config.effect, 1, 0, BACKLIGHT_EFFECT_MAX );
 	backlight_config_save();
}

void backlight_effect_speed_increase(void)
{
 	g_config.effect_speed = incrementup( g_config.effect_speed, 1, 0, 3 );
 	backlight_config_save();
}

void backlight_effect_speed_decrease(void)
{
 	g_config.effect_speed = decrementdown( g_config.effect_speed, 1, 0, 3 );
 	backlight_config_save();
}

void backlight_brightness_increase(void)
{
 	g_config.brightness = incrementup( g_config.brightness, 8, 0, 255 );
 	backlight_config_save();
}

void backlight_brightness_decrease(void)
{
 	g_config.brightness = decrementdown( g_config.brightness, 8, 0, 255 );
 	backlight_config_save();
}

void backlight_color_1_hue_increase(void)
{
	g_config.color_1.h = incrementup( g_config.color_1.h, 8, 0, 255 );
	backlight_config_save();
}

void backlight_color_1_hue_decrease(void)
{
 	g_config.color_1.h = decrementdown( g_config.color_1.h, 8, 0, 255 );
 	backlight_config_save();
}

void backlight_color_1_sat_increase(void)
{
	g_config.color_1.s = incrementup( g_config.color_1.s, 8, 0, 255 );
	backlight_config_save();
}

void backlight_color_1_sat_decrease(void)
{
	g_config.color_1.s = decrementdown( g_config.color_1.s, 8, 0, 255 );
	backlight_config_save();
}

void backlight_color_2_hue_increase(void)
{
	g_config.color_2.h = incrementup( g_config.color_2.h, 8, 0, 255 );
	backlight_config_save();
}

void backlight_color_2_hue_decrease(void)
{
	g_config.color_2.h = decrementdown( g_config.color_2.h, 8, 0, 255 );
	backlight_config_save();
}

void backlight_color_2_sat_increase(void)
{
	g_config.color_2.s = incrementup( g_config.color_2.s, 8, 0, 255 );
	backlight_config_save();
}

void backlight_color_2_sat_decrease(void)
{
	g_config.color_2.s = decrementdown( g_config.color_2.s, 8, 0, 255 );
	backlight_config_save();
}

void *backlight_get_custom_key_color_eeprom_address( uint8_t led )
{
	// 3 bytes per color
	return EEPROM_BACKLIGHT_KEY_COLOR_ADDR + ( led * 3 );
}

void backlight_get_key_color( uint8_t led, HSV *hsv )
{
	void *address = backlight_get_custom_key_color_eeprom_address( led );
	hsv->h = eeprom_read_byte(address);
	hsv->s = eeprom_read_byte(address+1);
	hsv->v = eeprom_read_byte(address+2);
}

void backlight_set_key_color( uint8_t row, uint8_t column, HSV hsv )
{
	uint8_t led;
	map_row_column_to_led( row, column, &led );
	if ( led < 72 )
	{
		void *address = backlight_get_custom_key_color_eeprom_address(led);
		eeprom_update_byte(address, hsv.h);
		eeprom_update_byte(address+1, hsv.s);
		eeprom_update_byte(address+2, hsv.v);
	}
}

void backlight_test_led( uint8_t index, bool red, bool green, bool blue )
{
	for ( int i=0; i<72; i++ )
	{
		if ( i == index )
		{
			IS31_set_led_control_register( i, red, green, blue );
		}
		else
		{
			IS31_set_led_control_register( i, false, false, false );
		}
	}
}

uint32_t backlight_get_tick(void)
{
	return g_tick;
}

void backlight_debug_led( bool state )
{
	if (state)
	{
		// Output high.
		DDRE |= (1<<6);
		PORTE |= (1<<6);
	}
	else
	{
		// Output low.
		DDRE &= ~(1<<6);
		PORTE &= ~(1<<6);
	}
}

#endif // BACKLIGHT_ENABLED