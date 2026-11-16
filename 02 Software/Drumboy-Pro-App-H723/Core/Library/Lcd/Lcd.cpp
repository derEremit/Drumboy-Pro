#include "Lcd.h"

Lcd::Lcd(LcdRotation rotation_, RGB16Color foreColor_, RGB16Color backColor_, LcdFont font_)
    : foreColor(foreColor_),
      backColor(backColor_),
      rotation(rotation_),
      fontAlignment(LEFT) {
    // set dimensions based on rotation
    switch (rotation) {
        case 0:
            width = kLCD_WIDTH;
            height = kLCD_HEIGHT;
            break;
        case 1:
            width = kLCD_HEIGHT;
            height = kLCD_WIDTH;
            break;
        case 2:
            width = kLCD_WIDTH;
            height = kLCD_HEIGHT;
            break;
        case 3:
            width = kLCD_HEIGHT;
            height = kLCD_WIDTH;
            break;
    }
    setFont(font_);
}

Lcd::~Lcd() {}

////////////////////////////////////////////////////////////////////////////////
/* Private Functions ---------------------------------------------------------*/
////////////////////////////////////////////////////////////////////////////////

void Lcd::setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    /// @brief Configures LCD address boundaries for subsequent pixel writes.

    // 1. CHIP SELECT ASSERTION
    // Select LCD bus before address transaction.
    LCD_CS_LOW;

    // 2. COLUMN START ADDRESS (X0)
    // Send high and low bytes for start column.

    // High byte
    LCD_RS_CMD;
    LCD_DATA_GPIO_Port->ODR = LCD_SET_COLUMN_ADDRESS_0;
    LCD_WR_STROBE;
    LCD_RS_DATA;
    LCD_DATA_GPIO_Port->ODR = (x0 >> 8);
    LCD_WR_STROBE;

    // Low byte
    LCD_RS_CMD;
    LCD_DATA_GPIO_Port->ODR = LCD_SET_COLUMN_ADDRESS_1;
    LCD_WR_STROBE;
    LCD_RS_DATA;
    LCD_DATA_GPIO_Port->ODR = (x0 & 0xFF);
    LCD_WR_STROBE;

    // 3. COLUMN END ADDRESS (X1)
    // Send high and low bytes for end column.

    // High byte
    LCD_RS_CMD;
    LCD_DATA_GPIO_Port->ODR = LCD_SET_COLUMN_ADDRESS_2;
    LCD_WR_STROBE;
    LCD_RS_DATA;
    LCD_DATA_GPIO_Port->ODR = (x1 >> 8);
    LCD_WR_STROBE;

    // Low byte
    LCD_RS_CMD;
    LCD_DATA_GPIO_Port->ODR = LCD_SET_COLUMN_ADDRESS_3;
    LCD_WR_STROBE;
    LCD_RS_DATA;
    LCD_DATA_GPIO_Port->ODR = (x1 & 0xFF);
    LCD_WR_STROBE;

    // 4. ROW START ADDRESS (Y0)
    // Send high and low bytes for start row.

    // High byte
    LCD_RS_CMD;
    LCD_DATA_GPIO_Port->ODR = LCD_SET_ROW_ADDRESS_0;
    LCD_WR_STROBE;
    LCD_RS_DATA;
    LCD_DATA_GPIO_Port->ODR = (y0 >> 8);
    LCD_WR_STROBE;

    // Low byte
    LCD_RS_CMD;
    LCD_DATA_GPIO_Port->ODR = LCD_SET_ROW_ADDRESS_1;
    LCD_WR_STROBE;
    LCD_RS_DATA;
    LCD_DATA_GPIO_Port->ODR = (y0 & 0xFF);
    LCD_WR_STROBE;

    // 5. ROW END ADDRESS (Y1)
    // Send high and low bytes for end row.

    // High byte
    LCD_RS_CMD;
    LCD_DATA_GPIO_Port->ODR = LCD_SET_ROW_ADDRESS_2;
    LCD_WR_STROBE;
    LCD_RS_DATA;
    LCD_DATA_GPIO_Port->ODR = (y1 >> 8);
    LCD_WR_STROBE;

    // Low byte
    LCD_RS_CMD;
    LCD_DATA_GPIO_Port->ODR = LCD_SET_ROW_ADDRESS_3;
    LCD_WR_STROBE;
    LCD_RS_DATA;
    LCD_DATA_GPIO_Port->ODR = (y1 & 0xFF);
    LCD_WR_STROBE;

    // 6. MEMORY WRITE PREPARATION
    // Commit window and switch controller to write mode.
    LCD_RS_CMD;
    LCD_DATA_GPIO_Port->ODR = LCD_WRITE_MEMORY_START;
    LCD_WR_STROBE;

    // Switch RS to data mode for pixel payload.
    LCD_RS_DATA;

    // Release LCD bus after address setup.
    LCD_CS_HIGH;
}

void Lcd::clearAddressWindow() {
    /// @brief Resets the active address window to full-screen bounds.

    // 1. FULL-SCREEN WINDOW RESET
    // Restore address window to full panel bounds.
    setAddressWindow(0, 0, width - 1, height - 1);
}

void Lcd::fastFill(RGB16Color color, uint32_t pixel) {
    /// @brief Fills a pixel region with a single color using a high-speed write loop.

    // 1. CHIP SELECT ASSERTION
    // Select LCD bus for burst pixel write.
    LCD_CS_LOW;

    // 2. DATA BUS PRELOAD
    // Put fill color on data pins once for repeated strobes.
    LCD_DATA_GPIO_Port->ODR = color;

    // 3. WRITE REGISTER CACHING
    // Cache WR register address and pin masks for throughput.
    volatile uint32_t* wr_reg = &(LCD_WR_GPIO_Port->BSRR);

    // Precompute BSRR set/reset masks.
    const uint32_t wr_set = LCD_WR_Pin;
    const uint32_t wr_reset = LCD_WR_Pin << 16U;

    // 4. BLOCK FILL LOOP
    // Write 32 pixels per iteration using manual unrolling.
    uint32_t blocks = pixel / 32;

    while (blocks--) {
        // Toggle WR pin 32 times.
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;

        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;

        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;

        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
    }

    // 5. REMAINING PIXELS
    // Flush tail pixels after block loop.
    uint32_t remaining = pixel % 32;

    while (remaining--) {
        *wr_reg = wr_reset;
        *wr_reg = wr_set;
    }

    // 6. CHIP SELECT RELEASE
    // End transfer and free LCD bus.
    LCD_CS_HIGH;
}

uint8_t Lcd::getLetter(char letter) {
    /// @brief Maps an ASCII character to the internal font glyph index.

    // 1. LOOKUP TABLE DEFINITION
    // Map ASCII code points to glyph indices.
    static const uint8_t fontMap[128] = {
        /* 00-09 */ 52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
        /* 10-19 */ 52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
        /* 20-29 */ 52, 52, 52, 52, 52, 52, 52, 52, 52, 52,
        /* 30-39 */ 52, 52, 52, 51 /*!*/, 52, 50 /*#*/, 52, 39 /*%*/, 52, 52,
        /* 40-49 */ 52, 52, 52, 38 /*+*/, 41 /*,*/, 37 /*-*/, 40 /*.*/, 36 /*/*/, 0, 1,
        /* 50-59 */ 2, 3, 4, 5, 6, 7, 8, 9, 42 /*:*/, 52,
        /* 60-69 */ 43 /*<*/, 52, 44 /*>*/, 49 /*?*/, 52, 10 /*A*/, 11, 12, 13, 14,
        /* 70-79 */ 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
        /* 80-89 */ 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
        /* 90-99 */ 35 /*Z*/, 45 /*[*/, 52, 46 /*]*/, 52, 47 /*_*/, 52, 10 /*a*/, 11, 12,
        /*100-109*/ 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        /*110-119*/ 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
        /*120-129*/ 33, 34, 35 /*z*/, 52, 48 /*|*/, 52, 52, 52  // ...rest are 52
    };

    // 2. INPUT NORMALIZATION
    // Convert signed char to unsigned index safely.
    unsigned char index = (unsigned char)letter;

    // 3. BOUNDARY CHECK
    // Return default glyph for out-of-range values.
    if (index > 127) {
        return 52;
    }

    // 4. GLYPH INDEX RETURN
    // Return mapped glyph index for renderer.
    return fontMap[index];
}

