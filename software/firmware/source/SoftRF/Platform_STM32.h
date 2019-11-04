/*
 * Platform_STM32.h
 * Copyright (C) 2019 Linar Yusupov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#if defined(ARDUINO_ARCH_STM32)

#ifndef PLATFORM_STM32_H
#define PLATFORM_STM32_H

#include "IPAddress.h"
#include <Adafruit_NeoPixel.h>

/* Maximum of tracked flying objects is now SoC-specific constant */
#define MAX_TRACKING_OBJECTS    8

#define isValidFix()            isValidGNSSFix()

#define uni_begin()             strip.begin()
#define uni_show()              strip.show()
#define uni_setPixelColor(i, c) strip.setPixelColor(i, c)
#define uni_numPixels()         strip.numPixels()
#define uni_Color(r,g,b)        strip.Color(r,g,b)
#define color_t                 uint32_t

#define yield()                 ({ })
#define snprintf_P              snprintf
#define EEPROM_commit()         {}

#define SSD1306_OLED_I2C_ADDR   0x3C

#define SOC_GPIO_PIN_MODE_PULLDOWN INPUT_PULLDOWN
#define SOC_ADC9_VOLTAGE_DIVIDER   (4096.0 / 3.3)

#define AN3155_BR               115200
#define AN3155_BITS             SERIAL_8E1

// button
#define SOC_GPIO_PIN_BUTTON     USER_BTN

enum rst_reason {
  REASON_DEFAULT_RST      = 0,  /* normal startup by power on */
  REASON_WDT_RST          = 1,  /* hardware watch dog reset */
  REASON_EXCEPTION_RST    = 2,  /* exception reset, GPIO status won't change */
  REASON_SOFT_WDT_RST     = 3,  /* software watch dog reset, GPIO status won't change */
  REASON_SOFT_RESTART     = 4,  /* software restart ,system_restart , GPIO status won't change */
  REASON_DEEP_SLEEP_AWAKE = 5,  /* wake up from deep-sleep */
  REASON_EXT_SYS_RST      = 6   /* external system reset */
};

enum stm32_board_id {
  STM32_BLUE_PILL,
  STM32_TTGO_TWATCH_EB_1_3,
  STM32_TTGO_TWATCH_EB_1_6,
  STM32_TTGO_T_MOTION_1_1
};

struct rst_info {
  uint32_t reason;
  uint32_t exccause;
  uint32_t epc1;
  uint32_t epc2;
  uint32_t epc3;
  uint32_t excvaddr;
  uint32_t depc;
};

/* Primary target hardware (S76G) */
#if defined(ARDUINO_NUCLEO_L073RZ)

#if !defined(USBCON)
#define Serial                  Serial1
#endif
#define swSer                   Serial4
#define UATSerial               Serial2  /* PA3, PA2 */

/* S76G GNSS is operating at 115200 baud (by default) */
#define SERIAL_IN_BR            115200
#define SERIAL_IN_BITS          SERIAL_8N1

/*
 * Make use of AN3155 specs for S76G (valid for SkyWatch only)
 * to simplify SoftRF firmware installation
 * via ESP32 UART bypass code
 */
#define SERIAL_OUT_BR   (hw_info.model == SOFTRF_MODEL_DONGLE ? STD_OUT_BR   : AN3155_BR)
#define SERIAL_OUT_BITS (hw_info.model == SOFTRF_MODEL_DONGLE ? STD_OUT_BITS : AN3155_BITS)

static inline void HAL_GetUID(uint32_t *UID)
{
  UID[0] = (uint32_t)(READ_REG(*((uint32_t *)UID_BASE)));
  UID[1] = (uint32_t)(READ_REG(*((uint32_t *)(UID_BASE +  4U))));
  UID[2] = (uint32_t)(READ_REG(*((uint32_t *)(UID_BASE + 14U))));
}

/* Peripherals */
#define SOC_GPIO_PIN_CONS_RX  PA10
#define SOC_GPIO_PIN_CONS_TX  PA9

#define SOC_GPIO_PIN_SWSER_RX PC11
#define SOC_GPIO_PIN_SWSER_TX PC10

#define SOC_GPIO_PIN_LED      SOC_UNUSED_PIN // PA8

#define SOC_GPIO_PIN_GNSS_RST PB2
#define SOC_GPIO_PIN_GNSS_LS  PC6
#define SOC_GPIO_PIN_GNSS_PPS PB5
#define SOC_GPIO_PIN_STATUS   SOC_UNUSED_PIN // PB0

#define SOC_GPIO_PIN_BUZZER   PA0
#define SOC_GPIO_PIN_BATTERY  PB1

/* SPI0 */
#define SOC_GPIO_PIN_MOSI     PB15
#define SOC_GPIO_PIN_MISO     PB14
#define SOC_GPIO_PIN_SCK      PB13
#define SOC_GPIO_PIN_SS       PB12

