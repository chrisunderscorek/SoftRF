/*
 * Platform_RPi.cpp
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

/*
 * Usage example:
 *
 *  pi@raspberrypi $ make -f Makefile.RPi
 *
 *     < ... skipped ... >
 *
 *  pi@raspberrypi $ sudo ./SkyView
 *
 */

#if defined(RASPBERRY_PI)

#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

#include "SoCHelper.h"
#include "NMEAHelper.h"
#include "EPDHelper.h"
#include "TrafficHelper.h"
#include "EEPROMHelper.h"
#include "WiFiHelper.h"
#include "GDL90Helper.h"
#include "BatteryHelper.h"
#include "OLEDHelper.h"

#include "SkyView.h"

#include <alsa/asoundlib.h>
#include <sndfile.h>
#include <string.h>

TTYSerial SerialInput("/dev/ttyUSB0");

static const uint8_t SS    = 8; // pin 24

/* Waveshare Pi HAT 2.7" */
GxEPD2_BW<GxEPD2_270, GxEPD2_270::HEIGHT> epd_waveshare(GxEPD2_270(/*CS=*/ SS,
                                       /*DC=*/ 25, /*RST=*/ 17, /*BUSY=*/ 24));

Adafruit_SSD1306 odisplay(SCREEN_WIDTH, SCREEN_HEIGHT,
  &SPI, /*DC=*/ 25, /*RST=*/ 17, /*CS=*/ SS);

lmic_pinmap lmic_pins = {
    .nss = LMIC_UNUSED_PIN,
    .rxtx = { LMIC_UNUSED_PIN, LMIC_UNUSED_PIN },
    .rst = LMIC_UNUSED_PIN,
    .dio = {LMIC_UNUSED_PIN, LMIC_UNUSED_PIN, LMIC_UNUSED_PIN},
};

void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

void radio_irq_handler (u1_t dio) {
}
u1_t radio_has_irq (void) {
    return 0;
}

eeprom_t eeprom_block;
settings_t *settings = &eeprom_block.field.settings;

char UDPpacketBuffer[UDP_PACKET_BUFSIZE]; // buffer to hold incoming packets

hardware_info_t hw_info = {
  .model    = SOFTRF_MODEL_RASPBERRY,
  .revision = 0,
  .soc      = SOC_NONE,
  .display  = DISPLAY_NONE
};

static sqlite3 *fln_db;
static sqlite3 *ogn_db;
static sqlite3 *paw_db;

static void RPi_setup()
{

  eeprom_block.field.settings.adapter         = ADAPTER_WAVESHARE_PI_HAT_2_7;

  eeprom_block.field.settings.connection      = CON_SERIAL;
  eeprom_block.field.settings.baudrate        = B38400;
  eeprom_block.field.settings.protocol        = PROTOCOL_NMEA;
  eeprom_block.field.settings.orientation     = DIRECTION_NORTH_UP;

  strcpy(eeprom_block.field.settings.ssid,      DEFAULT_AP_SSID);
  strcpy(eeprom_block.field.settings.psk,       DEFAULT_AP_PSK);

  eeprom_block.field.settings.bluetooth       = BLUETOOTH_OFF;

  strcpy(eeprom_block.field.settings.bt_name,   DEFAULT_BT_NAME);
  strcpy(eeprom_block.field.settings.bt_key,    DEFAULT_BT_KEY);

  eeprom_block.field.settings.units           = UNITS_METRIC;
  eeprom_block.field.settings.vmode           = VIEW_MODE_RADAR;
  eeprom_block.field.settings.zoom            = ZOOM_MEDIUM;
  eeprom_block.field.settings.idpref          = ID_REG;
  eeprom_block.field.settings.voice           = VOICE_1;
}

static void RPi_fini()
{

}

static uint32_t RPi_getChipId()
{
  return gethostid();
}

static void RPi_swSer_begin(unsigned long baud)
{
  SerialInput.begin(baud);
}

static void RPi_Battery_setup()
{
  /* TBD */
}

static float RPi_Battery_voltage()
{
  return 0.0;  /* TBD */
}

static void RPi_EPD_setup()
{
  display = &epd_waveshare;
}

static size_t RPi_WiFi_Receive_UDP(uint8_t *buf, size_t max_size)
{
  return 0; /* TBD */
}