RGB16Color Lcd::alphaColor(RGB16Color color_, uint8_t alpha) {
    /// @brief Applies alpha scaling to an RGB565 color value.

    // 1. RGB565 CHANNEL EXTRACTION
    // Split 16-bit packed color into channel components.
    uint32_t r = (color_ >> 11);        // 5-bit Red
    uint32_t g = (color_ >> 5) & 0x3F;  // 6-bit Green
    uint32_t b = (color_ & 0x1F);       // 5-bit Blue

    // 2. ALPHA SCALING
    // Apply alpha factor to each channel.
    r = (r * alpha) >> 8;
    g = (g * alpha) >> 8;
    b = (b * alpha) >> 8;

    // 3. RGB565 REPACK
    // Recompose scaled channels into 16-bit color.
    return (RGB16Color)((r << 11) | (g << 5) | b);
}

RGB16Color Lcd::alphaColor(struct RGB24RawColor color_, uint8_t alpha) {
    /// @brief Applies alpha scaling to an RGB24 color and converts it to RGB565.

    // 1. CHANNEL CONVERSION WITH ALPHA
    // Scale 24-bit channels and convert to RGB565 bit depth.
    uint16_t r = (color_.red * alpha) >> 11;    // Red: 8-bit -> 5-bit with Alpha
    uint16_t g = (color_.green * alpha) >> 10;  // Green: 8-bit -> 6-bit with Alpha
    uint16_t b = (color_.blue * alpha) >> 11;   // Blue: 8-bit -> 5-bit with Alpha

    // 2. RGB565 PACK
    // Pack converted channels into output color.
    return (RGB16Color)((r << 11) | (g << 5) | b);
}

inline RGB16Color Lcd::convertColor(struct RGB24RawColor color_) {
    /// @brief Converts a raw RGB24 color to RGB565 format.

    // 1. RGB888 TO RGB565 CONVERSION
    // Drop lower bits from each channel to fit RGB565.
    uint16_t r = (color_.red >> 3);    // 8-bit -> 5-bit
    uint16_t g = (color_.green >> 2);  // 8-bit -> 6-bit
    uint16_t b = (color_.blue >> 3);   // 8-bit -> 5-bit

    // 2. RGB565 PACK
    // Pack converted channel values into 16-bit color.
    return (RGB16Color)((r << 11) | (g << 5) | b);
}

////////////////////////////////////////////////////////////////////////////////
/* Public Functions ----------------------------------------------------------*/
////////////////////////////////////////////////////////////////////////////////

void Lcd::setRotation(LcdRotation rotation_) {
    /// @brief Applies display rotation mode and updates logical panel dimensions.

    // 1. ROTATION STATE UPDATE
    // Store new rotation and prepare address mode command.
    rotation = rotation_;
    writeCommand(LCD_SET_ADDRESS_MODE);

    // 2. PANEL GEOMETRY CONFIGURATION
    // Apply controller mode bits and update logical width/height.
    switch (rotation) {
        case PORTRAIT_0:
            writeData(0x00);
            width = kLCD_WIDTH;
            height = kLCD_HEIGHT;
            break;
        case LANDSCAPE_0:
            writeData(MADCTL_MV | MADCTL_MX);
            width = kLCD_HEIGHT;
            height = kLCD_WIDTH;
            break;
        case PORTRAIT_1:
            writeData(MADCTL_MX | MADCTL_MY);
            width = kLCD_WIDTH;
            height = kLCD_HEIGHT;
            break;
        case LANDSCAPE_1:
            writeData(MADCTL_MV | MADCTL_MY);
            width = kLCD_HEIGHT;
            height = kLCD_WIDTH;
            break;
    }
}

void Lcd::setFont(LcdFont font_) {
    /// @brief Selects the active font and binds its glyph metrics.

    // 1. FONT STATE UPDATE
    // Store selected font identifier.
    font = font_;

    // 2. FONT TABLE BINDING
    // Bind glyph table and metrics for the selected font.
    switch (font) {
        case FONT_05x07:
            fontData = &kFontData_05x07[0][0];
            fontWidth = kFontWidth_05x07;
            fontHeight = kFontHeight_05x07;
            fontSpacing = kFontSpacing_05x07;
            break;
        case FONT_07x09:
            fontData = &kFontData_07x09[0][0];
            fontWidth = kFontWidth_07x09;
            fontHeight = kFontHeight_07x09;
            fontSpacing = kFontSpacing_07x09;
            break;
        case FONT_10x14:
            fontData = &kFontData_10x14[0][0];
            fontWidth = kFontWidth_10x14;
            fontHeight = kFontHeight_10x14;
            fontSpacing = kFontSpacing_10x14;
            break;
        case FONT_14x18:
            fontData = &kFontData_14x18[0][0];
            fontWidth = kFontWidth_14x18;
            fontHeight = kFontHeight_14x18;
            fontSpacing = kFontSpacing_14x18;
            break;
    }
}