/* NRF905 */
#define SOC_GPIO_PIN_TXE      PB11
#define SOC_GPIO_PIN_CE       PB8  // NC
#define SOC_GPIO_PIN_PWR      PB10

/* SX1276 */
#define SOC_GPIO_PIN_RST      PB10
#define SOC_GPIO_PIN_DIO0     PB11
#define SOC_GPIO_PIN_DIO1     PC13
#define SOC_GPIO_PIN_DIO2     PB9
#define SOC_GPIO_PIN_DIO3     PB4
#define SOC_GPIO_PIN_DIO4     PB3
#define SOC_GPIO_PIN_DIO5     PA15

/* RF antenna switch */
#define SOC_GPIO_PIN_ANT_RXTX PA1 // 1:Rx, 0:Tx

/* I2C */
#define SOC_GPIO_PIN_SDA      PB7
#define SOC_GPIO_PIN_SCL      PB6

/* SPI1 */
#define SOC_GPIO_SPI1_MOSI    PA7
#define SOC_GPIO_SPI1_MISO    PA6
#define SOC_GPIO_SPI1_SCK     PA5
#define SOC_GPIO_SPI1_SS      PA4

#define USE_OLED                 //  +3.5 kb
#define USE_NMEA_CFG             //  +2.5 kb
#define EXCLUDE_MPL3115A2        //  -  1 kb

/* SoftRF/S7xG PFLAU NMEA sentence extension(s) */
#define PFLAU_EXT1_FMT  ",%06X,%d,%d,%d"
#define PFLAU_EXT1_ARGS ,ThisAircraft.addr,settings->rf_protocol,rx_packets_counter,tx_packets_counter

/* Secondary target ("Blue pill") */
#elif defined(ARDUINO_BLUEPILL_F103C8)

#define swSer                   Serial2
#define UATSerial               Serial3

static inline void HAL_GetUID(uint32_t *UID)
{
  UID[0] = (uint32_t)(READ_REG(*((uint32_t *)UID_BASE)));
  UID[1] = (uint32_t)(READ_REG(*((uint32_t *)(UID_BASE + 4U))));
  UID[2] = (uint32_t)(READ_REG(*((uint32_t *)(UID_BASE + 8U))));
}

/* Peripherals */
#define SOC_GPIO_PIN_SWSER_RX PA3
#define SOC_GPIO_PIN_SWSER_TX PA2
#define SOC_GPIO_PIN_LED      SOC_UNUSED_PIN // PA8
#define SOC_GPIO_PIN_BUZZER   PB8
#define SOC_GPIO_PIN_BATTERY  PB1

#define SOC_GPIO_PIN_RX3      PB11
#define SOC_GPIO_PIN_TX3      PB10

#define SOC_GPIO_PIN_GNSS_PPS SOC_UNUSED_PIN   // PA1
#define SOC_GPIO_PIN_STATUS   LED_GREEN

/* SPI */
#define SOC_GPIO_PIN_MOSI     PA7
#define SOC_GPIO_PIN_MISO     PA6
#define SOC_GPIO_PIN_SCK      PA5
#define SOC_GPIO_PIN_SS       PA4

/* NRF905 */
#define SOC_GPIO_PIN_TXE      PB4
#define SOC_GPIO_PIN_CE       PB3
#define SOC_GPIO_PIN_PWR      PB5

/* SX1276 (RFM95W) */
#define SOC_GPIO_PIN_RST      PB5
#define SOC_GPIO_PIN_DIO0     PB4

/* RF antenna switch */
#define SOC_GPIO_PIN_ANT_RXTX LMIC_UNUSED_PIN

/* I2C */
#define SOC_GPIO_PIN_SDA      PB7
#define SOC_GPIO_PIN_SCL      PB6

/* Component                         Cost */
/* -------------------------------------- */
/* USB Serial */                 //  + 10 kb
//#define USE_OLED               //  +3.5 kb
#define USE_NMEA_CFG             //  +2.5 kb
//#define EXCLUDE_BMP180         //  -  1 kb
//#define EXCLUDE_BMP280         //  -  2 kb
#define EXCLUDE_MPL3115A2        //  -  1 kb
//#define USE_OGN_RF_DRIVER
//#define WITH_RFM95
//#define WITH_RFM69
#define RFM69_POWER_RATING  1 /* 0 - RFM69xx , 1 - RFM69Hxx */
//#define WITH_SX1272
//#define WITH_SI4X32

#else
#error "This hardware platform is not supported!"
#endif

extern Adafruit_NeoPixel strip;

#endif /* PLATFORM_STM32_H */

#endif /* ARDUINO_ARCH_STM32 */