static bool RPi_DB_init()
{
  sqlite3_open("Aircrafts/fln.db", &fln_db);

  if (fln_db == NULL)
  {
    printf("Failed to open FlarmNet DB\n");
    return false;
  }

  sqlite3_open("Aircrafts/ogn.db", &ogn_db);

  if (ogn_db == NULL)
  {
    printf("Failed to open OGN DB\n");
    sqlite3_close(fln_db);
    return false;
  }

  sqlite3_open("Aircrafts/paw.db", &paw_db);

  if (paw_db == NULL)
  {
    printf("Failed to open PilotAware DB\n");
    sqlite3_close(fln_db);
    sqlite3_close(ogn_db);
    return false;
  }

  return true;
}

static bool RPi_DB_query(uint8_t type, uint32_t id, char *buf, size_t size)
{
  sqlite3_stmt *stmt;
  char *query = NULL;
  int error;
  bool rval = false;
  const char *reg_key, *db_key;
  sqlite3 *db;

  switch(type)
  {
  case DB_OGN:
    reg_key = "acreg";
    db_key  = "devices";
    db      = ogn_db;
    break;
  case DB_PAW:
    reg_key = "registration";
    db_key  = "aircrafts";
    db      = paw_db;
    break;
  case DB_FLN:
  default:
    reg_key = "registration";
    db_key  = "aircrafts";
    db      = fln_db;
    break;
  }

  if (db == NULL) {
    return false;
  }

  error = asprintf(&query, "select %s from %s where id = %d",reg_key, db_key, id);

  if (error == -1) {
    return false;
  }

  sqlite3_prepare_v2(db, query, strlen(query), &stmt, NULL);

  while (sqlite3_step(stmt) != SQLITE_DONE) {
    if (sqlite3_column_type(stmt, 0) == SQLITE3_TEXT) {

      size_t len = strlen((char *) sqlite3_column_text(stmt, 0));

      if (len > 0) {
        len = len > size ? size : len;
        strncpy(buf, (char *) sqlite3_column_text(stmt, 0), len);
        rval = true;
      }
    }
  }

  sqlite3_finalize(stmt);
  free(query);

  return rval;
}

static void RPi_DB_fini()
{
  if (fln_db != NULL) {
    sqlite3_close(fln_db);
  }

  if (ogn_db != NULL) {
    sqlite3_close(ogn_db);
  }

  if (paw_db != NULL) {
    sqlite3_close(paw_db);
  }
}

static void play_file(snd_pcm_t *pcm_handle, char *filename, short int* buf, snd_pcm_uframes_t frames)
{
    int pcmrc;
    int readcount;

    SF_INFO sfinfo;
    SNDFILE *infile = NULL;

    infile = sf_open(filename, SFM_READ, &sfinfo);

    while ((readcount = sf_readf_short(infile, buf, frames))>0) {

        pcmrc = snd_pcm_writei(pcm_handle, buf, readcount);
        if (pcmrc == -EPIPE) {
            fprintf(stderr, "Underrun!\n");
            snd_pcm_prepare(pcm_handle);
        }
        else if (pcmrc < 0) {
            fprintf(stderr, "Error writing to PCM device: %s\n", snd_strerror(pcmrc));
        }
        else if (pcmrc != readcount) {
            fprintf(stderr,"PCM write difffers from file read.\n");
        }
    }

    sf_close(infile);
}

static void RPi_TTS(char *message)
{
  snd_pcm_t *pcm_handle;
  snd_pcm_hw_params_t *params;
  snd_pcm_uframes_t frames;
  short int* buf = NULL;
  int dir;
  int channels = 1;

  char filename[MAX_FILENAME_LEN];

  if (settings->voice != VOICE_OFF) {

    /* Open the PCM device in playback mode */
    snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);

    /* Allocate parameters object and fill it with default values*/
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    /* Set parameters */
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_handle, params, 1);
    snd_pcm_hw_params_set_rate(pcm_handle, params, 22050, 0);

    /* Write parameters */
    snd_pcm_hw_params(pcm_handle, params);

    /* Allocate buffer to hold single period */
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);

    buf = (short int *) malloc(frames * channels * sizeof(short int));

    char *word = strtok (message, " ");

    while (word != NULL)
    {
        strcpy(filename, WAV_FILE_PREFIX);
        strcat(filename,  settings->voice == VOICE_1 ? VOICE1_SUBDIR :
                         (settings->voice == VOICE_2 ? VOICE2_SUBDIR :
                         (settings->voice == VOICE_3 ? VOICE3_SUBDIR :
                          "" )));
        strcat(filename, word);
        strcat(filename, WAV_FILE_SUFFIX);
        play_file(pcm_handle, filename, buf, frames);
        word = strtok (NULL, " ");
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    free(buf);
  }
}