void Lcd::initialize() {
    /// @brief Initializes LCD controller registers and enables display output.

    // 1. BUS AND BACKLIGHT PREP
    // Prepare LCD control lines before reset sequence.
    LCD_BL_LOW;
    LCD_RD_HIGH;
    LCD_WR_HIGH;
    LCD_CS_LOW;

    // 2. HARD RESET SEQUENCE
    // Issue hardware reset pulse with required timing.
    HAL_Delay(250);
    HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_SET);
    HAL_Delay(120);

    // 3. NT35516 CORE INITIALIZATION
    // Send controller startup command stream.
    writeCommand(0xFF);
    writeData(0xAA);
    writeData(0x55);
    writeData(0x25);
    writeData(0x01);
    writeCommand(0xF2);
    writeData(0x00);
    writeData(0x00);
    writeData(0x4A);
    writeData(0x0A);
    writeData(0xA8);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x0B);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x40);
    writeData(0x01);
    writeData(0x51);
    writeData(0x00);
    writeData(0x01);
    writeData(0x00);
    writeData(0x01);
    writeCommand(0xF3);
    writeData(0x02);
    writeData(0x03);
    writeData(0x07);
    writeData(0x45);
    writeData(0x88);
    writeData(0xD1);
    writeData(0x0D);

    // 4. PAGE 0 CONFIGURATION
    // Program base timing and panel registers.
    writeCommand(0xF0);
    writeData(0x55);
    writeData(0xAA);
    writeData(0x52);
    writeData(0x08);
    writeData(0x00);
    writeCommand(0xB1);
    writeData(0xFC);
    writeData(0x00);
    writeData(0x00);
    writeCommand(0xB8);
    writeData(0x01);
    writeData(0x02);
    writeData(0x02);
    writeData(0x02);
    writeCommand(0xBB);
    writeData(0x63);
    writeData(0x03);
    writeData(0x63);
    writeCommand(0xC9);
    writeData(0x63);
    writeData(0x06);
    writeData(0x0D);
    writeData(0x1A);
    writeData(0x17);
    writeData(0x00);

    // 5. PAGE 1 CONFIGURATION
    // Program extended analog/gamma support registers.
    writeCommand(0xF0);
    writeData(0x55);
    writeData(0xAA);
    writeData(0x52);
    writeData(0x08);
    writeData(0x01);
    writeCommand(0xB0);
    writeData(0x05);
    writeData(0x05);
    writeData(0x05);
    writeCommand(0xB1);
    writeData(0x05);
    writeData(0x05);
    writeData(0x05);
    writeCommand(0xB2);
    writeData(0x01);
    writeData(0x01);
    writeData(0x01);
    writeCommand(0xB3);
    writeData(0x0E);
    writeData(0x0E);
    writeData(0x0E);
    writeCommand(0xB4);
    writeData(0x0A);
    writeData(0x0A);
    writeData(0x0A);
    writeCommand(0xB6);
    writeData(0x44);
    writeData(0x44);
    writeData(0x44);
    writeCommand(0xB7);
    writeData(0x34);
    writeData(0x34);
    writeData(0x34);
    writeCommand(0xB8);
    writeData(0x20);
    writeData(0x20);
    writeData(0x20);
    writeCommand(0xB9);
    writeData(0x26);
    writeData(0x26);
    writeData(0x26);
    writeCommand(0xBA);
    writeData(0x24);
    writeData(0x24);
    writeData(0x24);
    writeCommand(0xBC);
    writeData(0x00);
    writeData(0xC8);
    writeData(0x00);
    writeCommand(0xBD);
    writeData(0x00);
    writeData(0xC8);
    writeData(0x00);
    writeCommand(0xBE);
    writeData(0x92);
    writeCommand(0xC0);
    writeData(0x00);
    writeData(0x08);
    writeCommand(0xCA);
    writeData(0x00);
    
    // 6. GAMMA CONFIGURATION
    // Load gamma curves for display channels.
    writeCommand(0xD0);
    writeData(0x0A);
    writeData(0x10);
    writeData(0x0D);
    writeData(0x0F);
    writeCommand(0xD1);
    writeData(0x00);
    writeData(0x70);
    writeData(0x01);
    writeData(0x16);
    writeData(0x01);
    writeData(0x2A);
    writeData(0x01);
    writeData(0x48);
    writeData(0x01);
    writeData(0x61);
    writeData(0x01);
    writeData(0x7d);
    writeData(0x01);
    writeData(0x94);
    writeData(0x01);
    writeData(0xb7);
    writeCommand(0xD5);
    writeData(0x00);
    writeData(0x70);
    writeData(0x01);
    writeData(0x16);
    writeData(0x01);
    writeData(0x2A);
    writeData(0x01);
    writeData(0x48);
    writeData(0x01);
    writeData(0x61);
    writeData(0x01);
    writeData(0x7d);
    writeData(0x01);
    writeData(0x94);
    writeData(0x01);
    writeData(0xb7);
    writeCommand(0xD9);
    writeData(0x00);
    writeData(0x70);
    writeData(0x01);
    writeData(0x16);
    writeData(0x01);
    writeData(0x2A);
    writeData(0x01);
    writeData(0x48);
    writeData(0x01);
    writeData(0x61);
    writeData(0x01);
    writeData(0x7d);
    writeData(0x01);
    writeData(0x94);
    writeData(0x01);
    writeData(0xb7);
    writeCommand(0xE0);
    writeData(0x00);
    writeData(0x70);
    writeData(0x01);
    writeData(0x16);
    writeData(0x01);
    writeData(0x2A);
    writeData(0x01);
    writeData(0x48);
    writeData(0x01);
    writeData(0x61);
    writeData(0x01);
    writeData(0x7d);
    writeData(0x01);
    writeData(0x94);
    writeData(0x01);
    writeData(0xb7);
    writeCommand(0xE4);
    writeData(0x00);
    writeData(0x70);
    writeData(0x01);
    writeData(0x16);
    writeData(0x01);
    writeData(0x2A);
    writeData(0x01);
    writeData(0x48);
    writeData(0x01);
    writeData(0x61);
    writeData(0x01);
    writeData(0x7d);
    writeData(0x01);
    writeData(0x94);
    writeData(0x01);
    writeData(0xb7);
    writeCommand(0xE8);
    writeData(0x00);
    writeData(0x70);
    writeData(0x01);
    writeData(0x16);
    writeData(0x01);
    writeData(0x2A);
    writeData(0x01);
    writeData(0x48);
    writeData(0x01);
    writeData(0x61);
    writeData(0x01);
    writeData(0x7d);
    writeData(0x01);
    writeData(0x94);
    writeData(0x01);
    writeData(0xb7);
    writeCommand(0xD2);
    writeData(0x01);
    writeData(0xd1);
    writeData(0x01);
    writeData(0xfe);
    writeData(0x02);
    writeData(0x1f);
    writeData(0x02);
    writeData(0x55);
    writeData(0x02);
    writeData(0x80);
    writeData(0x02);
    writeData(0x81);
    writeData(0x02);
    writeData(0xad);
    writeData(0x02);
    writeData(0xdf);
    writeCommand(0xD6);
    writeData(0x01);
    writeData(0xd1);
    writeData(0x01);
    writeData(0xfe);
    writeData(0x02);
    writeData(0x1f);
    writeData(0x02);
    writeData(0x55);
    writeData(0x02);
    writeData(0x80);
    writeData(0x02);
    writeData(0x81);
    writeData(0x02);
    writeData(0xad);
    writeData(0x02);
    writeData(0xdf);
    writeCommand(0xDD);
    writeData(0x01);
    writeData(0xd1);
    writeData(0x01);
    writeData(0xfe);
    writeData(0x02);
    writeData(0x1f);
    writeData(0x02);
    writeData(0x55);
    writeData(0x02);
    writeData(0x80);
    writeData(0x02);
    writeData(0x81);
    writeData(0x02);
    writeData(0xad);
    writeData(0x02);
    writeData(0xdf);
    writeCommand(0xE1);
    writeData(0x01);
    writeData(0xd1);
    writeData(0x01);
    writeData(0xfe);
    writeData(0x02);
    writeData(0x1f);
    writeData(0x02);
    writeData(0x55);
    writeData(0x02);
    writeData(0x80);
    writeData(0x02);
    writeData(0x81);
    writeData(0x02);
    writeData(0xad);
    writeData(0x02);
    writeData(0xdf);
    writeCommand(0xE5);
    writeData(0x01);
    writeData(0xd1);
    writeData(0x01);
    writeData(0xfe);
    writeData(0x02);
    writeData(0x1f);
    writeData(0x02);
    writeData(0x55);
    writeData(0x02);
    writeData(0x80);
    writeData(0x02);
    writeData(0x81);
    writeData(0x02);
    writeData(0xad);
    writeData(0x02);
    writeData(0xdf);
    writeCommand(0xE9);
    writeData(0x01);
    writeData(0xd1);
    writeData(0x01);
    writeData(0xfe);
    writeData(0x02);
    writeData(0x1f);
    writeData(0x02);
    writeData(0x55);
    writeData(0x02);
    writeData(0x80);
    writeData(0x02);
    writeData(0x81);
    writeData(0x02);
    writeData(0xad);
    writeData(0x02);
    writeData(0xdf);
    writeCommand(0xD3);
    writeData(0x02);
    writeData(0xf7);
    writeData(0x03);
    writeData(0x1f);
    writeData(0x03);
    writeData(0x3a);
    writeData(0x03);
    writeData(0x59);
    writeData(0x03);
    writeData(0x70);
    writeData(0x03);
    writeData(0x8b);
    writeData(0x03);
    writeData(0x99);
    writeData(0x03);
    writeData(0xae);
    writeCommand(0xD7);
    writeData(0x02);
    writeData(0xf7);
    writeData(0x03);
    writeData(0x1f);
    writeData(0x03);
    writeData(0x3a);
    writeData(0x03);
    writeData(0x59);
    writeData(0x03);
    writeData(0x70);
    writeData(0x03);
    writeData(0x8b);
    writeData(0x03);
    writeData(0x99);
    writeData(0x03);
    writeData(0xae);
    writeCommand(0xDE);
    writeData(0x02);
    writeData(0xf7);
    writeData(0x03);
    writeData(0x1f);
    writeData(0x03);
    writeData(0x3a);
    writeData(0x03);
    writeData(0x59);
    writeData(0x03);
    writeData(0x70);
    writeData(0x03);
    writeData(0x8b);
    writeData(0x03);
    writeData(0x99);
    writeData(0x03);
    writeData(0xae);
    writeCommand(0xE2);
    writeData(0x02);
    writeData(0xf7);
    writeData(0x03);
    writeData(0x1f);
    writeData(0x03);
    writeData(0x3a);
    writeData(0x03);
    writeData(0x59);
    writeData(0x03);
    writeData(0x70);
    writeData(0x03);
    writeData(0x8b);
    writeData(0x03);
    writeData(0x99);
    writeData(0x03);
    writeData(0xae);
    writeCommand(0xE6);
    writeData(0x02);
    writeData(0xf7);
    writeData(0x03);
    writeData(0x1f);
    writeData(0x03);
    writeData(0x3a);
    writeData(0x03);
    writeData(0x59);
    writeData(0x03);
    writeData(0x70);
    writeData(0x03);
    writeData(0x8b);
    writeData(0x03);
    writeData(0x99);
    writeData(0x03);
    writeData(0xae);
    writeCommand(0xEA);
    writeData(0x02);
    writeData(0xf7);
    writeData(0x03);
    writeData(0x1f);
    writeData(0x03);
    writeData(0x3a);
    writeData(0x03);
    writeData(0x59);
    writeData(0x03);
    writeData(0x70);
    writeData(0x03);
    writeData(0x8b);
    writeData(0x03);
    writeData(0x99);
    writeData(0x03);
    writeData(0xae);
    writeCommand(0xD4);
    writeData(0x03);
    writeData(0xFf);
    writeData(0x03);
    writeData(0xFF);
    writeCommand(0xD8);
    writeData(0x03);
    writeData(0xFf);
    writeData(0x03);
    writeData(0xFF);
    writeCommand(0xDF);
    writeData(0x03);
    writeData(0xFf);
    writeData(0x03);
    writeData(0xFF);
    writeCommand(0xE3);
    writeData(0x03);
    writeData(0xFf);
    writeData(0x03);
    writeData(0xFF);
    writeCommand(0xE7);
    writeData(0x03);
    writeData(0xFf);
    writeData(0x03);
    writeData(0xFF);
    writeCommand(0xEB);
    writeData(0x03);
    writeData(0xFf);
    writeData(0x03);
    writeData(0xFF);

    // 7. SLEEP OUT AND DISPLAY ENABLE
    // Exit sleep mode and enable pixel output.
    writeCommand(0x1100);
    HAL_Delay(120);
    writeCommand(0x2900);
    HAL_Delay(100);
    writeCommand(0x3A00);
    writeData(0x55);
    setRotation(rotation);
    writeCommand(LCD_SET_DISPLAY_ON);
    clearScreen();
}

