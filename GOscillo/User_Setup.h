// See SetupX_Template.h for all options available
#define USER_SETUP_ID 73

#define ST7789

#define TFT_MISO 19  // (leave TFT SDO disconnected if other SPI devices share MISO)
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15  // Chip select control pin
#define TFT_DC    2  // Data Command control pin
#define TFT_RST   4  // Reset pin (could connect to RST pin)

#define GPIO_PIN 33 // PWM-out - should not assign GPIO #36 throough #39

#define RXD2 16     // Hardware Serial 2 pins
#define TXD2 17

const int LCD_WIDTH = 170;
const int LCD_HEIGHT = 320;
const int LCD_YMAX = 120;
const int SAMPLES = 210;
const int NSAMP = 1024;
const int DISPLNG = 210;
const int DOTS_DIV = 15;
const int XOFF = 1;
const int YOFF = 20;
const int BOTTOM_LINE = YOFF + LCD_YMAX + 10;
const int TRIG_W = 12;
const int MENU = XOFF + SAMPLES + TRIG_W;

// Optional touch screen chip select
//#define TOUCH_CS 5 // Chip select pin (T_CS) of touch screen

#define LOAD_GLCD    // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2   // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
//#define LOAD_FONT4   // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
//#define LOAD_FONT6   // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
//#define LOAD_FONT7   // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
//#define LOAD_FONT8   // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
//#define LOAD_GFXFF   // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT

#define TFT_BLACK      ST77XX_BLACK
#define TFT_DARKGREY   0xd3d3  // DARKGREY
#define TFT_GREEN      ST77XX_GREEN
#define TFT_YELLOW     ST77XX_YELLOW
#define TFT_LIGHTGREY  0xd3d3  // LIGHTGREY
#define TFT_WHITE      ST77XX_WHITE
#define TFT_CYAN       ST77XX_CYAN
#define TFT_RED        ST77XX_RED

// TFT SPI clock frequency
// #define SPI_FREQUENCY  20000000
// #define SPI_FREQUENCY  27000000
#define SPI_FREQUENCY  40000000
// #define SPI_FREQUENCY  80000000

// Optional reduced SPI frequency for reading TFT
#define SPI_READ_FREQUENCY  16000000

// SPI clock frequency for touch controller
#define SPI_TOUCH_FREQUENCY  2500000