static void RPi_Button_setup()
{

}

static void RPi_Button_loop()
{

}

static void RPi_Button_fini()
{

}

const SoC_ops_t RPi_ops = {
  SOC_RPi,
  "RPi",
  RPi_setup,
  RPi_fini,
  RPi_getChipId,
  NULL,
  NULL,
  NULL,
  RPi_swSer_begin,
  NULL,
  NULL,
  NULL,
  RPi_Battery_setup,
  RPi_Battery_voltage,
  RPi_EPD_setup,
  RPi_WiFi_Receive_UDP,
  RPi_DB_init,
  RPi_DB_query,
  RPi_DB_fini,
  RPi_TTS,
  RPi_Button_setup,
  RPi_Button_loop,
  RPi_Button_fini
};

int main()
{
  // Init GPIO bcm
  if (!bcm2835_init()) {
      fprintf( stderr, "bcm2835_init() Failed\n\n" );
      exit(EXIT_FAILURE);
  }

  Serial.begin(38400);

  hw_info.soc = SoC_setup(); // Has to be very first procedure in the execution order

  Battery_setup();
  SoC->Button_setup();

  switch (settings->adapter)
  {
  case ADAPTER_WAVESHARE_PI_HAT_2_7:
    Serial.print(F("Intializing E-ink display module (may take up to 10 seconds)... "));
    Serial.flush();

    hw_info.display = EPD_setup();
    if (hw_info.display != DISPLAY_NONE) {
      Serial.println(F(" done."));
    } else {
      Serial.println(F(" failed!"));
    }
    break;
  case ADAPTER_OLED:
    /* 2.42" SPI OLED does not have MISO wire to probe the display */
    OLED_setup();
    hw_info.display = DISPLAY_OLED_2_4;
    break;
  default:
    break;
  }

  switch (settings->protocol)
  {
  case PROTOCOL_GDL90:
    GDL90_setup();
    break;
  case PROTOCOL_NMEA:
  default:
    NMEA_setup();
    break;
  }

  if (!SoC->DB_init()) {
      fprintf( stderr, "Unable to open aircrafts database(s)\n\n" );
      exit(EXIT_FAILURE);
  }

#if 0
  char registration[9];
  long start = micros();
  if (SoC->DB_query(DB_FLN, 14619344, registration, sizeof(registration) - 1)) {
    Serial.print(F("(FLN) registration of "));
    Serial.print(14619344);
    Serial.print(F(" is "));
    Serial.println(registration);
  }
   Serial.print(F("Time taken:"));
   Serial.println(micros()-start);

  start = micros();
  if (SoC->DB_query(DB_OGN, 14619344, registration, sizeof(registration) - 1)) {
    Serial.print(F("(OGN) registration of "));
    Serial.print(14619344);
    Serial.print(F(" is "));
    Serial.println(registration);
  }
   Serial.print(F("Time taken:"));
   Serial.println(micros()-start);

  start = micros();
  if (SoC->DB_query(DB_PAW, 14619344, registration, sizeof(registration) - 1)) {
    Serial.print(F("(PAW) registration of "));
    Serial.print(14619344);
    Serial.print(F(" is "));
    Serial.println(registration);
  }
   Serial.print(F("Time taken:"));
   Serial.println(micros()-start);
#endif

#if 0
  char sentence[] = "traffic 3oclock 7 kms distance 5 hundred feet high";
  SoC->TTS(sentence);
#endif

  while (true) {

    switch (settings->protocol)
    {
    case PROTOCOL_GDL90:
      GDL90_loop();
      break;
    case PROTOCOL_NMEA:
    default:
      NMEA_loop();
      break;
    }

    switch (hw_info.display)
    {
    case DISPLAY_EPD_2_7:
      EPD_loop();
      break;
    case DISPLAY_OLED_2_4:
      OLED_loop();
      break;
    default:
      break;
    }

    Traffic_ClearExpired();
  }

  return 0;
}

#endif /* RASPBERRY_PI */