void Lcd::clearScreen() {
    /// @brief Clears the full screen using the current background color.

    // 1. BACKGROUND CLEAR
    // Fill full screen with configured background color.
    fillScreen(backColor);
}

void Lcd::fillScreen() {
    /// @brief Fills the full screen using the current foreground color.

    // 1. FOREGROUND FILL
    // Fill full screen with configured foreground color.
    fillScreen(foreColor);
}

void Lcd::fillScreen(RGB16Color color) {
    /// @brief Fills the full screen with a specified RGB565 color.

    // 1. FULL-SCREEN COLOR FILL
    // Fill full display area with the requested color.
    clearAddressWindow();
    LCD_RS_HIGH;
    fastFill(color, width * height);
}

void Lcd::drawPixel(uint16_t x, uint16_t y) {
    /// @brief Draws a single pixel at the given coordinates.

    // 1. SINGLE PIXEL DRAW
    // Draw one foreground pixel when coordinates are valid.
    if ((x < width) && (y < height)) {
        setAddressWindow(x, y, x, y);
        LCD_RS_HIGH;
        LCD_CS_LOW;
        LCD_DATA_GPIO_Port->ODR = foreColor;
        LCD_WR_LOW;
        LCD_WR_HIGH;
        LCD_CS_HIGH;
    }
}

void Lcd::drawHLine(uint16_t x, uint16_t y, int16_t l) {
    /// @brief Draws a horizontal line with clipping support.

    if (l == 0 || y >= height) return;
    uint16_t startX = x;
    uint16_t absL = (l > 0) ? (uint16_t)l : (uint16_t)(-l);
    if (l < 0) {
        if (absL > x + 1) {
            absL = x + 1;
            startX = 0;
        } else {
            startX = x - absL + 1;
        }
    }
    if (startX >= width) return;
    if (startX + absL > width) {
        absL = width - startX;
    }
    if (absL > 0) {
        setAddressWindow(startX, y, startX + absL - 1, y);
        LCD_RS_HIGH;
        fastFill(foreColor, absL);
    }
}

void Lcd::drawVLine(uint16_t x, uint16_t y, int16_t l) {
    /// @brief Draws a vertical line with clipping support.

    if (l == 0 || x >= width) return;
    uint16_t startY = y;
    uint16_t absL = (l > 0) ? (uint16_t)l : (uint16_t)(-l);
    if (l < 0) {
        if (absL > y + 1) {
            absL = y + 1;
            startY = 0;
        } else {
            startY = y - absL + 1;
        }
    }
    if (startY >= height) return;
    if (startY + absL > height) {
        absL = height - startY;
    }
    if (absL > 0) {
        setAddressWindow(x, startY, x, startY + absL - 1);
        LCD_RS_HIGH;
        fastFill(foreColor, absL);
    }
}

void Lcd::drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    /// @brief Draws a line segment between two points using Bresenham rasterization.

    // 1. BRESENHAM LINE DRAW
    // Draw line segment using integer rasterization.
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        swap(x0, y0);
        swap(x1, y1);
    }
    if (x0 > x1) {
        swap(x0, x1);
        swap(y0, y1);
    }
    int16_t dx = x1 - x0;
    int16_t dy = abs(y1 - y0);
    int16_t err = dx / 2;
    int16_t ystep;
    (y0 < y1) ? ystep = 1 : ystep = -1;
    for (; x0 <= x1; x0++) {
        (steep) ? drawPixel(y0, x0) : drawPixel(x0, y0);
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

void Lcd::drawRect(uint16_t x, uint16_t y, uint32_t w, uint32_t h) {
    /// @brief Draws a rectangle outline.

    // 1. RECTANGLE OUTLINE DRAW
    // Draw rectangle border using four line primitives.
    if ((x < width) && (y < height)) {
        if ((x + w - 1) >= width) w = width - x;
        if ((y + h - 1) >= height) h = height - y;
        drawHLine(x, y, w);
        drawHLine(x, y + h - 1, w);
        drawVLine(x, y, h);
        drawVLine(x + w - 1, y, h);
    }
}

void Lcd::fillRect(uint16_t x, uint16_t y, uint32_t w, uint32_t h) {
    /// @brief Fills a rectangular area with foreground color.

    // 1. RECTANGLE AREA FILL
    // Fill rectangle interior with foreground color.
    if ((x < width) && (y < height)) {
        if ((x + w - 1) >= width) w = width - x;
        if ((y + h - 1) >= height) h = height - y;
        setAddressWindow(x, y, x + w - 1, y + h - 1);
        LCD_RS_HIGH;
        fastFill(foreColor, w * h);
    }
}

void Lcd::drawCircle(uint16_t x, uint16_t y, uint16_t r) {
    /// @brief Draws a circle outline centered at the given coordinates.

    // 1. MIDPOINT CIRCLE DRAW
    // Draw full circle outline using midpoint algorithm.
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t xc = 0;
    int16_t yc = r;
    drawPixel(x, y + r);
    drawPixel(x, y - r);
    drawPixel(x + r, y);
    drawPixel(x - r, y);
    while (xc < yc) {
        if (f >= 0) {
            yc--;
            ddF_y += 2;
            f += ddF_y;
        }
        xc++;
        ddF_x += 2;
        f += ddF_x;
        drawPixel(x + xc, y + yc);
        drawPixel(x - xc, y + yc);
        drawPixel(x + xc, y - yc);
        drawPixel(x - xc, y - yc);
        drawPixel(x + yc, y + xc);
        drawPixel(x - yc, y + xc);
        drawPixel(x + yc, y - xc);
        drawPixel(x - yc, y - xc);
    }
}

void Lcd::drawCircleHelper(uint16_t x, uint16_t y, uint16_t r, uint8_t cornername) {
    /// @brief Draws selected circle quadrants based on a corner mask.

    // 1. CIRCLE QUADRANT DRAW
    // Draw selected quadrants based on corner mask.
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t xh = 0;
    int16_t yh = r;
    while (xh < yh) {
        if (f >= 0) {
            yh--;
            ddF_y += 2;
            f += ddF_y;
        }
        xh++;
        ddF_x += 2;
        f += ddF_x;
        if (cornername & 0x4) {
            drawPixel(x + xh, y + yh);
            drawPixel(x + yh, y + xh);
        }
        if (cornername & 0x2) {
            drawPixel(x + xh, y - yh);
            drawPixel(x + yh, y - xh);
        }
        if (cornername & 0x8) {
            drawPixel(x - yh, y + xh);
            drawPixel(x - xh, y + yh);
        }
        if (cornername & 0x1) {
            drawPixel(x - yh, y - xh);
            drawPixel(x - xh, y - yh);
        }
    }
}

void Lcd::drawQuarterCircle(uint16_t x, uint16_t y, uint16_t r, uint8_t quarter) {
    /// @brief Draws one quarter arc of a circle.

    // 1. QUARTER ARC RASTER SETUP
    // Initialize midpoint parameters for selected quarter.
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t xc = 0;
    int16_t yc = r;

    // 2. CARDINAL START PIXELS
    // Draw first two anchor pixels of selected quarter.
    switch (quarter) {
        case 0:
            drawPixel(x, y - r);
            drawPixel(x + r, y);
            break;
        case 1:
            drawPixel(x, y - r);
            drawPixel(x - r, y);
            break;
        case 2:
            drawPixel(x, y + r);
            drawPixel(x - r, y);
            break;
        case 3:
            drawPixel(x, y + r);
            drawPixel(x + r, y);
            break;
    }

    // 3. MIDPOINT LOOP
    // Trace quarter arc with midpoint updates.
    while (xc < yc) {
        if (f >= 0) {
            yc--;
            ddF_y += 2;
            f += ddF_y;
        }
        xc++;
        ddF_x += 2;
        f += ddF_x;
        switch (quarter) {
            case 0:
                drawPixel(x + xc, y - yc);
                drawPixel(x + yc, y - xc);
                break;
            case 1:
                drawPixel(x - xc, y - yc);
                drawPixel(x - yc, y - xc);
                break;
            case 2:
                drawPixel(x - xc, y + yc);
                drawPixel(x - yc, y + xc);
                break;
            case 3:
                drawPixel(x + xc, y + yc);
                drawPixel(x + yc, y + xc);
                break;
        }
    }
}

void Lcd::drawHalfCircle(uint16_t x, uint16_t y, uint16_t r, uint8_t half) {
    /// @brief Draws one half of a circle by combining two quarter arcs.

    // 1. HALF SELECT MAPPING
    // Map half selector to two quarter IDs.
    uint8_t h0, h1;

    switch (half) {
        case 0:
            h0 = 0;
            h1 = 1;
            break;
        case 1:
            h0 = 1;
            h1 = 2;
            break;
        case 2:
            h0 = 2;
            h1 = 3;
            break;
        case 3:
            h0 = 3;
            h1 = 0;
            break;
        default:
            h0 = 0;
            h1 = 1;
            break;
    }

    // 2. QUARTER COMPOSITION
    // Render two quarter arcs to form the half circle.
    drawQuarterCircle(x, y, r, h0);
    drawQuarterCircle(x, y, r, h1);
}

void Lcd::fillCircle(uint16_t x, uint16_t y, uint16_t r) {
    /// @brief Fills a circle area centered at the given coordinates.

    // 1. CIRCLE AREA FILL
    // Fill circle interior with vertical spans.
    drawVLine(x, y - r, 2 * r + 1);
    fillCircleHelper(x, y, r, 3, 0);
}

void Lcd::fillCircleHelper(uint16_t x, uint16_t y, uint16_t r, uint8_t cornername, uint16_t delta) {
    /// @brief Fills selected circle sectors with vertical spans.

    // 1. FILL HELPER SETUP
    // Initialize midpoint state for span filling.
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t xh = 0;
    int16_t yh = r;

    // 2. VERTICAL SPAN RASTERIZATION
    // Draw mirrored vertical spans for selected corners.
    while (xh < yh) {
        if (f >= 0) {
            yh--;
            ddF_y += 2;
            f += ddF_y;
        }
        xh++;
        ddF_x += 2;
        f += ddF_x;
        if (cornername & 0x1) {
            drawVLine(x + xh, y - yh, 2 * yh + 1 + delta);
            drawVLine(x + yh, y - xh, 2 * xh + 1 + delta);
        }
        if (cornername & 0x2) {
            drawVLine(x - xh, y - yh, 2 * yh + 1 + delta);
            drawVLine(x - yh, y - xh, 2 * xh + 1 + delta);
        }
    }
}

void Lcd::drawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    /// @brief Draws a triangle outline from three vertices.

    // 1. TRIANGLE OUTLINE DRAW
    // Draw three edges between triangle vertices.
    drawLine(x0, y0, x1, y1);
    drawLine(x1, y1, x2, y2);
    drawLine(x2, y2, x0, y0);
}

void Lcd::fillTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    /// @brief Fills a triangle using scanline rasterization.

    // 1. SCANLINE TRIANGLE FILL
    // Fill triangle using y-sorted scanline rasterization.
    int16_t a, b, y, last;

    // 2. VERTEX SORTING
    // Sort vertices by ascending y coordinate.
    if (y0 > y1) {
        swap(y0, y1);
        swap(x0, x1);
    }
    if (y1 > y2) {
        swap(y2, y1);
        swap(x2, x1);
    }
    if (y0 > y1) {
        swap(y0, y1);
        swap(x0, x1);
    }

    // 3. FLAT TRIANGLE FAST PATH
    // Handle degenerate single-scanline case.
    if (y0 == y2) {
        a = b = x0;
        if (x1 < a)
            a = x1;
        else if (x1 > b)
            b = x1;
        if (x2 < a)
            a = x2;
        else if (x2 > b)
            b = x2;
        drawHLine(a, y0, b - a + 1);
        return;
    }
    int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0, dx12 = x2 - x1, dy12 = y2 - y1;
    int32_t sa = 0, sb = 0;
    (y1 == y2) ? last = y1 : last = y1 - 1;

    // 4. UPPER HALF RASTERIZATION
    // Fill scanlines from top vertex to middle vertex.
    for (y = y0; y <= last; y++) {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        if (a > b) swap(a, b);
        drawHLine(a, y, b - a + 1);
    }

    // 5. LOWER HALF RASTERIZATION
    // Fill scanlines from middle vertex to bottom vertex.
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++) {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        if (a > b) swap(a, b);
        drawHLine(a, y, b - a + 1);
    }
}

void Lcd::drawText(const char text[], uint16_t textLength, uint16_t x, uint16_t y) {
    /// @brief Renders text at the specified position using current font settings.

    // 1. TEXT METRICS CALCULATION
    // Compute text block size and initial position.
    uint16_t wText = textLength * (fontWidth + fontSpacing);
    uint16_t hText = fontHeight;
    uint16_t xPos = x;

    // 2. INPUT VALIDATION
    // Skip rendering for empty text payload.
    if (textLength == 0) return;

    // 3. ALIGNMENT ADJUSTMENT
    // Apply configured text alignment mode.
    switch (fontAlignment) {
        case CENTER:
            xPos = x - (wText / 2);
            break;
        case RIGHT:
            xPos = x - wText;
            break;
        case LEFT:
        default:
            xPos = x;
            break;
    }

    // 4. GLYPH INDEX PRECALCULATION
    // Resolve glyph table index for each character.
    uint8_t letterIndices[textLength];
    for (uint16_t i = 0; i < textLength; i++) {
        letterIndices[i] = getLetter(text[i]);
    }

    // 5. DRAW WINDOW SETUP
    // Configure LCD address window for text rectangle.
    setAddressWindow(xPos, y, xPos + wText - 1, y + hText - 1);

    // 6. BUS POINTER CACHING
    // Cache data and write strobe registers.
    LCD_CS_LOW;
    LCD_RS_DATA;
    volatile uint32_t* dataReg = &(LCD_DATA_GPIO_Port->ODR);
    volatile uint32_t* wrReg = &(LCD_WR_GPIO_Port->BSRR);
    const uint32_t wrSet = LCD_WR_Pin;
    const uint32_t wrReset = LCD_WR_Pin << 16U;

    // 7. COLOR CACHE
    // Cache foreground/background colors for fast access.
    const uint16_t fgColor = foreColor;
    const uint16_t bgColor = backColor;

    // 8. TEXT RASTER LOOP
    // Stream glyph pixels row by row to LCD.
    for (uint16_t row = 0; row < hText; row++) {
        for (uint16_t i = 0; i < textLength; i++) {
            // Retrieve pre-calculated font index.
            uint8_t index = letterIndices[i];

            // Calculate row offset for current glyph.
            uint32_t offset = (index * fontHeight) + row;

            // Fetch bit pattern for glyph row.
            uint16_t lineData = *(fontData + offset);

            // Start with most significant glyph bit.
            uint16_t mask = 1 << (fontWidth - 1);
            for (uint16_t col = 0; col < fontWidth; col++) {
                // Select foreground/background color by mask.
                if (lineData & mask) {
                    *dataReg = fgColor;
                } else {
                    *dataReg = bgColor;
                }

                // Toggle WR strobe.
                *wrReg = wrReset;
                *wrReg = wrSet;

                // Shift mask to next pixel.
                mask >>= 1;
            }

            // 9. CHARACTER SPACING FILL
            // Emit spacing pixels after each glyph.
            if (fontSpacing > 0) {
                *dataReg = bgColor;
                for (uint8_t s = 0; s < fontSpacing; s++) {
                    *wrReg = wrReset;
                    *wrReg = wrSet;
                }
            }
        }
    }
    LCD_CS_HIGH;
}

void Lcd::drawNumber(int32_t num, uint8_t numSize, uint16_t x, uint16_t y) {
    /// @brief Formats a signed number and renders it as fixed-width text.

    // 1. BUFFER INITIALIZATION
    // Prepare fixed-size text buffer for numeric rendering.
    char buffer[16];

    // 2. INPUT VALIDATION
    // Enforce supported digit count range.
    if (numSize == 0 || numSize > 15) return;

    // 3. SIGN NORMALIZATION
    // Convert negative value to absolute and remember sign.
    char* ptr = buffer + numSize;
    *ptr = '\0';
    int32_t tempNum = num;
    bool isNegative = false;

    if (tempNum < 0) {
        isNegative = true;
        tempNum = -tempNum;
    }

    // 4. DIGIT ENCODING
    // Write digits from right to left.
    do {
        if (ptr > buffer) {
            *(--ptr) = (tempNum % 10) + '0';
            tempNum /= 10;
        } else {
            break;
        }
    } while (tempNum != 0);

    // 5. SIGN INSERTION
    // Prefix minus sign if value was negative.
    if (isNegative && ptr > buffer) {
        *(--ptr) = '-';
    }

    // 6. LEFT PADDING
    // Fill remaining cells with spaces.
    while (ptr > buffer) {
        *(--ptr) = ' ';
    }

    // 7. NUMBER RENDER
    // Draw formatted number string.
    drawText(buffer, numSize, x, y);
}

void Lcd::clearHLine(uint16_t x, uint16_t y, uint16_t l) {
    /// @brief Clears a horizontal segment using background color.

    // 1. HORIZONTAL CLEAR
    // Clear horizontal segment with background color.
    if ((x < width) && (y < height)) {
        if ((x + l - 1) >= width) l = width - x;
        setAddressWindow(x, y, x + l - 1, y);
        LCD_RS_HIGH;
        fastFill(backColor, l);
    }
}

void Lcd::clearVLine(uint16_t x, uint16_t y, uint16_t l) {
    /// @brief Clears a vertical segment using background color.

    // 1. VERTICAL CLEAR
    // Clear vertical segment with background color.
    if ((x < width) && (y < height)) {
        if ((y + l - 1) >= height) l = height - y;
        setAddressWindow(x, y, x, y + l - 1);
        LCD_RS_HIGH;
        fastFill(backColor, l);
    }
}

void Lcd::clearRect(uint16_t x, uint16_t y, uint32_t w, uint32_t h) {
    /// @brief Clears a rectangular region using background color.

    // 1. RECTANGLE CLEAR
    // Clear rectangle region with background color.
    if ((x < width) && (y < height)) {
        if ((x + w - 1) >= width) w = width - x;
        if ((y + h - 1) >= height) h = height - y;
        setAddressWindow(x, y, x + w - 1, y + h - 1);
        LCD_RS_HIGH;
        fastFill(backColor, w * h);
    }
}

void Lcd::drawRGB16Image(const RGB16Color* indexPtr, const uint8_t* pixelPtr, uint8_t indexSize, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    /// @brief Draws an indexed RGB565 image.

    // 1. DRAW WINDOW SETUP
    // Configure destination area for indexed image.
    setAddressWindow(x, y, x + w - 1, y + h - 1);
    LCD_CS_LOW;
    LCD_RS_DATA;

    // 2. BUS POINTER CACHING
    // Cache register pointers and strobe masks.
    volatile uint32_t* dataReg = &(LCD_DATA_GPIO_Port->ODR);
    volatile uint32_t* wrReg = &(LCD_WR_GPIO_Port->BSRR);

    const uint32_t wrSet = LCD_WR_Pin;
    const uint32_t wrReset = LCD_WR_Pin << 16U;

    // 3. PIXEL STREAM OUTPUT
    // Expand indexed pixels and write to LCD.
    uint32_t totalPixels = w * h;
    for (uint32_t i = 0; i < totalPixels; i++) {
        *dataReg = indexPtr[pixelPtr[i]];
        *wrReg = wrReset;
        *wrReg = wrSet;
    }

    LCD_CS_HIGH;
}

void Lcd::drawRGB16Image(const RGB16Color* indexPtr, const uint8_t* pixelPtr, uint8_t indexSize, uint8_t alpha, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    /// @brief Draws an indexed RGB565 image with alpha applied to its palette.

    // 1. PALETTE ALPHA PRECOMPUTE
    // Apply alpha to source RGB565 palette.
    RGB16Color indexRGB16[indexSize];
    for (uint8_t i = 0; i < indexSize; i++) {
        indexRGB16[i] = alphaColor(indexPtr[i], alpha);
    }

    // 2. DRAW WINDOW SETUP
    // Configure destination area for image draw.
    setAddressWindow(x, y, x + w - 1, y + h - 1);

    LCD_CS_LOW;
    LCD_RS_DATA;

    volatile uint32_t* dataReg = &(LCD_DATA_GPIO_Port->ODR);
    volatile uint32_t* wrReg = &(LCD_WR_GPIO_Port->BSRR);
    const uint32_t wrSet = LCD_WR_Pin;
    const uint32_t wrReset = LCD_WR_Pin << 16U;

    // 3. PIXEL STREAM OUTPUT
    // Write alpha-adjusted indexed pixels.
    uint32_t totalPixels = w * h;
    for (uint32_t j = 0; j < totalPixels; j++) {
        *dataReg = indexRGB16[pixelPtr[j]];
        *wrReg = wrReset;
        *wrReg = wrSet;
    }

    LCD_CS_HIGH;
}

void Lcd::drawRGB16ImagePartial(const RGB16Color* indexPtr, const uint8_t* pixelPtr, uint8_t indexSize, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t xStart, uint16_t xWidth) {
    /// @brief Draws a horizontal slice of an indexed RGB565 image.

    // 1. SOURCE RANGE CLAMP
    // Clamp requested slice to source image width.
    if ((xStart + xWidth) > w) xWidth = w - xStart;

    // 2. DRAW WINDOW SETUP
    // Configure destination area for partial draw.
    setAddressWindow(x, y, x + xWidth - 1, y + h - 1);

    LCD_CS_LOW;
    LCD_RS_DATA;

    volatile uint32_t* dataReg = &(LCD_DATA_GPIO_Port->ODR);
    volatile uint32_t* wrReg = &(LCD_WR_GPIO_Port->BSRR);
    const uint32_t wrSet = LCD_WR_Pin;
    const uint32_t wrReset = LCD_WR_Pin << 16U;

    // 3. ROW-WISE PIXEL OUTPUT
    // Stream selected source window row by row.
    for (uint32_t i = 0; i < h; i++) {
        const uint8_t* rowStartPtr = &pixelPtr[(i * w) + xStart];
        for (uint32_t j = 0; j < xWidth; j++) {
            *dataReg = indexPtr[rowStartPtr[j]];
            *wrReg = wrReset;
            *wrReg = wrSet;
        }
    }

    LCD_CS_HIGH;
}

void Lcd::drawRGB24Image(const RGB24RawColor* indexPtr, const uint8_t* pixelPtr, uint8_t indexSize, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    /// @brief Draws an indexed RGB24 image after RGB565 conversion.

    // 1. PALETTE COLOR CONVERSION
    // Convert RGB24 palette to RGB565.
    RGB16Color indexRGB16[indexSize];
    for (uint8_t i = 0; i < indexSize; i++) {
        indexRGB16[i] = convertColor(indexPtr[i]);
    }

    // 2. DRAW WINDOW SETUP
    // Configure destination area for image draw.
    setAddressWindow(x, y, x + w - 1, y + h - 1);

    LCD_CS_LOW;
    LCD_RS_DATA;

    volatile uint32_t* dataReg = &(LCD_DATA_GPIO_Port->ODR);
    volatile uint32_t* wrReg = &(LCD_WR_GPIO_Port->BSRR);
    const uint32_t wrSet = LCD_WR_Pin;
    const uint32_t wrReset = LCD_WR_Pin << 16U;

    // 3. PIXEL STREAM OUTPUT
    // Expand indexed pixels and write to LCD.
    uint32_t totalPixels = w * h;
    for (uint32_t j = 0; j < totalPixels; j++) {
        *dataReg = indexRGB16[pixelPtr[j]];
        *wrReg = wrReset;
        *wrReg = wrSet;
    }

    LCD_CS_HIGH;
}

void Lcd::drawRGB24Image(const RGB24RawColor* indexPtr, const uint8_t* pixelPtr, uint8_t indexSize, uint8_t alpha, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    /// @brief Draws an indexed RGB24 image with alpha-adjusted palette conversion.

    // 1. PALETTE ALPHA CONVERSION
    // Convert RGB24 palette to alpha-adjusted RGB565.
    RGB16Color indexRGB16[indexSize];
    for (uint8_t i = 0; i < indexSize; i++) {
        indexRGB16[i] = alphaColor(indexPtr[i], alpha);
    }

    // 2. DRAW WINDOW SETUP
    // Configure destination area for image draw.
    setAddressWindow(x, y, x + w - 1, y + h - 1);

    LCD_CS_LOW;
    LCD_RS_DATA;

    volatile uint32_t* dataReg = &(LCD_DATA_GPIO_Port->ODR);
    volatile uint32_t* wrReg = &(LCD_WR_GPIO_Port->BSRR);
    const uint32_t wrSet = LCD_WR_Pin;
    const uint32_t wrReset = LCD_WR_Pin << 16U;

    // 3. PIXEL STREAM OUTPUT
    // Write alpha-adjusted indexed pixels.
    uint32_t totalPixels = w * h;
    for (uint32_t j = 0; j < totalPixels; j++) {
        *dataReg = indexRGB16[pixelPtr[j]];
        *wrReg = wrReset;
        *wrReg = wrSet;
    }

    LCD_CS_HIGH;
}

void Lcd::fadeRGB16Image(const RGB16Color* indexPtr, const uint8_t* pixelPtr, uint8_t indexSize, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool mode, uint8_t step, uint16_t delay) {
    /// @brief Fades an indexed RGB565 image in or out over multiple steps.

    // 1. WORKSPACE PREPARATION
    // Allocate converted palette and cache bus registers.
    RGB16Color indexRGB16[indexSize];

    volatile uint32_t* dataReg = &(LCD_DATA_GPIO_Port->ODR);
    volatile uint32_t* wrReg = &(LCD_WR_GPIO_Port->BSRR);
    const uint32_t wrSet = LCD_WR_Pin;
    const uint32_t wrReset = LCD_WR_Pin << 16U;

    // 2. FADE STEP LOOP
    // Recompute palette and redraw image for each alpha step.
    uint32_t totalPixels = w * h;

    for (uint8_t i = 1; i <= step; i++) {
        uint8_t currentAlpha;
        if (mode) {
            currentAlpha = (uint8_t)((i * 255) / step);
        } else {
            currentAlpha = (uint8_t)(255 - ((i * 255) / step));
        }

        for (uint8_t j = 0; j < indexSize; j++) {
            indexRGB16[j] = alphaColor(indexPtr[j], currentAlpha);
        }

        setAddressWindow(x, y, x + w - 1, y + h - 1);
        LCD_CS_LOW;
        LCD_RS_DATA;

        for (uint32_t k = 0; k < totalPixels; k++) {
            *dataReg = indexRGB16[pixelPtr[k]];
            *wrReg = wrReset;
            *wrReg = wrSet;
        }
        LCD_CS_HIGH;

        HAL_Delay(delay);
    }
}

void Lcd::fadeRGB24Image(const RGB24RawColor* indexPtr, const uint8_t* pixelPtr, uint8_t indexSize, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool mode, uint8_t step, uint16_t delay) {
    /// @brief Fades an indexed RGB24 image in or out over multiple steps.

    // 1. WORKSPACE PREPARATION
    // Allocate converted palette and cache bus registers.
    RGB16Color indexRGB16[indexSize];

    volatile uint32_t* dataReg = &(LCD_DATA_GPIO_Port->ODR);
    volatile uint32_t* wrReg = &(LCD_WR_GPIO_Port->BSRR);
    const uint32_t wrSet = LCD_WR_Pin;
    const uint32_t wrReset = LCD_WR_Pin << 16U;

    // 2. FADE STEP LOOP
    // Recompute palette and redraw image for each alpha step.
    uint32_t totalPixels = w * h;

    for (uint8_t i = 1; i <= step; i++) {
        uint8_t currentAlpha;
        if (mode) {
            currentAlpha = (uint8_t)((i * 255) / step);
        } else {
            currentAlpha = (uint8_t)(255 - ((i * 255) / step));
        }

        for (uint8_t j = 0; j < indexSize; j++) {
            indexRGB16[j] = alphaColor(indexPtr[j], currentAlpha);
        }

        setAddressWindow(x, y, x + w - 1, y + h - 1);
        LCD_CS_LOW;
        LCD_RS_DATA;

        for (uint32_t k = 0; k < totalPixels; k++) {
            *dataReg = indexRGB16[pixelPtr[k]];
            *wrReg = wrReset;
            *wrReg = wrSet;
        }
        LCD_CS_HIGH;

        HAL_Delay(delay);
    }
}

void Lcd::drawInitSdAlert(SdResult sdResult) {
    /// @brief Draws the startup SD-card alert panel for the given status.

    // 1. ICON COLOR PREPARATION
    // Convert SD icon palette entries to RGB565.
    uint16_t x;
    uint16_t y;
    RGB16Color sdIconIndexRGB16[kSdIconIndexSize];
    RGB16Color sdDetailIconIndexRGB16[kSdDetailIconIndexSize];

    // 2. SD ICON DRAW
    // Draw base SD icon at alert location.
    for (uint8_t i = 0; i < kSdIconIndexSize; i++) {
        uint16_t red = kSdIconIndex[i] >> 3;
        uint16_t green = kSdIconIndex[i] >> 2;
        uint16_t blue = kSdIconIndex[i] >> 3;
        sdIconIndexRGB16[i] = (red << 11) | (green << 5) | (blue);
    }
    x = 441;
    y = 220;
    drawRGB16Image(sdIconIndexRGB16, kSdIconData, kSdIconIndexSize, x, y, kSdIconWidth, kSdIconHeight);

    // 3. DETAIL ICON DRAW
    // Draw status detail icon on top of SD card icon.
    for (uint8_t i = 0; i < kSdDetailIconIndexSize; i++) {
        uint16_t red = kSdDetailIconIndex[i] >> 3;
        uint16_t green = kSdDetailIconIndex[i] >> 2;
        uint16_t blue = kSdDetailIconIndex[i] >> 3;
        sdDetailIconIndexRGB16[i] = (red << 11) | (green << 5) | (blue);
    }
    uint16_t offset;
    (sdResult != SD_ERROR_DETECT) ? offset = 0 : offset = 600;
    x += 27;
    y += 68;
    drawRGB16Image(sdDetailIconIndexRGB16, &kSdDetailIconData[offset], kSdDetailIconIndexSize, x, y, kSdDetailIconWidth, kSdDetailIconHeight);
    setBackColor(BLACK);
    setForeColor(RED);

    // 4. ALERT FRAME DRAW
    // Render red frame around alert message area.
    for (uint8_t i = 0; i < 32; i++) {
        uint16_t red = kCircleIndex[i] >> 3;
        uint16_t green = 0;
        uint16_t blue = 0;
        sdIconIndexRGB16[i] = (red << 11) | (green << 5) | (blue);
    }
    x = 314 + 53;
    y = 382;
    drawRGB16Image(sdIconIndexRGB16, kCircleLeftData, 32, x, y, 13, 25);

    x = 314 + 53 + 213;
    y = 382;
    drawRGB16Image(sdIconIndexRGB16, kCircleRightData, 32, x, y, 13, 25);

    x = 380;
    drawHLine(x, y, 200);
    drawHLine(x, y + 24, 200);
    clearRect(380, 390, 200, 9);

    // 5. ALERT TEXT SELECTION
    // Select message string based on SD result code.
    const char* alertText = "";
    switch (sdResult) {
        case SD_ERROR_DETECT:
            alertText = kSdAlertTextInsert;
            break;
        case SD_ERROR_MOUNT:
            alertText = kSdAlertTextFormat;
            break;
        case SD_ERROR_SERIAL:
            alertText = kSdAlertTextSerial;
            break;
        case SD_ERROR_SYSTEMFOLDER:
            alertText = kSdAlertTextSystemFolder;
            break;
        case SD_ERROR_SAMPLEFOLDER:
            alertText = kSdAlertTextSampleFolder;
            break;
        case SD_ERROR_FILEFOLDER:
            alertText = kSdAlertTextFileFolder;
            break;
        case SD_ERROR_DRUMKITFOLDER:
            alertText = kSdAlertTextDrumkitFolder;
            break;
        case SD_ERROR_SOUNDFOLDER:
            alertText = kSdAlertTextSoundFolder;
            break;
        case SD_ERROR_IMAGEFOLDER:
            alertText = kSdAlertTextImageFolder;
            break;
        case SD_ERROR_FIRMWAREFOLDER:
            alertText = kSdAlertTextFirmwareFolder;
            break;
        case SD_ERROR_PRESETFOLDER:
            alertText = kSdAlertTextPresetFolder;
            break;
        case SD_ERROR_SYSTEMFILE:
            alertText = kSdAlertTextSystemFile;
            break;
        case SD_ANALYZE:
            alertText = kSdAlertTextAnalyze;
            break;
        default:
            break;
    }

    // 6. ALERT TEXT DRAW
    // Draw selected message centered in frame.
    setAlignment(CENTER);
    setFont(FONT_07x09);
    drawText(alertText, strlen(alertText), 480, 390);
}

void Lcd::drawInitSdReadAlert() {
    /// @brief Draws the startup SD-card reading status message.

    // 1. READ STATUS RENDER
    // Draw SD-card reading status text.
    setForeColor(RED);
    setBackColor(BLACK);
    clearRect(380, 390, 200, 9);
    setAlignment(CENTER);
    setFont(FONT_07x09);
    drawText("READING SDCARD", 15, 480, 390);
}

void Lcd::clearInitSdAlert() {
    /// @brief Clears the startup SD-card alert region.

    // 1. ALERT AREA CLEAR
    // Clear SD alert region and restore color.
    setForeColor(BLACK);
    fillRect(360, 190, 240, 220);
    setForeColor(WHITE);
}

void Lcd::invertDisplay(bool invert) {
    /// @brief Enables or disables LCD color inversion mode.

    // 1. INVERSION TOGGLE
    // Enable or disable panel inversion mode.
    if (invert) {
        writeCommand(LCD_ENTER_INVERT_MODE);
    } else {
        writeCommand(LCD_EXIT_INVERT_MODE);
    }
}

void Lcd::displayOn() {
    /// @brief Turns on the LCD panel and backlight.

    // 1. DISPLAY POWER-UP
    // Enable panel output and backlight.
    writeCommand(LCD_SET_DISPLAY_ON);
    clearScreen();
    HAL_Delay(120);
    LCD_BL_HIGH;
}

void Lcd::displayOff() {
    /// @brief Turns off the LCD panel and backlight.

    // 1. DISPLAY POWER-DOWN
    // Disable backlight and panel output.
    LCD_BL_LOW;
    HAL_Delay(120);
    writeCommand(LCD_SET_DISPLAY_OFF);
}