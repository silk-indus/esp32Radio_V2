//***************************************************************************************************
//*  ESP32_Radio V2 -- Webradio receiver for ESP32, VS1053 MP3 module and optional display.         *
//*                    By Ed Smallenburg.                                                           *
//***************************************************************************************************
// ESP32 libraries used:  See platformio.ini
// A library for the VS1053 (for ESP32) is not available (or not easy to find).  Therefore
// a class for this module is derived from the maniacbug library and integrated in this sketch.
// The Helix codecs for MP3 and AAC are taken from Wolle (schreibfaul1), see:
// https://github.com/schreibfaul1/ESP32-audioI2S
//
// See http://www.internet-radio.com for suitable stations.  Add the stations of your choice
// to the preferences through the webinterface.
// You may also use the "search" page of the webinterface to find stations.
//
// Brief description of the program:
// First a suitable WiFi network is found and a connection is made.
// Then a connection will be made to a shoutcast server.  The server starts with some
// info in the header in readable ascii, ending with a double CRLF, like:
//  icy-name:Classic Rock Florida - SHE Radio
//  icy-genre:Classic Rock 60s 70s 80s Oldies Miami South Florida
//  icy-url:http://www.ClassicRockFLorida.com
//  content-type:audio/mpeg
//  icy-pub:1
//  icy-metaint:32768          - Metadata after 32768 bytes of MP3-data
//  icy-br:128                 - in kb/sec (for Ogg this is like "icy-br=Quality 2"
//
// After de double CRLF is received, the server starts sending mp3- or Ogg-data.  For mp3, this
// data may contain metadata (non mp3) after every "metaint" mp3 bytes.
// The metadata is empty in most cases, but if any is available the content will be
// presented on the TFT.
// Pushing an input button causes the player to execute a programmable command.
//
// The display used is a Chinese 1.8 color TFT module 128 x 160 pixels.
// Now there is room for 26 characters per line and 16 lines.
// Software will work without installing the display.
// Other displays are also supported. See documentation.
// For configuration of the WiFi network(s): see the global data section further on.
//
// The VSPI interface is used for VS1053, TFT and SD.
//
// Wiring. Note that this is just an example.  Pins (except 18, 19 and 23 of the SPI interface)
// can be configured in the config page of the web interface.
//
// ESP32dev Signal  Wired to LCD        Wired to VS1053      AI Audio board    Wired to the rest
// -------- ------  --------------      -------------------  ---------------   ------------------------
// GPIO32           -                   pin 1 XDCS           I2C Clock
// GPIO33           -                   -                    I2C Data
// GPIO5            -                   pin 2 XCS            KEY 6             -
// GPIO4            -                   pin 4 DREQ           AMPLIFIER_ENABLE  -
// GPIO2            pin 3 D/C or A0     -                    SPI_MISO          -
// GPIO16   RXD2    -                   -                                      TX of NEXTION (if in use)
// GPIO17   TXD2    -                   -                                      RX of NEXTION (if in use)
// GPIO18   SCK     pin 5 CLK or SCK    pin 5 SCK            KEY 5             -
// GPIO19   MISO    -                   pin 7 MISO           KEY 3             -
// GPIO23   MOSI    pin 4 DIN or SDA    pin 6 MOSI           KEY 4             -
// GPIO15           pin 2 CS            -                    SPI_MOSI          -
// GPIO3    RXD0    -                   -                                      Reserved serial input
// GPIO1    TXD0    -                   -                                      Reserved serial output
// GPIO34   -       -                   -                    SD detect         Optional pull-up resistor
// GPIO35   -       -                   -                                      Infrared receiver VS1838B
// GPIO25   -       -                   -                    I2S DSIN          Rotary encoder CLK
// GPIO26   -       -                   -                    I2S LRC           Rotary encoder DT
// GPIO27   -       -                   -                    I2S BCLK          Rotary encoder SW
// GPIO13   -       -                   -                    SD card CS        -
// GPIO14   -       -                   -                    SPI_SCK           -
// GPIO36   -       -                   -                    KEY 1             -
// GPIO13   -       -                   -                    KEY 2             -
// GPIO19   -       -                   -                    KEY 3             -
// GPIO23   -       -                   -                    KEY 4             -
// GPIO18   -       -                   -                    KEY 5             -
// GPIO05   -       -                   -                    KEY 6             -
// -------  ------  ---------------     -------------------                    ----------------
// GND      -       pin 8 GND           pin 8 GND                              Power supply GND
// VCC 5 V  -       pin 7 BL            -                                      Power supply
// VCC 5 V  -       pin 6 VCC           pin 9 5V                               Power supply
// EN       -       pin 1 RST           pin 3 XRST                             -
//
//  History:
//   Date     Author        Remarks
// ----------  --  ------------------------------------------------------------------
// 06-08-2021, ES: Copy from version 1.
// 06-08-2021, ES: Use SPIFFS and Async webserver.
// 23-08-2021, ES: Version with software MP3/AAC decoders.
// 05-10-2021, ES: Fixed internal DAC output, fixed OTA update.
// 06-10-2021, ES: Fixed AP mode.
// 10-02-2022, ES: Included ST7789 display.
// 11-02-2022, ES: SD card implementation.
// 26-03-2022, ES: Fixed NEXTION bug.
// 12-04-2022, ES: Fixed dataqueue bug (NEXT function).
// 13-04-2022, ES: Fixed redirect bug (preset was reset), fixed playlist.
// 14-04-2022, ES: Added posibility for a fixed WiFi network.
// 15-04-2022, ES: Redesigned station selection.
// 25-04-2022, ES: Support for WT32-ETH01 (wired Ethernet).
// 13-05-2022, ES: Correction I2S settings.
// 15-05-2022, ES: Correction mp3 list for web interface.
// 17-11-2022, ES: Support of AI Audio kit V2.1.
// 22-11-2022, ES: Fixed memory leak.
// 28-04-2023, ES: Correct "Request station:port failed!"
// 10-05-2023, ES: SD card files stored on the SDcard.
// 19-05-2023, ES: Mute and unmute with 2 buttons or commands.
// 22-05-2023, ES: Use internal mutex for SPI bus.
// 16-06-2023, ES: Add sleep commmeand.
// 09-10-2023, ES: Reduce GPIO errors by checking GPIO pins.
// 14-12-2023, ES: Add mqtt trigger to refresh all items.
// 16-01-2024, ES: Disable brownout.
// 16-02-2024, ES: SPDIFF output (experimental).
// 19-02-2024, ES: Fixed mono stream, correct handling of reset command.
// 27-06-2024, ES: Simplified WiFi network set.
// 17-10-2024, ES: Support for ESP32-S3.
// 19-03-2025, ES: Correction for new Espressif SDK
// 29-04-2025, ES: Allow special treat of "mute"-command, Pako2 wish.
//                 Correction tracks bug on SD card, Pako2 suggestion.
// 29-06-2025, ES: Changed I2S_COMM_FORMAT_STAND_I2MSB into I2S_COMM_FORMAT_STAND_MSB.

//
// Define the version number, the format used is the HTTP standard.
#define VERSION     "Mon, 14 Sep 2026 20:30:00 GMT"
//
#include <Arduino.h>                                      // Standard include for Platformio Arduino projects
#include "soc/soc.h"                                      // For brown-out detector setting
#include "soc/rtc_cntl_reg.h"                             // For brown-out detector settingtest
//#include <esp_log.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>                            // TLS client for HTTPS radio streams
#include <esp_wifi.h>                                    // WiFi power-save and disconnect diagnostics
#include <vector>
#include "config.h"                                       // Specify display type, decoder type
#include <nvs.h>                                          // Access to NVS
#include <PubSubClient.h>                                 // MTTQ access
#ifdef ETHERNET
  #include <ETH.h>                                        // Definitions for Ethernet controller
  //#include <AsyncWebServer_WT32_ETH01.h>
  #define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN              // External clock from crystal oscillator
  #define ETH_TYPE        ETH_PHY_LAN8720                 // Type of controller
  #define ETH_ADDR        1                               // I2C address of Ethernet PHY
#endif
#include <ESPAsyncWebServer.h>                            // For Async Web server
#include <ESPmDNS.h>                                      // For multicast DNS
#include <time.h>                                         // Time functions
#include <SPI.h>                                          // For SPI handling
#ifdef ENABLEOTA
  #include <ArduinoOTA.h>                                 // Over the air updates
#endif
#include <freertos/queue.h>                               // FreeRtos queue support
#include <freertos/task.h>                                // FreeRtos task handling
#include <esp_task_wdt.h>
#include <driver/adc.h>
#include <base64.h>                                       // For Basic authentication
#include <SPIFFS.h>                                       // Filesystem
#include "utils.h"                                        // Some handy utilities
#if defined(DEC_HELIX_SPDIF) || defined(DEC_HELIX_INT) || defined(DEC_HELIX_AI)
  #define DEC_HELIX
#endif
#if defined(DEC_HELIX_AI)                                 // AI Audio kit board?
  #include <AC101.h>
  #define GPIO_PA_EN 21                                   // GPIO for enabling amplifier
  AC101 dac ;                                             // AC101 controls
#endif
#if defined(DEC_HELIX)
  #include <driver/i2s.h>                                 // Driver for I2S output
  #include "mp3_decoder.h"                                // Yes, include libhelix_HMP3DECODER
  #include "aac_decoder.h"                                // and libhelix_HAACDECODER
  #include "helixfuncs.h"                                 // Helix functions
#else
  #include "VS1053.h"                                     // Driver for VS1053
#endif
#define MAXKEYS           200                             // Max. number of NVS keys in table
#define FSIF              true                            // Format SPIFFS if not existing
#define QSIZ              400                             // Number of entries in the MP3 stream queue
#define NVSBUFSIZE        150                             // Max size of a string in NVS
// Access point name if connection to WiFi network fails.  Also the hostname for WiFi and OTA.
// Note that the password of an AP must be at least as long as 8 characters.
// Also used for other naming.
#ifndef NAME                                              // Name may be defined in config.h
  #define NAME            "ESP32-Radio"
#endif
#define MAXPRESETS        200                             // Max number of presets in preferences
#define MAXMQTTCONNECTS   5                               // Maximum number of MQTT reconnects before give-up
#define METASIZ           1024                            // Size of metaline buffer
#define BL_TIME           45                              // Time-out [sec] for blanking TFT display (BL pin)
//
// Subscription topics for MQTT.  The topic will be pefixed by "PREFIX/", where PREFIX is replaced
// by the the mqttprefix in the preferences.  The next definition will yield the topic
// "ESP32Radio/command" if mqttprefix is "ESP32Radio".
#define MQTT_SUBTOPIC     "command"                      // Command to receive from MQTT
//
#define heapspace heap_caps_get_largest_free_block ( MALLOC_CAP_8BIT )

//**************************************************************************************************
// Forward declaration and prototypes of various functions.                                        *
//**************************************************************************************************
void        tftlog ( const char *str, bool newline = false ) ;
bool        showstreamtitle ( const char* ml, bool full = false ) ;
bool        isAdvertisementMetadata ( const char* metadata ) ;
void        handlebyte_ch ( uint8_t b ) ;
void        handleCmd()  ;
const char* analyzeCmd ( const char* str ) ;
const char* analyzeCmd ( const char* par, const char* val ) ;
void        chomp ( String &str ) ;
String      nvsgetstr ( const char* key ) ;
bool        nvssearch ( const char* key ) ;
void        sdfuncs() ;
void        stop_mp3client () ;
void        pump_secure_stream () ;
void        tftset ( uint16_t inx, const char *str ) ;
void        tftset ( uint16_t inx, String& str ) ;
void        playtask ( void* parameter ) ;                 // Task to play the stream on VS1053 or HELIX decoder
void        displayinfo ( uint16_t inx ) ;
void        gettime() ;
void        reservepin ( int8_t rpinnr ) ;
uint32_t    ssconv ( const uint8_t* bytes ) ;
void        scan_content_length ( const char* metalinebf ) ;
String      decode_spec_chars ( String str ) ;
void        handle_notfound  ( AsyncWebServerRequest *request ) ;
void        handle_getprefs  ( AsyncWebServerRequest *request ) ;
void        handle_saveprefs ( AsyncWebServerRequest *request ) ;
void        handle_getdefs   ( AsyncWebServerRequest *request ) ;
void        handle_settings  ( AsyncWebServerRequest *request ) ;
void        handle_mp3list   ( AsyncWebServerRequest *request ) ;
void        handle_reset     ( AsyncWebServerRequest *request ) ;
bool        readhostfrompref ( int16_t preset, String* host, String* hsym = NULL ) ;
void        enterStationDigit ( uint8_t digit ) ;
void        confirmStationNumber() ;
void        openStationList() ;
void        openSDList() ;
void        moveStationList ( int8_t direction, uint8_t count ) ;
void        confirmStationList() ;



//**************************************************************************************************
// Several structs and enums.                                                                      *
//**************************************************************************************************
//

enum qdata_type { QDATA, QSTARTSONG, QSTOPSONG,       // datatyp in qdata_struct,
                  QSTOPTASK } ;
struct qdata_struct                                   // Data in queue for playtask (dataqueue)
{
  qdata_type                          datatyp ;       // Identifier
  __attribute__((aligned(4))) uint8_t buf[32] ;       // Buffer for chunk of mp3 data
} ;

struct ini_struct
{
  String         mqttbroker ;                         // The name of the MQTT broker server
  String         mqttprefix ;                         // Prefix to use for topics
  uint16_t       mqttport ;                           // Port, default 1883
  String         mqttuser ;                           // User for MQTT authentication
  String         mqttpasswd ;                         // Password for MQTT authentication
  uint8_t        reqvol ;                             // Requested volume
  uint8_t        rtone[4] ;                           // Requested bass/treble settings
  String         clk_server ;                         // Server to be used for time of day clock
  int8_t         clk_offset ;                         // Offset in hours with respect to UTC
  int8_t         clk_dst ;                            // Number of hours shift during DST
  int8_t         ir_pin ;                             // GPIO connected to output of IR decoder
  int8_t         enc_clk_pin ;                        // GPIO connected to CLK of rotary encoder
  int8_t         enc_dt_pin ;                         // GPIO connected to DT of rotary encoder
  int8_t         enc_sw_pin ;                         // GPIO connected to SW of rotary encoder / ZIPPY B5
  int8_t         enc_up_pin ;                         // GPIO connected to UP of ZIPPY B5 side switch
  int8_t         enc_dwn_pin ;                        // GPIO connected to DOWN of ZIPPY B5 side switch
  int8_t         tft_cs_pin ;                         // GPIO connected to CS of TFT screen
  int8_t         tft_dc_pin ;                         // GPIO connected to D/C or A0 of TFT screen
  int8_t         tft_scl_pin ;                        // GPIO connected to SCL of i2c TFT screen
  int8_t         tft_sda_pin ;                        // GPIO connected to SDA of I2C TFT screen
  int8_t         tft_bl_pin ;                         // GPIO to activate BL of display
  int8_t         tft_blx_pin ;                        // GPIO to activate BL of display (inversed logic)
  int8_t         nxt_rx_pin ;                         // GPIO for input from NEXTION
  int8_t         nxt_tx_pin ;                         // GPIO for output to NEXTION
  int8_t         sd_cs_pin ;                          // GPIO connected to CS of SD card
  int8_t         sd_detect_pin ;                      // GPIO connected to SC card detect (LOW is inserted)
  int8_t         vs_cs_pin ;                          // GPIO connected to CS of VS1053
  int8_t         vs_dcs_pin ;                         // GPIO connected to DCS of VS1053
  int8_t         vs_dreq_pin ;                        // GPIO connected to DREQ of VS1053
  int8_t         shutdown_pin ;                       // GPIO to shut down the amplifier
  int8_t         shutdownx_pin ;                      // GPIO to shut down the amplifier (inversed logic)
  int8_t         spi_sck_pin ;                        // GPIO connected to SPI SCK pin
  int8_t         spi_miso_pin ;                       // GPIO connected to SPI MISO pin
  int8_t         spi_mosi_pin ;                       // GPIO connected to SPI MOSI pin
  int8_t         i2s_bck_pin ;                        // GPIO Pin number for I2S "BCK"
  int8_t         i2s_lck_pin ;                        // GPIO Pin number for I2S "L(R)CK"
  int8_t         i2s_din_pin ;                        // GPIO Pin number for I2S "DIN"
  int8_t         i2s_spdif_pin ;                      // GPIO Pin number for SPDIF output
  int8_t         eth_mdc_pin ;                        // GPIO Pin number for Ethernet controller MDC
  int8_t         eth_mdio_pin ;                       // GPIO Pin number for Ethernet controller MDIO
  int8_t         eth_power_pin ;                      // GPIO Pin number for Ethernet controller POWER
  uint16_t       bat0 ;                               // ADC value for 0 percent battery charge
  uint16_t       bat100 ;                             // ADC value for 100 percent battery charge
} ;

struct WifiInfo_t                                     // For list with WiFi info
{
  String ssid ;                                       // SSID for an entry
  String passphrase ;                                 // Passphrase for an entry
} ;

// Preset info
enum station_state_t { ST_PRESET, ST_REDIRECT,        // Possible preset status
                       ST_PLAYLIST, ST_STATION } ;
struct preset_info_t
{
    int16_t            preset ;                       // Preset to play
    int16_t            highest_preset ;               // Highest possible preset
    station_state_t    station_state ;                // Station state
    int16_t            playlistnr ;                   // Index in playlist
    int16_t            highest_playlistnr ;           // Highest possible preset
    String             playlisthost ;                 // Host with playlist
    String             host ;                         // Resulting host
    String             hsym ;                         // Symbolic name (comment after name)
} ;

const char* TAG = "main" ;                            // For debug lines


//**************************************************************************************************
// Global data section.                                                                            *
//**************************************************************************************************
// There is a block ini-data that contains some configuration.  Configuration data is              *
// saved in the preferences by the webinterface.  On restart the new data will                     *
// de read from these preferences.                                                                 *
// Items in ini_block can be changed by commands from webserver/MQTT/Serial.                       *
//**************************************************************************************************

enum datamode_t { INIT = 0x1, HEADER = 0x2, DATA = 0x4,      // State for datastream
                  METADATA = 0x8, PLAYLISTINIT = 0x10,
                  PLAYLISTHEADER = 0x20, PLAYLISTDATA = 0x40,
                  STOPREQD = 0x80, STOPPED = 0x100
                } ;

// Global variables
preset_info_t        presetinfo ;                        // Info about the current or new station
ini_struct           ini_block ;                         // Holds configurable data
AsyncWebServer       cmdserver ( 80 ) ;                  // Instance of embedded webserver, port 80
AsyncClient*         mp3client = NULL ;                  // An instance of the mp3 client
WiFiClientSecure     mp3secureclient ;                    // Synchronous TLS client for HTTPS streams
bool                 secure_stream_active = false ;      // True while HTTPS transport is selected
bool                 stream_redirect_request = false ;   // Next connection follows a Location header
bool                 stream_redirect_playlist = false ;  // Redirect belongs to a playlist response
uint8_t              stream_redirect_count = 0 ;         // Protect against redirect loops
bool                 ad_suppression_active = false ;      // Drop audio until normal metadata returns
WiFiClient           wmqttclient ;                       // An instance for mqtt
PubSubClient         mqttclient ( wmqttclient ) ;        // Client for MQTT subscriber
TaskHandle_t         maintask ;                          // Taskhandle for main task
TaskHandle_t         xplaytask ;                         // Task handle for playtask
TaskHandle_t         xsdtask ;                           // Task handle for SD task
volatile bool        player_init_done = false ;          // Decoder initialization has finished
volatile bool        player_ready = false ;              // Hardware decoder passed communication test
hw_timer_t*          timer = NULL ;                      // For timer
char                 timetxt[9] ;                        // Converted timeinfo
const qdata_struct   stopcmd = {QSTOPSONG} ;             // Command for radio/SD
const qdata_struct   startcmd = {QSTARTSONG} ;           // Command for radio/SD
QueueHandle_t        radioqueue = 0 ;                    // Queue for icecast commands
QueueHandle_t        dataqueue = 0 ;                     // Queue for mp3 datastream
QueueHandle_t        sdqueue = 0 ;                       // For commands to sdfuncs
qdata_struct         outchunk ;                          // Data to queue
qdata_struct         inchunk ;                           // Data from queue
uint8_t*             outqp = outchunk.buf ;              // Pointer to buffer in outchunk
uint32_t             totalcount = 0 ;                    // Counter mp3 data
datamode_t           datamode ;                          // State of datastream
int                  metacount ;                         // Number of bytes in metadata
int                  datacount ;                         // Counter databytes before metadata
RTC_NOINIT_ATTR char metalinebf[METASIZ + 1] ;           // Buffer for metaline/ID3 tags
RTC_NOINIT_ATTR char cmd[130] ;                          // Command from MQTT or Serial
int16_t              metalinebfx ;                       // Index for metalinebf
String               icystreamtitle ;                    // Streamtitle from metadata
String               oldstreamtitle ;                    // Previous displayed stream title
String               icyname ;                           // Icecast station name
String               icyname_raw ;                       // Station name before charset conversion
String               stream_charset ;                    // Charset announced by the stream
String               audio_ct ;                          // Content-type, like "audio/aacp"
String               ipaddress ;                         // Own IP-address
int                  bitrate ;                           // Bitrate in kb/sec
int                  mbitrate ;                          // Measured bitrate
int                  metaint = 0 ;                       // Number of databytes between metadata
bool                 reqtone = false ;                   // New tone setting requested
bool                 muteflag = false ;                  // Mute output
bool                 resetreq = false ;                  // Request to reset the ESP32
bool                 testreq = false ;                   // Request to print test info
bool                 sleepreq = false ;                  // Request for deep sleep
bool                 eth_connected = false ;             // Ethernet connected or not
bool                 NetworkFound = false ;              // True if WiFi network connected
bool                 mqtt_on = false ;                   // MQTT in use
uint16_t             mqttcount = 0 ;                     // Counter MAXMQTTCONNECTS
int8_t               playingstat = 0 ;                   // 1 if radio is playing (for MQTT)
int16_t              playlist_num = 0 ;                  // Nonzero for selection from playlist
bool                 chunked = false ;                   // Station provides chunked transfer
int                  chunkcount = 0 ;                    // Counter for chunked transfer
uint16_t             ir_value = 0 ;                      // IR code
uint32_t             ir_0 = 550 ;                        // Average duration of an IR short pulse
uint32_t             ir_1 = 1650 ;                       // Average duration of an IR long pulse
struct tm            timeinfo ;                          // Will be filled by NTP server
bool                 time_req = false ;                  // Set time requested
uint16_t             adcvalraw ;                         // ADC value (raw)
uint16_t             adcval ;                            // ADC value (battery voltage, averaged)
uint32_t             clength ;                           // Content length found in http header
uint16_t             bltimer = 0 ;                       // Backlight time-out counter
bool                 dsp_ok = false ;                    // Display okay or not
int                  ir_intcount = 0 ;                   // For test IR interrupts
bool                 spftrigger = false ;                // To trigger execution of special functions
bool                 station_number_entry = false ;      // Numeric preset entry is active
String               station_number_input ;              // Digits entered on the remote
uint32_t             station_number_time = 0 ;           // Time of the latest entered digit
bool                 station_list_active = false ;        // Full-screen station list is visible
bool                 station_list_sd = false ;            // Full-screen list currently shows SD tracks
int16_t              station_list_preset = 0 ;            // Preset selected in station list
int16_t              station_list_sd_index = 0 ;          // Track selected in SD list
uint16_t             station_list_scroll = 0 ;            // UTF-8 character offset in station title
uint32_t             station_list_scroll_time = 0 ;       // Time of the latest marquee step
bool                 station_list_scroll_needed = false ; // Selected title exceeds its row width
const char*          fixedwifi = "" ;                    // Used for FIXEDWIFI option
#ifndef ETHERNET
std::vector<WifiInfo_t> wifilist ;                        // Credentials tried directly in list order
volatile bool        wifi_assoc_event = false ;           // Association attempt ended unsuccessfully
volatile uint8_t     wifi_assoc_reason = 0 ;              // ESP-IDF disconnect reason
#endif
File                 SPIFFSfile ;                        /// File handle for SPIFFS file

// nvs stuff
const esp_partition_t*  nvs ;                                     // Pointer to partition struct
esp_err_t               nvserr ;                                  // Error code from nvs functions
uint32_t                nvshandle = 0 ;                           // Handle for nvs access
RTC_NOINIT_ATTR char    nvskeys[MAXKEYS][NVS_KEY_NAME_MAX_SIZE] ; // Space for NVS keys

// Rotary encoder stuff
#define sv DRAM_ATTR static volatile
sv uint16_t       clickcount = 0 ;                       // Incremented per encoder click
sv int16_t        rotationcount = 0 ;                    // Current position of rotary switch
sv uint16_t       enc_inactivity = 0 ;                   // Time inactive
sv bool           singleclick = false ;                  // True if single click detected
sv bool           doubleclick = false ;                  // True if double click detected
sv bool           tripleclick = false ;                  // True if triple click detected
sv bool           longclick = false ;                    // True if longclick detected
enum enc_menu_t { VOLUME, PRESET, TRACK } ;              // State for rotary encoder menu
enc_menu_t        enc_menu_mode = VOLUME ;               // Default is VOLUME mode
//
struct progpin_struct                                    // For programmable input pins
{
  int8_t         gpio ;                                  // Pin number
  bool           reserved ;                              // Reserved for connected devices
  bool           avail ;                                 // Pin is available for a command
  String         command ;                               // Command to execute when activated
                                                         // Example: "uppreset=1"
  bool           cur ;                                   // Current state, true = HIGH, false = LOW
} ;

#ifdef CONFIG_IDF_TARGET_ESP32
  progpin_struct   progpin[] =                             // Input pins and programmed function
  {
    {  0, false, false,  "", false },
    //{  1, true,  false,  "", false },                    // Reserved for TX Serial output
    {  2, false, false,  "", false },
    //{  3, true,  false,  "", false },                    // Reserved for RX Serial input
    {  4, false, false,  "", false },
    {  5, false, false,  "", false },
    //{  6, true,  false,  "", false },                    // Reserved for FLASH SCK
    //{  7, true,  false,  "", false },                    // Reserved for FLASH D0
    //{  8, true,  false,  "", false },                    // Reserved for FLASH D1
    //{  9, true,  false,  "", false },                    // Reserved for FLASH D2
    //{ 10, true,  false,  "", false },                    // Reserved for FLASH D3
    //{ 11, true,  false,  "", false },                    // Reserved for FLASH CMD
    { 12, false, false,  "", false },
    { 13, false, false,  "", false },
    { 14, false, false,  "", false },
    { 15, false, false,  "", false },
    { 16, false, false,  "", false },                      // May be UART 2 RX for Nextion
    { 17, false, false,  "", false },                      // May be UART 2 TX for Nextion
    { 18, false, false,  "", false },                      // Default for SPI CLK
    { 19, false, false,  "", false },                      // Default for SPI MISO
    //{ 20, true,  false,  "", false },                    // Not exposed on DEV board
    { 21, false, false,  "", false },                      // Also Wire SDA
    { 22, false, false,  "", false },                      // Also Wire SCL
    { 23, false, false,  "", false },                      // Default for SPI MOSI
    //{ 24, true,  false,  "", false },                    // Not exposed on DEV board
    { 25, false, false,  "", false },                      // DAC output / I2S output
    { 26, false, false,  "", false },                      // DAC output / I2S output
    { 27, false, false,  "", false },                      // I2S output
    //{ 28, true,  false,  "", false },                    // Not exposed on DEV board
    //{ 29, true,  false,  "", false },                    // Not exposed on DEV board
    //{ 30, true,  false,  "", false },                    // Not exposed on DEV board
    //{ 31, true,  false,  "", false },                    // Not exposed on DEV board
    { 32, false, false,  "", false },
    { 33, false, false,  "", false },
    { 34, false, false,  "", false },                      // Note, no internal pull-up
    { 35, false, false,  "", false },                      // Note, no internal pull-up
    //{ 36, true,  false,  "", false },                    // Reserved for ADC battery level
    { 39, false,  false,  "", false },                     // Note, no internal pull-up
    { -1, false, false,  "", false }                       // End of list
  } ;
#endif
#ifdef CONFIG_IDF_TARGET_ESP32S3
  progpin_struct   progpin[] =                             // Input pins and programmed function
  {
    {  0, false, false,  "", false },                      // Also boot
    //{  1, true,  false,  "", false },                    // Reserved for ADC battery level
    {  2, false, false,  "", false },
    {  3, true,  false,  "", false },
    {  4, false, false,  "", false },
    {  5, false, false,  "", false },
    {  6, true,  false,  "", false },
    {  7, true,  false,  "", false },
    {  8, true,  false,  "", false },
    {  9, true,  false,  "", false },
    { 10, true,  false,  "", false },
    { 11, true,  false,  "", false },
    { 12, false, false,  "", false },
    { 13, false, false,  "", false },
    { 14, false, false,  "", false },
    { 15, false, false,  "", false },
    { 16, false, false,  "", false },                      // May be UART 2 RX for Nextion
    { 17, false, false,  "", false },                      // May be UART 2 TX for Nextion
    { 18, false, false,  "", false },                      // Default for SPI CLK
    //{ 19, false, false,  "", false },                    // Reserved USB D-
    //{ 20, true,  false,  "", false },                    // Reserved USB D+
    { 21, false, false,  "", false },                      // Also Wire SDA
    //{ 35, false, false,  "", false },                    // Reserved PSRAM
    //{ 36, true,  false,  "", false },                    // Reserved PSRAM
    //{ 37, true,  false,  "", false },                    // Reserved PSRAM
    { 38, false,  false,  "", false },
    { 39, false,  false,  "", false },
    { 40, false,  false,  "", false },
    { 41, false,  false,  "", false },
    { 42, false,  false,  "", false },
    //{ 43, false,  false,  "", false },                   // Reserved USB/Serial U0TXD
    //{ 44, false,  false,  "", false },                   // Reserved USB/Serial U0RXD
    //{ 45, false,  false,  "", false },                   // Reserved SPI Flash voltage
    //{ 46, false,  false,  "", false },                   // Reserved Boot mode
    { 47, false,  false,  "", false },
    { 48, false,  false,  "", false },
    { -1, false, false,  "", false }                       // End of list
  } ;
#endif

struct touchpin_struct                                   // For programmable input pins
{
  int8_t         gpio ;                                  // Pin number GPIO
  bool           reserved ;                              // Reserved for connected devices
  bool           avail ;                                 // Pin is available for a command
  String         command ;                               // Command to execute when activated
                                                         // Example: "uppreset=1"
  bool           cur ;                                   // Current state, true = HIGH, false = LOW
  int16_t        count ;                                 // Counter number of times low level
} ;
#ifdef CONFIG_IDF_TARGET_ESP32
touchpin_struct   touchpin[] =                           // Touch pins and programmed function
{
  {   4, false, false, "", false, 0 },                   // TOUCH0
  {   0, true,  false, "", false, 0 },                   // TOUCH1, reserved for BOOT button
  {   2, false, false, "", false, 0 },                   // TOUCH2
  {  15, false, false, "", false, 0 },                   // TOUCH3
  //{  13, false, false, "", false, 0 },                   // TOUCH4, reserved for SPI
  //{  12, false, false, "", false, 0 },                   // TOUCH5, reserved for SPI
  {  14, false, false, "", false, 0 },                   // TOUCH6
  {  27, false, false, "", false, 0 },                   // TOUCH7
  {  33, false, false, "", false, 0 },                   // TOUCH8
  {  32, false, false, "", false, 0 },                   // TOUCH9
  {  -1, false, false, "", false, 0 }                    // End of list
  // End of table
} ;
#endif
#ifdef CONFIG_IDF_TARGET_ESP32S3
touchpin_struct   touchpin[] =                           // Touch pins and programmed function
{
  {   1, true,  false, "", false, 0 },                   // TOUCH1
  {   2, false, false, "", false, 0 },                   // TOUCH2
  {   3, false, false, "", false, 0 },                   // TOUCH3
  {   4, false, false, "", false, 0 },                   // TOUCH4
  {   5, false, false, "", false, 0 },                   // TOUCH5
  {   6, false, false, "", false, 0 },                   // TOUCH6
  {   7, false, false, "", false, 0 },                   // TOUCH7
  {   8, false, false, "", false, 0 },                   // TOUCH8
  {   9, false, false, "", false, 0 },                   // TOUCH9
  {  10, false, false, "", false, 0 },                   // TOUCH10
  {  11, false, false, "", false, 0 },                   // TOUCH11
  {  12, false, false, "", false, 0 },                   // TOUCH12
  {  13, false, false, "", false, 0 },                   // TOUCH13
  {  14, false, false, "", false, 0 },                   // TOUCH14
  {  -1, false, false, "", false, 0 }                    // End of list
  // End of table
} ;
#endif


//**************************************************************************************************
// End of global data section.                                                                     *
//**************************************************************************************************




//**************************************************************************************************
//                                     M Q T T P U B _ C L A S S                                   *
//**************************************************************************************************
// ID's for the items to publish to MQTT.  Is index in amqttpub[]
enum { MQTT_IP,     MQTT_ICYNAME, MQTT_STREAMTITLE, MQTT_NOWPLAYING,
       MQTT_PRESET, MQTT_VOLUME, MQTT_PLAYING, MQTT_PLAYLISTPOS
     } ;
enum { MQSTRING, MQINT8, MQINT16 } ;                     // Type of variable to publish

class mqttpubc                                           // For MQTT publishing
{
    struct mqttpub_struct
    {
      const char*    topic ;                             // Topic as partial string (without prefix)
      uint8_t        type ;                              // Type of payload
      void*          payload ;                           // Payload for this topic
      bool           topictrigger ;                      // Set to true to trigger MQTT publish
    } ;
    // Publication topics for MQTT.  The topic will be pefixed by "PREFIX/", where PREFIX is replaced
    // by the the mqttprefix in the preferences.
  protected:
    mqttpub_struct amqttpub[9] =                         // Definitions of various MQTT topic to publish
    { // Index is equal to enum above
      { "ip",              MQSTRING, &ipaddress,             false }, // Definition for MQTT_IP
      { "icy/name",        MQSTRING, &icyname,               false }, // Definition for MQTT_ICYNAME
      { "icy/streamtitle", MQSTRING, &icystreamtitle,        false }, // Definition for MQTT_STREAMTITLE
      { "nowplaying",      MQSTRING, &ipaddress,             false }, // Definition for MQTT_NOWPLAYING
      { "preset" ,         MQINT8,   &presetinfo.preset,     false }, // Definition for MQTT_PRESET
      { "volume" ,         MQINT8,   &ini_block.reqvol,      false }, // Definition for MQTT_VOLUME
      { "playing",         MQINT8,   &playingstat,           false }, // Definition for MQTT_PLAYING
      { "playlist/pos",    MQINT16,  &presetinfo.playlistnr, false }, // Definition for MQTT_PLAYLISTPOS
      { NULL,              0,        NULL,                   false }  // End of definitions
    } ;
  public:
    void          trigger ( uint8_t item ) ;                      // Trigger publishing for one item
    void          triggerall () ;                                 // Trigger all items
    void          publishtopic() ;                                // Publish triggerer items
} ;


//**************************************************************************************************
// MQTTPUB  class implementation.                                                                  *
//**************************************************************************************************

//**************************************************************************************************
//                                            T R I G G E R                                        *
//**************************************************************************************************
// Set request for an item to publish to MQTT.                                                     *
//**************************************************************************************************
void mqttpubc::trigger ( uint8_t item )                    // Trigger publishig for one item
{
  amqttpub[item].topictrigger = true ;                     // Request re-publish for an item
}

//**************************************************************************************************
//                                       T R I G G E R A L L                                       *
//**************************************************************************************************
// Set request for all items to publish to MQTT.                                                   *
//**************************************************************************************************
void mqttpubc::triggerall()                                // Trigger publishig for one item
{
  int item = 0 ;                                           // Item to refresh

  while ( amqttpub[item].topic )                           // Cycle through the list of items
  {
    trigger ( item++ ) ;                                   // Trigger this item and select next
  }
}

//**************************************************************************************************
//                                     P U B L I S H T O P I C                                     *
//**************************************************************************************************
// Publish a topic to MQTT broker.                                                                 *
//**************************************************************************************************
void mqttpubc::publishtopic()
{
  int         i = 0 ;                                         // Loop control
  char        topic[80] ;                                     // Topic to send
  const char* payload ;                                       // Points to payload
  char        intvar[10] ;                                    // Space for integer parameter

  while ( amqttpub[i].topic )
  {
    if ( amqttpub[i].topictrigger )                           // Topic ready to send?
    {
      amqttpub[i].topictrigger = false ;                      // Success or not: clear trigger
      sprintf ( topic, "%s/%s", ini_block.mqttprefix.c_str(),
                amqttpub[i].topic ) ;                         // Add prefix to topic
      switch ( amqttpub[i].type )                             // Select conversion method
      {
        case MQSTRING :
          payload = ((String*)amqttpub[i].payload)->c_str() ;
          //payload = pstr->c_str() ;                           // Get pointer to payload
          break ;
        case MQINT8 :
          sprintf ( intvar, "%d",
                    *(int8_t*)amqttpub[i].payload ) ;         // Convert to array of char
          payload = intvar ;                                  // Point to this array
          break ;
        case MQINT16 :
          sprintf ( intvar, "%d",
                    *(int16_t*)amqttpub[i].payload ) ;        // Convert to array of char
          payload = intvar ;                                  // Point to this array
          break ;
        default :
          continue ;                                          // Unknown data type
      }
      ESP_LOGI ( TAG, "Publish to topic %s : %s",             // Show for debug
                 topic, payload ) ;
      if ( !mqttclient.publish ( topic, payload ) )           // Publish!
      {
        ESP_LOGE ( TAG, "MQTT publish failed!" ) ;            // Failed
      }
      return ;                                                // Do the rest later
    }
    i++ ;                                                     // Next entry
  }
}

mqttpubc         mqttpub ;                                    // Instance for mqttpubc

//
// Include software for the right display
#ifdef BLUETFT
 #include "bluetft.h"                                        // For ILI9163C or ST7735S 128x160 display
#endif
#ifdef ST7789
 #include "ST7789.h"                                         // For ST7789 240x240 display
#endif
#ifdef ILI9341
 #include "ILI9341.h"                                        // For ILI9341 320x240 display
#endif
#ifdef OLED1306
 #include "oled.h"                                           // For OLED I2C SD1306 64x128 display
#endif
#ifdef OLED1309
 #include "oled.h"                                           // For OLED I2C SD1309 64x128 display
#endif
#ifdef OLED1106
 #include "oled.h"                                           // For OLED I2C SH1106 64x128 display
#endif
#ifdef LCD1602I2C
 #include "LCD1602.h"                                        // For LCD 1602 display (I2C)
#endif
#ifdef LCD2004I2C
 #include "LCD2004.h"                                        // For LCD 2004 display (I2C)
#endif
#ifdef DUMMYTFT
 #include "dummytft.h"                                       // For Dummy display
#endif
#ifdef NEXTION
 #include "NEXTION.h"                                        // For NEXTION display
#endif

// Include software for SD card.  Will include dummy if "SDCARD" is not defined
#include "SDcard.h"                                         // For SD card interface
#if defined(SDCARD) && defined(BLUETFT)
  #include <TJpg_Decoder.h>                                  // Render embedded ID3 JPEG cover art
  static bool toggleSDCover() ;                              // IR 0 toggles embedded cover view
#endif

//**************************************************************************************************
//                                  M Y Q U E U E S E N D                                          *
//**************************************************************************************************
// Send to queue if existing.                                                                      *
//**************************************************************************************************
void myQueueSend ( QueueHandle_t q, const void* msg, int waittime = 0 )
{
  if ( q )                                                // Check if we have a legal queue
  {
    xQueueSend ( q, msg, waittime ) ;                     // Queue okay, send to it
  }
}


//**************************************************************************************************
//                                           B L S E T                                             *
//**************************************************************************************************
// Enable or disable the TFT backlight if configured.                                              *
// May be called from interrupt level.                                                             *
//**************************************************************************************************
void IRAM_ATTR blset ( bool enable )
{
  if ( ini_block.tft_bl_pin >= 0 )                       // Backlight for TFT control?
  {
    digitalWrite ( ini_block.tft_bl_pin, enable ) ;      // Enable/disable backlight
  }
  if ( ini_block.tft_blx_pin >= 0 )                      // Backlight for TFT (inversed logic) control?
  {
    digitalWrite ( ini_block.tft_blx_pin, !enable ) ;    // Enable/disable backlight
  }
  if ( enable )
  {
    bltimer = 0 ;                                        // Reset counter backlight time-out
  }
}


//**************************************************************************************************
//                                      N V S O P E N                                              *
//**************************************************************************************************
// Open Preferences with my-app namespace. Each application module, library, etc.                  *
// has to use namespace name to prevent key name collisions. We will open storage in               *
// RW-mode (second parameter has to be false).                                                     *
//**************************************************************************************************
void nvsopen()
{
  if ( ! nvshandle )                                         // Opened already?
  {
    nvserr = nvs_open ( NAME, NVS_READWRITE, &nvshandle ) ;  // No, open nvs
    if ( nvserr )
    {
      ESP_LOGE ( TAG, "nvs_open failed!" ) ;
    }
  }
}


//**************************************************************************************************
//                                      N V S C L E A R                                            *
//**************************************************************************************************
// Clear all preferences.                                                                          *
//**************************************************************************************************
esp_err_t nvsclear()
{
  nvsopen() ;                                         // Be sure to open nvs
  return nvs_erase_all ( nvshandle ) ;                // Clear all keys
}


//**************************************************************************************************
//                                      N V S G E T S T R                                          *
//**************************************************************************************************
// Read a string from nvs.                                                                         *
//**************************************************************************************************
String nvsgetstr ( const char* key )
{
  static char   nvs_buf[NVSBUFSIZE] ;       // Buffer for contents
  size_t        len = NVSBUFSIZE ;          // Max length of the string, later real length

  nvsopen() ;                               // Be sure to open nvs
  nvs_buf[0] = '\0' ;                       // Return empty string on error
  nvserr = nvs_get_str ( nvshandle, key, nvs_buf, &len ) ;
  if ( nvserr )
  {
    ESP_LOGE ( TAG, "nvs_get_str failed %X for key %s, keylen is %d, len is %d!",
               nvserr, key, strlen ( key), len ) ;
    ESP_LOGE ( TAG, "Contents: %s", nvs_buf ) ;
  }
  return String ( nvs_buf ) ;
}


//**************************************************************************************************
//                                      N V S S E T S T R                                          *
//**************************************************************************************************
// Put a key/value pair in nvs.  Length is limited to allow easy read-back.                        *
// No writing if no change.                                                                        *
//**************************************************************************************************
esp_err_t nvssetstr ( const char* key, String val )
{
  String curcont ;                                         // Current contents
  bool   wflag = true  ;                                   // Assume update or new key

  //ESP_LOGI ( TAG, "Setstring for %s: %s", key, val.c_str() ) ;
  if ( val.length() >= NVSBUFSIZE )                        // Limit length of string to store
  {
    ESP_LOGE ( TAG, "nvssetstr length failed!" ) ;
    return ESP_ERR_NVS_NOT_ENOUGH_SPACE ;
  }
  if ( nvssearch ( key ) )                                 // Already in nvs?
  {
    curcont = nvsgetstr ( key ) ;                          // Read current value
    wflag = ( curcont != val ) ;                           // Value change?
  }
  if ( wflag )                                             // Update or new?
  {
    //ESP_LOGI ( TAG, "nvssetstr update value" ) ;
    nvserr = nvs_set_str ( nvshandle, key, val.c_str() ) ; // Store key and value
    if ( nvserr )                                          // Check error
    {
      ESP_LOGE ( TAG, "nvssetstr failed!" ) ;
    }
  }
  return nvserr ;
}


//**************************************************************************************************
//                                      N V S S E A R C H                                          *
//**************************************************************************************************
// Check if key exists in nvs.                                                                     *
//**************************************************************************************************
bool nvssearch ( const char* key )
{
  size_t        len = NVSBUFSIZE ;                      // Length of the string

  nvsopen() ;                                           // Be sure to open nvs
  nvserr = nvs_get_str ( nvshandle, key, NULL, &len ) ; // Get length of contents
  return ( nvserr == ESP_OK ) ;                         // Return true if found
}


//**************************************************************************************************
//                                      Q U E U E T O P T                                          *
//**************************************************************************************************
// Queue a special function for the play task.                                                     *
// These are high priority messages like stop or start.  So we make sure the message fits.         *
//**************************************************************************************************
void queueToPt ( qdata_type func )
{
  qdata_struct     specchunk ;                            // Special function to queue

  while ( xQueueReceive ( dataqueue, &specchunk, 0 ) ) ;  // Empty the queue
  specchunk.datatyp = func ;                              // Put function in datatyp
  xQueueSendToFront ( dataqueue, &specchunk, 200 ) ;      // Send to queue (First Out)
  vTaskDelay ( 1 ) ;                                      // Give Play task time to react
}


//**************************************************************************************************
//                                      T F T S E T                                                *
//**************************************************************************************************
// Request to display a segment on TFT.  Version for char* and String parameter.                   *
//**************************************************************************************************
void tftset ( uint16_t inx, const char *str )
{
  if ( inx < TFTSECS )                                  // Segment available on display
  {
    if ( str )                                          // String specified?
    {
      tftdata[inx].str = String ( str ) ;               // Yes, set string
    }
    tftdata[inx].update_req = true ;                    // and request flag
  }
}

void tftset ( uint16_t inx, String& str )
{
  if ( inx < TFTSECS )                                  // Segment available on display
  {
    tftdata[inx].str = str ;                            // Set string
    tftdata[inx].update_req = true ;                    // and request flag
  }
}


//**************************************************************************************************
//                                          U P D A T E N R                                        *
//**************************************************************************************************
// Used by nextPreset because update for preset and playlist number is about the same.             *
// Modify the pnr, handle over- and underflow. handle relative setting.                            *
//**************************************************************************************************
bool updateNr ( int16_t* pnr, int16_t maxnr, int16_t nr, bool relative )
{
  bool res = true ;                                           // Assume positive result

  //ESP_LOGI ( TAG, "updateNr %d <= %d to %d, relative is %d",
  //           *pnr, maxnr, nr, relative ) ;
  if ( relative )                                             // Relative to pnr?
  {
    *pnr += nr ;                                              // Yes, compute new pnr
  }
  else
  {
    *pnr = nr ;                                               // Not relative, set direct pnr
  }
  if ( *pnr < 0 )                                             // Check result
  {
    res = false ;                                             // Negative result, set bad result
    *pnr = maxnr ;                                            // and wrap
  }
  if ( *pnr > maxnr )                                         // Check if result beyond max
  {
    res = false ;                                             // Too high, set bad result
    *pnr = 0 ;                                                // and wrap
  }
  //ESP_LOGI ( TAG, "updateNr result is %d", *pnr ) ;
  return res ;                                                // Return the result
}


//**************************************************************************************************
//                                          N E X T P R E S E T                                    *
//**************************************************************************************************
// Set the preset for the next station.  May be relative.                                          *
//**************************************************************************************************
bool nextPreset ( int16_t pnr, bool relative = false )
{
  preset_info_t previous = presetinfo ;
  station_number_entry = false ;
  station_number_input = "" ;
  //ESP_LOGI ( TAG, "nextpreset called with pnr = %d", pnr ) ;
  if ( ( presetinfo.station_state == ST_STATION ) ||           // In station mode?
       ( presetinfo.station_state == ST_REDIRECT ) )           // or redirect mode?
  {
    presetinfo.station_state = ST_PRESET ;                     // No "next" in station/redirect mode
  }
  if ( presetinfo.station_state == ST_PLAYLIST )               // In playlist mode?
  {
    if ( ! updateNr ( &presetinfo.playlistnr,                  // Yes, next index possible?
                      presetinfo.highest_playlistnr,
                      pnr, relative ) )
    {
      presetinfo.station_state = ST_PRESET ;                   // No, end playlist mode
    }
  }
  if ( presetinfo.station_state == ST_PRESET )                 // In preset mode?
  {
    updateNr ( &presetinfo.preset,                             // Select next preset
               presetinfo.highest_preset,
               pnr, relative ) ;
    if ( ! readhostfrompref ( presetinfo.preset,              // Set host
                              &presetinfo.host,
                              &presetinfo.hsym ) )
    {
      presetinfo = previous ;                                // Invalid number: keep current station
      return false ;
    }
    ESP_LOGI ( TAG, "nextPreset is %d", presetinfo.preset ) ;
  }
  tftset ( 2, (const char*)NULL ) ;                           // Show current number in default color
  return true ;
}


//**************************************************************************************************
//                                          T I M E R 1 0 S E C                                    *
//**************************************************************************************************
// Extra watchdog.  Called every 10 seconds.                                                       *
// If totalcount has not been changed, there is a problem and playing will stop.                   *
// Note that calling timely procedures within this routine or in called functions will             *
// cause a crash!                                                                                  *
//**************************************************************************************************
void IRAM_ATTR timer10sec()
{
  static uint32_t oldtotalcount = 7321 ;          // Needed for change detection
  static uint8_t  morethanonce = 0 ;              // Counter for succesive fails
  uint32_t        bytesplayed ;                   // Bytes send to MP3 converter

  if ( datamode & ( INIT | HEADER | DATA |        // Test op playing
                    METADATA | PLAYLISTINIT |
                    PLAYLISTHEADER |
                    PLAYLISTDATA ) )
  {
    bytesplayed = totalcount - oldtotalcount ;    // Number of bytes played in the 10 seconds
    oldtotalcount = totalcount ;                  // Save for comparison in next cycle
    if ( bytesplayed == 0 )                       // Still playing?
    {
      if ( morethanonce > 10 )                    // No! Happened too many times?
      {
        resetreq = true ;                         // Yes, restart
      }
      //if ( datamode & ( PLAYLISTDATA |          // In playlist mode?
      //                  PLAYLISTINIT |
      //                  PLAYLISTHEADER ) )
      //{
      //  playlist_num = 0 ;                      // Yes, end of playlist
      //}
      //if ( ( morethanonce > 0 ) ||              // Happened more than once?
      //     ( playlist_num > 0 ) )               // Or playlist active?
      if ( morethanonce > 0 )                     // Happened more than once?
      {
        datamode = STOPREQD ;                     // Stop player
      }
      morethanonce++ ;                            // Count the fails
    }
    else
    {
      //                                          // Data has been send to MP3 decoder
      // Bitrate in kbits/s is bytesplayed / 10 / 1000 * 8
      mbitrate = ( bytesplayed + 625 ) / 1250 ;   // Measured bitrate, rounded
      morethanonce = 0 ;                          // Data seen, reset failcounter
    }
  }
}


//**************************************************************************************************
//                                          T I M E R 1 0 0                                        *
//**************************************************************************************************
// Called every 100 msec on interrupt level, so must be in IRAM and no lengthy operations          *
// allowed.                                                                                        *
//**************************************************************************************************
void IRAM_ATTR timer100()
{
  sv int16_t   count10sec = 0 ;                   // Counter for activatie 10 seconds process
  sv int16_t   eqcount = 0 ;                      // Counter for equal number of clicks
  sv int16_t   oldclickcount = 0 ;                // To detect difference

  spftrigger = true ;                             // Activate spfuncs
  if ( ++count10sec == 100  )                     // 10 seconds passed?
  {
    timer10sec() ;                                // Yes, do 10 second procedure
    count10sec = 0 ;                              // Reset count
  }
  if ( ( count10sec % 10 ) == 0 )                 // One second over?
  {
    if ( ++timeinfo.tm_sec >= 60 )                // Yes, update number of seconds
    {
      timeinfo.tm_sec = 0 ;                       // Wrap after 60 seconds
      if ( ++timeinfo.tm_min >= 60 )
      {
        timeinfo.tm_min = 0 ;                     // Wrap after 60 minutes
        if ( ++timeinfo.tm_hour >= 24 )
        {
          timeinfo.tm_hour = 0 ;                  // Wrap after 24 hours
        }
      }
    }
    time_req = true ;                             // Yes, show current time request
    if ( ++bltimer == BL_TIME )                   // Time to blank the TFT screen?
    {
      bltimer = 0 ;                               // Yes, reset counter
      blset ( false ) ;                           // Disable TFT (backlight)
    }
  }
  // Handle rotary encoder. Inactivity counter will be reset by encoder interrupt
  if ( enc_inactivity < 36000 )                   // Count inactivity time, but limit to 36000
  {
    enc_inactivity++ ;
  }
  // Now detection of single/double click of rotary encoder switch or ZIPPY B5
  if ( clickcount )                               // Any click?
  {
    if ( oldclickcount == clickcount )            // Yes, stable situation?
    {
      if ( ++eqcount == 6 )                       // Long time stable?
      {
        eqcount = 0 ;
        if ( clickcount > 2 )                     // Triple click?
        {
          tripleclick = true ;                    // Yes, set result
        }
        else if ( clickcount == 2 )               // Double click?
        {
          doubleclick = true ;                    // Yes, set result
        }
        else
        {
          singleclick = true ;                    // Just one click seen
        }
        clickcount = 0 ;                          // Reset number of clicks
      }
    }
    else
    {
      oldclickcount = clickcount ;                // To detect change
      eqcount = 0 ;                               // Not stable, reset count
    }
  }
}


//**************************************************************************************************
//                                          I S R _ I R                                            *
//**************************************************************************************************
// Interrupts received from VS1838B on every change of the signal.                                 *
// Intervals are 640 or 1640 microseconds for data.  syncpulses are 3400 micros or longer.         *
// Input is complete after 65 level changes.                                                       *
// Only the last 32 level changes are significant and will be handed over to common data.          *
//**************************************************************************************************
void IRAM_ATTR isr_IR()
{
  sv uint32_t      t0 = 0 ;                          // To get the interval
  sv uint32_t      ir_locvalue = 0 ;                 // IR code
  sv int           ir_loccount = 0 ;                 // Length of code
  uint32_t         t1, intval ;                      // Current time and interval since last change
  uint32_t         mask_in = 2 ;                     // Mask input for conversion
  uint16_t         mask_out = 1 ;                    // Mask output for conversion

  ir_intcount++ ;                                    // Test IR input.
  t1 = micros() ;                                    // Get current time
  intval = t1 - t0 ;                                 // Compute interval
  t0 = t1 ;                                          // Save for next compare
  if ( ( intval > 300 ) && ( intval < 800 ) )        // Short pulse?
  {
    ir_locvalue = ir_locvalue << 1 ;                 // Shift in a "zero" bit
    ir_loccount++ ;                                  // Count number of received bits
    ir_0 = ( ir_0 * 3 + intval ) / 4 ;               // Compute average durartion of a short pulse
  }
  else if ( ( intval > 1400 ) && ( intval < 1900 ) ) // Long pulse?
  {
    ir_locvalue = ( ir_locvalue << 1 ) + 1 ;         // Shift in a "one" bit
    ir_loccount++ ;                                  // Count number of received bits
    ir_1 = ( ir_1 * 3 + intval ) / 4 ;               // Compute average durartion of a short pulse
  }
  else if ( ir_loccount == 65 )                      // Value is correct after 65 level changes
  {
    while ( mask_in )                                // Convert 32 bits to 16 bits
    {
      if ( ir_locvalue & mask_in )                   // Bit set in pattern?
      {
        ir_value |= mask_out ;                       // Set set bit in result
      }
      mask_in <<= 2 ;                                // Shift input mask 2 positions
      mask_out <<= 1 ;                               // Shift output mask 1 position
    }
    ir_loccount = 0 ;                                // Ready for next input
  }
  else
  {
    ir_locvalue = 0 ;                                // Reset decoding
    ir_loccount = 0 ;
  }
}


//**************************************************************************************************
//                                          I S R _ E N C _ S W I T C H                            *
//**************************************************************************************************
// Interrupts received from rotary encoder switch or ZIPPY B5.                                     *
//**************************************************************************************************
void IRAM_ATTR isr_enc_switch()
{
  sv uint32_t     oldtime = 0 ;                            // Time in millis previous interrupt
  sv bool         sw_state ;                               // True is pushed (LOW)
  bool            newstate ;                               // Current state of input signal
  uint32_t        newtime ;                                // Current timestamp
  uint32_t        dtime ;                                  // Time difference with previous interrupt

  newstate = ( digitalRead ( ini_block.enc_sw_pin ) == LOW ) ;
  newtime = xTaskGetTickCount() ;                          // Time of last interrupt
  dtime = ( newtime - oldtime ) & 0xFFFF ;                 // Compute delta
  if ( dtime < 50 )                                        // Debounce
  {
    return ;                                               // Ignore bouncing
  }
  if ( newstate != sw_state )                              // State changed?
  {
    oldtime = newtime ;                                    // Time of change for next compare
    sw_state = newstate ;                                  // Yes, set current (new) state
    if ( !sw_state )                                       // SW released?
    {
      if ( ( dtime ) > 2000 )                              // More than 2 second?
      {
        longclick = true ;                                 // Yes, register longclick
        clickcount = 0 ;                                   // Forget normal count
      }
      else
      {
        clickcount++ ;                                     // Yes, click detected
      }
      enc_inactivity = 0 ;                                 // Not inactive anymore
    }
  }
}

#ifdef ZIPPYB5
  //**************************************************************************************************
  //                                          I S R _ E N C _ T U R N                                *
  //**************************************************************************************************
  // Interrupts received from ZIPPY B5 side switch (clk signal) knob turn.                           *
  //**************************************************************************************************
  void IRAM_ATTR isr_enc_turn()
  {
    sv uint32_t     trig_time = 0 ;                               // For debounce
    uint32_t        new_time ;                                    // New time

    new_time = xTaskGetTickCount() ;                              // Get time
    if ( new_time <= trig_time )                                  // For debounce
    {
      return ;                                                    // No action
    }
    trig_time = new_time + 300 ;                                  // Dead time 
    if ( digitalRead ( ini_block.enc_up_pin ) == LOW )            // Up pin activated?
    {
      rotationcount++ ;
    }
    else if ( digitalRead ( ini_block.enc_dwn_pin ) == LOW )      // Down pin activated?
    {
      rotationcount-- ;
    }
    enc_inactivity = 0 ;                                          // Mark activity
  }
#else
  //**************************************************************************************************
  //                                          I S R _ E N C _ T U R N                                *
  //**************************************************************************************************
  // Interrupts received from rotary encoder (clk signal) knob turn.                                 *
  // The encoder is a Manchester coded device, the outcomes (-1,0,1) of all the previous state and   *
  // actual state are stored in the enc_states[].                                                    *
  // Full_status is a 4 bit variable, the upper 2 bits are the previous encoder values, the lower    *
  // ones are the actual ones.                                                                       *
  // 4 bits cover all the possible previous and actual states of the 2 PINs, so this variable is     *
  // the index enc_states[].                                                                         *
  // No debouncing is needed, because only the valid states produce values different from 0.         *
  // Rotation is 4 if position is moved from one fixed position to the next, so it is devided by 4.  *
  //**************************************************************************************************
  void IRAM_ATTR isr_enc_turn()
  {
    sv uint32_t     old_state = 0x0001 ;                          // Previous state
    sv int16_t      locrotcount = 0 ;                             // Local rotation count
    uint8_t         act_state = 0 ;                               // The current state of the 2 PINs
    uint8_t         inx ;                                         // Index in enc_state
    sv const int8_t enc_states [] =                               // Table must be in DRAM (iram safe)
    { 0,                    // 00 -> 00
      -1,                   // 00 -> 01                           // dt goes HIGH
      1,                    // 00 -> 10
      0,                    // 00 -> 11
      1,                    // 01 -> 00                           // dt goes LOW
      0,                    // 01 -> 01
      0,                    // 01 -> 10
      -1,                   // 01 -> 11                           // clk goes HIGH
      -1,                   // 10 -> 00                           // clk goes LOW
      0,                    // 10 -> 01
      0,                    // 10 -> 10
      1,                    // 10 -> 11                           // dt goes HIGH
      0,                    // 11 -> 00
      1,                    // 11 -> 01                           // clk goes LOW
      -1,                   // 11 -> 10                           // dt goes HIGH
      0                     // 11 -> 11
    } ;
    // Read current state of CLK, DT pin. Result is a 2 bit binary number: 00, 01, 10 or 11.
    act_state = ( digitalRead ( ini_block.enc_clk_pin ) << 1 ) +
                digitalRead ( ini_block.enc_dt_pin ) ;
    inx = ( old_state << 2 ) + act_state ;                        // Form index in enc_states
    locrotcount += enc_states[inx] ;                              // Get delta: 0, +1 or -1
    if ( locrotcount == 4 )
    {
      rotationcount++ ;                                           // Divide by 4
      locrotcount = 0 ;
    }
    else if ( locrotcount == -4 )
    {
      rotationcount-- ;                                           // Divide by 4
      locrotcount = 0 ;
    }
    old_state = act_state ;                                       // Remember current status
    enc_inactivity = 0 ;                                          // Mark activity
  }
#endif

//**************************************************************************************************
//                         I S  A D V E R T I S E M E N T  M E T A D A T A                        *
//**************************************************************************************************
// Detect explicit ICY advertisement fields.  laut.fm also marks dynamically inserted ads with    *
// StreamUrl='0'; that rule is deliberately limited to laut.fm to avoid false positives elsewhere. *
//**************************************************************************************************
bool isAdvertisementMetadata ( const char* metadata )
{
  if ( metadata == NULL || *metadata == '\0' ) return false ;

  if ( strstr ( metadata, "AdCreativeId=" ) ||
       strstr ( metadata, "adw_ad=" )       ||
       strstr ( metadata, "Advertiser=" )   ||
       strstr ( metadata, "adId=" ) )
  {
    return true ;
  }

  String lowerhost = presetinfo.host ;
  lowerhost.toLowerCase() ;
  const bool lautstream = lowerhost.indexOf ( ".stream.laut.fm/" ) >= 0 ;
  return lautstream &&
         ( strstr ( metadata, "StreamUrl='0'" ) ||
           strstr ( metadata, "StreamUrl=\"0\"" ) ) ;
}


//**************************************************************************************************
//                                S H O W S T R E A M T I T L E                                    *
//**************************************************************************************************
// Show artist and songtitle if present in metadata.                                               *
// Show always if full=true.                                                                       *
// Returns true if title has changed.                                                              *
//**************************************************************************************************
bool showstreamtitle ( const char *ml, bool full )
{
  String raw = ml ? String ( ml ) : String() ;
  String streamtitle ;
  int titlepos = raw.indexOf ( "StreamTitle=" ) ;

  if ( titlepos >= 0 )
  {
    ESP_LOGI ( TAG, "Streamtitle found, %d bytes", raw.length() ) ;
    ESP_LOGI ( TAG, "%s", ml ) ;
    int begin = titlepos + 12 ;                  // Begin of artist and title
    int end = raw.indexOf ( ';', begin ) ;
    if ( end < 0 ) end = raw.length() ;
    if ( begin < end && ( raw[begin] == '\'' || raw[begin] == '"' ) )
    {
      char quote = raw[begin++] ;
      if ( end > begin && raw[end - 1] == quote ) end-- ;
    }
    streamtitle = raw.substring ( begin, end ) ;
  }
  else if ( full )
  {
    streamtitle = raw ;                          // Info probably from playlist
  }
  else
  {
    icystreamtitle = "" ;                       // Unknown type
    return false ;                              // Do not show
  }
  streamtitle = decode_spec_chars ( decodeStreamText ( streamtitle,
                                                        stream_charset ) ) ;
  // Save for status request from browser and for MQTT
  icystreamtitle = streamtitle ;
  String displaytitle = streamtitle ;
  int separator = displaytitle.indexOf ( " - " ) ;
  if ( separator >= 0 )                         // Look for artist/title separator
  {
    #ifdef NEXTION
      displaytitle = displaytitle.substring ( 0, separator ) + "\\r" +
                     displaytitle.substring ( separator + 3 ) ;
    #else
      displaytitle = displaytitle.substring ( 0, separator ) + "\n" +
                     displaytitle.substring ( separator + 3 ) ;
    #endif
  }
  if ( oldstreamtitle != displaytitle )          // Title changed?
  {
    oldstreamtitle = displaytitle ;
    tftset ( 1, displaytitle ) ;                // Yes, set screen segment text middle part
    return true ;                               // Return true if tiitle has changed
  }
  return false ;
}


//**************************************************************************************************
//                                    S E T D A T A M O D E                                        *
//**************************************************************************************************
// Change the datamode and show in debug for testing.                                              *
//**************************************************************************************************
void setdatamode ( datamode_t newmode )
{
  //ESP_LOGI ( TAG, "Change datamode from 0x%03X to 0x%03X",
  //           (int)datamode, (int)newmode ) ;
  datamode = newmode ;
}


//**************************************************************************************************
//                                    S T O P _ M P 3 C L I E N T                                  *
//**************************************************************************************************
// Disconnect from the server.                                                                     *
//**************************************************************************************************
struct stream_url_t
{
  bool     secure ;
  String   host ;
  String   hostheader ;
  String   path ;
  uint16_t port ;
} ;


// Split a radio URL without changing the value stored in presetinfo.host.  Old preferences that
// contain only host[:port]/path remain HTTP URLs; explicit http:// and https:// are preserved.
static bool parse_stream_url ( const String& source, stream_url_t& result )
{
  String url = source ;
  url.trim() ;
  String lowerurl = url ;
  lowerurl.toLowerCase() ;
  result.secure = false ;
  if ( lowerurl.startsWith ( "https://" ) )
  {
    result.secure = true ;
    url.remove ( 0, 8 ) ;
  }
  else if ( lowerurl.startsWith ( "http://" ) )
  {
    url.remove ( 0, 7 ) ;
  }
  else if ( url.indexOf ( "://" ) >= 0 )
  {
    ESP_LOGE ( TAG, "Unsupported stream URL scheme: %s", source.c_str() ) ;
    return false ;
  }

  int slash = url.indexOf ( '/' ) ;
  String authority = slash >= 0 ? url.substring ( 0, slash ) : url ;
  result.path = slash >= 0 ? url.substring ( slash ) : String ( "/" ) ;
  int fragment = result.path.indexOf ( '#' ) ;
  if ( fragment >= 0 ) result.path.remove ( fragment ) ;
  result.port = result.secure ? 443 : 80 ;

  int colon = authority.lastIndexOf ( ':' ) ;
  if ( colon >= 0 )
  {
    long parsedport = authority.substring ( colon + 1 ).toInt() ;
    if ( parsedport < 1 || parsedport > 65535 )
    {
      ESP_LOGE ( TAG, "Invalid stream port in URL: %s", source.c_str() ) ;
      return false ;
    }
    result.port = (uint16_t) parsedport ;
    result.host = authority.substring ( 0, colon ) ;
  }
  else
  {
    result.host = authority ;
  }
  result.host.trim() ;
  if ( result.host.isEmpty() || result.host.indexOf ( ' ' ) >= 0 ||
       result.host.indexOf ( '\r' ) >= 0 || result.host.indexOf ( '\n' ) >= 0 )
  {
    ESP_LOGE ( TAG, "Invalid stream host in URL: %s", source.c_str() ) ;
    return false ;
  }

  result.hostheader = result.host ;
  if ( ( result.secure && result.port != 443 ) ||
       ( !result.secure && result.port != 80 ) )
  {
    result.hostheader += String ( ':' ) + String ( result.port ) ;
  }
  return true ;
}


// Resolve all Location forms used in HTTP: absolute URL, scheme-relative URL, absolute path,
// query-only target and a path relative to the current resource.
static String resolve_redirect_url ( const String& baseurl, String location )
{
  location.trim() ;
  String lowerlocation = location ;
  lowerlocation.toLowerCase() ;
  if ( lowerlocation.startsWith ( "http://" ) || lowerlocation.startsWith ( "https://" ) )
  {
    return location ;
  }

  stream_url_t base ;
  if ( !parse_stream_url ( baseurl, base ) || location.isEmpty() ) return String() ;
  String prefix = String ( base.secure ? "https://" : "http://" ) + base.hostheader ;
  if ( location.startsWith ( "//" ) )
  {
    return String ( base.secure ? "https:" : "http:" ) + location ;
  }
  if ( location[0] == '/' ) return prefix + location ;

  String basepath = base.path ;
  int query = basepath.indexOf ( '?' ) ;
  if ( query >= 0 ) basepath.remove ( query ) ;
  if ( location[0] == '?' ) return prefix + basepath + location ;
  int lastslash = basepath.lastIndexOf ( '/' ) ;
  if ( lastslash >= 0 ) basepath.remove ( lastslash + 1 ) ;
  else basepath = "/" ;
  return prefix + basepath + location ;
}


static bool begin_stream_redirect ( const String& location )
{
  if ( stream_redirect_count >= 5 )
  {
    ESP_LOGE ( TAG, "Too many stream redirects; request stopped" ) ;
    return false ;
  }
  String redirected = resolve_redirect_url ( presetinfo.host, location ) ;
  if ( redirected.isEmpty() )
  {
    ESP_LOGE ( TAG, "Invalid redirect target: %s", location.c_str() ) ;
    return false ;
  }
  stream_redirect_count++ ;
  stream_redirect_request = true ;
  stream_redirect_playlist = ( datamode == PLAYLISTHEADER ) ;
  presetinfo.station_state = ST_REDIRECT ;
  presetinfo.host = redirected ;
  ESP_LOGI ( TAG, "Follow redirect %u to %s",
             stream_redirect_count, redirected.c_str() ) ;
  return true ;
}


void stop_mp3client ()
{
  queueToPt ( QSTOPSONG ) ;                        // Queue a request to stop the song
  if ( secure_stream_active )                       // HTTPS transport selected?
  {
    ESP_LOGI ( TAG, "Stopping HTTPS client" ) ;
    mp3secureclient.stop() ;
    secure_stream_active = false ;
  }
  while ( mp3client && mp3client->connected() )    // Client active and connected?
  {
    ESP_LOGI ( TAG, "Stopping client" ) ;          // Yes, stop connection to host
    //mp3client->close() ;                         // Causes memory leak!
    mp3client->abort() ;                           // This works better
    vTaskDelay ( 500 / portTICK_PERIOD_MS ) ;
  }
}


// WiFiClientSecure is synchronous, unlike the HTTP AsyncClient.  Drain a bounded amount on every
// main-loop pass and feed it into the existing header/metadata/audio parser.
void pump_secure_stream ()
{
  if ( !secure_stream_active || stream_redirect_request ) return ;
  static uint8_t buffer[1460] ;
  size_t handled = 0 ;
  while ( mp3secureclient.available() > 0 && handled < 5840 )
  {
    size_t available = (size_t)mp3secureclient.available() ;
    size_t wanted = available < sizeof ( buffer ) ? available : sizeof ( buffer ) ;
    int received = mp3secureclient.read ( buffer, wanted ) ;
    if ( received <= 0 ) break ;
    handled += received ;
    for ( int i = 0 ; i < received ; i++ )
    {
      handlebyte_ch ( buffer[i] ) ;
      if ( stream_redirect_request ) return ;        // Do not parse a redirect response body
    }
  }
}


//**************************************************************************************************
//                                    C O N N E C T T O H O S T                                    *
//**************************************************************************************************
// Connect to the Internet radio server specified by presetinfo and send the GET request.          *
//**************************************************************************************************
bool connecttohost()
{
  stream_url_t target ;                              // Parsed HTTP(S) URL
  String      auth  ;                                // For basic authentication
  String      getreq ;                               // GET command for MP3 host
  int         retrycount = 0 ;                       // Count for connect
  size_t      len ;                                  // Length of GET request
  bool        res = false ;                          // Function result, assume bad result
  bool        redirected_playlist = stream_redirect_request && stream_redirect_playlist ;

  stop_mp3client() ;                                 // Disconnect if still connected
  ad_suppression_active = false ;                    // A new connection starts with normal audio
  chomp ( presetinfo.host ) ;                        // Do some filtering
  if ( !stream_redirect_request )                    // User/preset initiated a new request?
  {
    stream_redirect_count = 0 ;                      // Start a fresh redirect chain
  }
  stream_redirect_request = false ;
  stream_redirect_playlist = false ;
  if ( !parse_stream_url ( presetinfo.host, target ) ) return false ;
  ESP_LOGI ( TAG, "Connect to host %s",
             presetinfo.host.c_str() ) ;
  tftset ( 0, NAME ) ;                               // Set screen segment text top line
  tftset ( 1, "" ) ;                                 // Clear song and artist
  displaytime ( "" ) ;                               // Clear time on TFT screen
  oldstreamtitle = "" ;                              // Force title redraw after reconnect
  stream_charset = "" ;                             // New response may use a different charset
  icyname_raw = "" ;                                // Forget station name from previous response
  setdatamode ( INIT ) ;                             // Start default in INIT mode
  chunked = false ;                                  // Assume not chunked
  String playlistpath = target.path ;
  int playlistquery = playlistpath.indexOf ( '?' ) ;
  if ( playlistquery >= 0 ) playlistpath.remove ( playlistquery ) ;
  playlistpath.toLowerCase() ;
  if ( redirected_playlist || playlistpath.endsWith ( ".m3u" ) ) // Is it an m3u playlist?
  {
    presetinfo.station_state = ST_PLAYLIST ;         // Yes, change station state
    presetinfo.playlisthost = presetinfo.host ;      // Save copy of playlist URL
    setdatamode ( PLAYLISTINIT ) ;                   // Yes, start in PLAYLIST mode
    ESP_LOGI ( TAG, "Playlist request, entry %d",
               presetinfo.playlistnr ) ;
  }
  if ( nvssearch ( "basicauth" ) )                  // Does "basicauth" exist?
  {
    auth = nvsgetstr ( "basicauth" ) ;               // Use basic authentication?
    if ( auth != "" )                                // Should be user:passwd
    {
      auth = base64::encode ( auth.c_str() ) ;        // Encode
      auth = String ( "Authorization: Basic " ) + auth + String ( "\r\n" ) ;
    }
  }
  getreq = String ( "GET " ) + target.path + String ( " HTTP/1.1\r\n" ) +
           String ( "Host: " ) + target.hostheader + String ( "\r\n" ) +
           String ( "Icy-MetaData: 1\r\n" ) + auth +
           String ( "User-Agent: ESP32-Radio-V2\r\n" ) +
           String ( "Accept: */*\r\n" ) +
           String ( "Connection: close\r\n\r\n" ) ;

  ESP_LOGI ( TAG, "Connect to %s on port %u using %s, path %s",
             target.host.c_str(), target.port,
             target.secure ? "HTTPS" : "HTTP", target.path.c_str() ) ;
  if ( target.secure )
  {
    mp3secureclient.setInsecure() ;                   // Radio URLs rarely provide a stable CA chain
    mp3secureclient.setTimeout ( 15000 ) ;
    if ( mp3secureclient.connect ( target.host.c_str(), target.port ) )
    {
      secure_stream_active = true ;
      ESP_LOGI ( TAG, "send HTTPS GET command" ) ;
      res = mp3secureclient.print ( getreq ) == getreq.length() ;
      if ( !res )
      {
        ESP_LOGE ( TAG, "HTTPS GET request was not sent completely" ) ;
        mp3secureclient.stop() ;
        secure_stream_active = false ;
      }
    }
  }
  else if ( mp3client && mp3client->connect ( target.host.c_str(), target.port ) )
  {
    while ( mp3client->disconnected() )              // Wait for connect
    {
      if ( retrycount++ > 50 )                       // For max 5 seconds
      {
        mp3client->abort() ;                         // No connect; avoid deprecated stop() and close() leak
        break ;                                      //
      }
      vTaskDelay ( 100 / portTICK_PERIOD_MS ) ;
    }
    if ( mp3client->connected() )
    {
      ESP_LOGI ( TAG, "send HTTP GET command" ) ;
      if ( mp3client->canSend() )
      {
        len = getreq.length() ;
        res = mp3client->write ( getreq.c_str(), len ) == len ;
      }
    }
  }
  if ( !res )
  {
    ESP_LOGE ( TAG, "%s request %s failed!",
               target.secure ? "HTTPS" : "HTTP", presetinfo.host.c_str() ) ;
  }
  return res ;
}


//**************************************************************************************************
//                                      S S C O N V                                                *
//**************************************************************************************************
// Convert an array with 4 "synchsafe integers" to a number.                                       *
// There are 7 bits used per byte.                                                                 *
//**************************************************************************************************
uint32_t ssconv ( const uint8_t* bytes )
{
  uint32_t res = 0 ;                                      // Result of conversion
  uint8_t  i ;                                            // Counter number of bytes to convert

  for ( i = 0 ; i < 4 ; i++ )                             // Handle 4 bytes
  {
    res = res * 128 + bytes[i] ;                          // Convert next 7 bits
  }
  return res ;                                            // Return the result
}

#ifdef ETHERNET
//**************************************************************************************************
//                                      E T H E V E N T                                            *
//**************************************************************************************************
// Will be executed on ethernet driver events.                                                     *
//**************************************************************************************************
void EthEvent ( WiFiEvent_t event )
{
  const char* fd = "" ;                               // Full duplex or not as a string

  switch ( event )                                    // What event?
  {
    case ARDUINO_EVENT_ETH_START :
      ESP_LOGI ( TAG, "ETH Started" ) ;               // Driver started
      ETH.setHostname ( NAME ) ;                      // Set the eth hostname now
      break ;
    case ARDUINO_EVENT_ETH_CONNECTED :
      ESP_LOGI ( TAG, "ETH cable connected" ) ;       // We have a connection
      break ;
    case ARDUINO_EVENT_ETH_GOT_IP :
      if ( ETH.fullDuplex() )                         // IP received from DHCP
      {
        fd = ", FULL_DUPLEX" ;                        // It is full duplex
      }
      ipaddress = ETH.localIP().toString() ;          // Remember for display
      ESP_LOGI ( TAG, "IPv4: %s, %d Mbps%s",          // Show status
                 ipaddress.c_str(),
                 ETH.linkSpeed(),
                 fd ) ;
      eth_connected = true ;                          // Set global flag: connection OK
      break ;
    case ARDUINO_EVENT_ETH_DISCONNECTED :
      ESP_LOGI ( TAG, "ETH cable disconnected" ) ;    // We have a disconnection
      eth_connected = false ;                         // Clear the global flag
      break ;
    case ARDUINO_EVENT_ETH_STOP :
      ESP_LOGI ( TAG, "ETH Stopped" ) ;
      eth_connected = false ;
      break ;
    default :
      ESP_LOGI ( TAG, "ETH event %d", (int)event ) ;  // Unknown event
      break ;
  }
}

//**************************************************************************************************
//                                       C O N N E C T E T H                                       *
//**************************************************************************************************
// Connect to Ethernet.                                                                            *
// If connection fails, the function returns false.                                                *
//**************************************************************************************************
bool connectETH()
{
  const char* pIP ;                                     // Pointer to IP address
  bool        res ;                                     // Result of connect
  int         tries = 0 ;                               // Counter for wait time
  
  ESP_LOGI ( TAG, "ETH pins %d, %d and %d",
             ini_block.eth_power_pin,
             ini_block.eth_mdc_pin,
             ini_block.eth_mdio_pin ) ;
  res =  ETH.begin ( ETH_ADDR, ini_block.eth_power_pin, // Start Ethernet driver
                     ini_block.eth_mdc_pin,
                     ini_block.eth_mdio_pin,
                     ETH_TYPE, ETH_CLK_MODE ) ;
  while ( res & ( ! eth_connected ) )                   // Wait for connect
  {
    vTaskDelay ( 1000 / portTICK_PERIOD_MS ) ;
    if ( tries++ == 10 )                                // Limit wait time
    {
      res = false ;                                     // No luck
    }
  }
  pIP = ipaddress.c_str() ;                             // As c-string
  ESP_LOGI ( TAG, "IP = %s", pIP ) ;
  tftlog ( "IP = " ) ;                                  // Show IP
  tftlog ( pIP, true ) ;
  #ifdef NEXTION
    dsp_println ( "\f" ) ;                              // Select new page if NEXTION 
  #endif
  return res ;                                          // Return result of connection
}

#else
static const char* wifi_reason_text ( uint8_t reason )
{
  switch ( reason )
  {
    case 0:   return "no disconnect reason reported" ;
    case 2:   return "authentication expired" ;
    case 4:   return "association expired" ;
    case 15:  return "four-way handshake timeout" ;
    case 201: return "access point not found" ;
    case 202: return "authentication failed" ;
    case 203: return "association failed" ;
    case 204: return "handshake timeout" ;
    case 208: return "association comeback time too long" ;
    default:  return "other disconnect reason" ;
  }
}


void WiFiEventHandler ( WiFiEvent_t event, WiFiEventInfo_t info )
{
  if ( event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED )
  {
    wifi_assoc_reason = info.wifi_sta_disconnected.reason ;
    wifi_assoc_event = true ;
  }
}


//**************************************************************************************************
//                                       C O N N E C T W I F I                                     *
//**************************************************************************************************
// Try every configured SSID directly.  Each network gets several attempts and a driver event      *
// terminates a refused association immediately instead of waiting for the complete timeout.       *
// If connection fails, an AP is created and the function returns false.                           *
//**************************************************************************************************
bool connectwifi()
{
  bool        connected = false ;                       // Connected to a configured network
  const char* pIP ;                                     // Pointer to IP address
  WifiInfo_t  winfo ;                                   // Entry from wifilist

  WiFi.softAPdisconnect ( true ) ;                      // Stop a possible configuration AP
  WiFi.mode ( WIFI_STA ) ;                              // This ESP is a station
  static bool event_registered = false ;
  if ( !event_registered )
  {
    WiFi.onEvent ( WiFiEventHandler ) ;                 // Register only once
    event_registered = true ;
  }
  WiFi.setSleep ( false ) ;                             // Keep stream connection responsive
  esp_err_t ps_result = esp_wifi_set_ps ( WIFI_PS_NONE ) ;
  if ( ps_result != ESP_OK )
  {
    ESP_LOGW ( TAG, "Could not disable WiFi power save, error 0x%X",
               (unsigned)ps_result ) ;
  }
  WiFi.setAutoReconnect ( true ) ;
  vTaskDelay ( 300 / portTICK_PERIOD_MS ) ;

  const uint8_t maxattempts = 3 ;
  for ( size_t network = 0 ; network < wifilist.size() && !connected ; network++ )
  {
    winfo = wifilist[network] ;
    for ( uint8_t attempt = 1 ; attempt <= maxattempts && !connected ; attempt++ )
    {
      ESP_LOGI ( TAG, "WiFi attempt %u/%u for SSID '%s'",
                 attempt, maxattempts, winfo.ssid.c_str() ) ;

      // Reset the association without erasing stored credentials or stopping the driver.
      WiFi.disconnect ( false, false ) ;
      vTaskDelay ( 250 / portTICK_PERIOD_MS ) ;
      wifi_assoc_event = false ;
      wifi_assoc_reason = 0 ;
      WiFi.begin ( winfo.ssid.c_str(), winfo.passphrase.c_str() ) ;

      const uint32_t started = millis() ;
      while ( WiFi.status() != WL_CONNECTED &&
              !wifi_assoc_event &&
              millis() - started < 10000UL )
      {
        vTaskDelay ( 50 / portTICK_PERIOD_MS ) ;
      }
      connected = WiFi.status() == WL_CONNECTED ;
      if ( connected )
      {
        ESP_LOGI ( TAG, "WiFi association succeeded for '%s' after %lu ms",
                   winfo.ssid.c_str(), (unsigned long)( millis() - started ) ) ;
        break ;
      }

      const uint8_t reason = wifi_assoc_reason ;
      ESP_LOGW ( TAG, "WiFi attempt failed: SSID '%s', status %d, reason %u (%s)",
                 winfo.ssid.c_str(), (int)WiFi.status(), reason,
                 wifi_reason_text ( reason ) ) ;
      WiFi.disconnect ( false, false ) ;
      if ( reason == 208 )                             // AP explicitly requests a later retry
      {
        vTaskDelay ( 1200 / portTICK_PERIOD_MS ) ;
      }
      else
      {
        vTaskDelay ( ( wifi_assoc_event ? 750 : 1000 ) / portTICK_PERIOD_MS ) ;
      }
      wifi_assoc_event = false ;
      wifi_assoc_reason = 0 ;
    }
  }

  if ( !connected )                                      // Must setup local AP?
  {
    ESP_LOGI ( TAG, "WiFi failed after direct retries. Trying to setup AP with"
               " name %s and password %s.",
               NAME, NAME ) ;
    WiFi.disconnect ( false, false ) ;                  // Preserve configured credentials
    WiFi.mode ( WIFI_AP_STA ) ;
    if ( ! WiFi.softAP ( NAME, NAME ) )                 // This ESP will be an AP
    {
      ESP_LOGE ( TAG, "AP failed" ) ;                   // Setup of AP failed
    }
    ipaddress = WiFi.softAPIP().toString() ;
  }
  else
  {
    const uint32_t dhcp_started = millis() ;
    while ( WiFi.localIP() == IPAddress ( 0, 0, 0, 0 ) &&
            millis() - dhcp_started < 5000UL )
    {
      vTaskDelay ( 50 / portTICK_PERIOD_MS ) ;
    }
    tftlog ( "SSID = " ) ;                              // Show SSID on display
    tftlog ( WiFi.SSID().c_str(), true ) ;
    ESP_LOGI ( TAG, "SSID = %s, RSSI = %d dBm",
               WiFi.SSID().c_str(), WiFi.RSSI() ) ;
    ipaddress = WiFi.localIP().toString() ;             // Form IP address
    ESP_LOGI ( TAG, "WiFi network ready for stream connections" ) ;
  }
  pIP = ipaddress.c_str() ;                             // As c-string
  ESP_LOGI ( TAG, "IP = %s", pIP ) ;
  tftlog ( "IP = " ) ;                                  // Show IP
  tftlog ( pIP, true ) ;
  #ifdef NEXTION
    vTaskDelay ( 2000 / portTICK_PERIOD_MS ) ;          // Show for some time
    dsp_println ( "\f" ) ;                              // Select new page if NEXTION 
  #endif
  return connected ;                                    // False means configuration AP mode
}
#endif

#ifdef ENABLEOTA
//**************************************************************************************************
//                                           O T A S T A R T                                       *
//**************************************************************************************************
// Update via WiFi/Ethernet has been started by Arduino IDE or PlatformIO.                         *
//**************************************************************************************************
void otastart()
{
  const char* p = "OTA update Started" ;

  ESP_LOGI ( TAG, "%s", p ) ;                      // Show event for debug
  tftset ( 2, p ) ;                                // Set screen segment bottom part
  if ( secure_stream_active )                      // Stop either active stream transport
  {
    mp3secureclient.stop() ;
    secure_stream_active = false ;
  }
  if ( mp3client ) mp3client->abort() ;
  timerAlarmDisable ( timer ) ;                    // Disable the timer
  disableCore0WDT() ;                              // Disable watchdog core 0
  disableCore1WDT() ;                              // Disable watchdog core 1
  queueToPt ( QSTOPTASK ) ;                        // Queue a request to stop the song
}


//**************************************************************************************************
//                                           O T A E R R O R                                       *
//**************************************************************************************************
// Update via WiFi has an error.                                                                   *
//**************************************************************************************************
void otaerror ( ota_error_t error)
{
  ESP_LOGE ( TAG, "OTA error %d", error ) ;
  tftset ( 2, "OTA error!" ) ;                        // Set screen segment bottom part
}
#endif                                                // ENABLEOTA


//**************************************************************************************************
//                                  R E A D H O S T F R O M P R E F                                *
//**************************************************************************************************
// Read the mp3 host from the preferences specified by the parameter.                              *
// The host will be returned.                                                                      *
// We search for "preset_x" or "preset_xx" or "preset_xxx".                                        *
//**************************************************************************************************
bool readhostfrompref ( int16_t preset, String* host, String* hsym )
{
  char           tkey[12] ;                            // Key as an array of char
  int            inx ;                                 // Position of comment in preset

  sprintf ( tkey, "preset_%d", preset ) ;              // Form the search key
  if ( !nvssearch ( tkey ) )                           // Does _x[x[x]] exists?
  {
    sprintf ( tkey, "preset_%03d", preset ) ;          // Form new search key
    if ( !nvssearch ( tkey ) )                         // Does _xxx exists?
    {
      sprintf ( tkey, "preset_%02d", preset ) ;        // Form new search key
    }
    if ( !nvssearch ( tkey ) )                         // Does _xx exists?
    {
      *host = String ( "" ) ;                          // Not found
      if ( hsym ) *hsym = *host ;                      // Symbolic name also unknown
      return false ;
    }
  }
  // Get the contents
  *host = nvsgetstr ( tkey ) ;                         // Get the station
  if ( hsym )                                          // Symbolic name parameter wanted?
  {
    *hsym = *host ;                                    // Symbolic name defailt equal to preset
    // See if comment if available.  Otherwise the preset itself.
    inx = hsym->indexOf ( "#" ) ;                      // Get position of "#"
    if ( inx > 0 )                                     // Hash sign present?
    {
      hsym->remove ( 0, inx + 1 ) ;                    // Yes, remove non-comment part
    }
    chomp ( *hsym ) ;                                  // Remove garbage from description
  }
  return true ;
}


//**************************************************************************************************
//                              S T A T I O N   S E L E C T I O N                                 *
//**************************************************************************************************
static bool presetExists ( int16_t preset )
{
  String host, name ;
  return readhostfrompref ( preset, &host, &name ) ;
}

static bool findPreset ( int16_t start, int8_t direction, uint8_t count,
                         int16_t* result )
{
  int16_t range = presetinfo.highest_preset + 1 ;
  if ( range <= 0 || !result ) return false ;
  int16_t preset = start ;
  for ( uint8_t step = 0 ; step < count ; step++ )
  {
    bool found = false ;
    for ( int16_t checked = 0 ; checked < range ; checked++ )
    {
      preset += direction ;
      if ( preset < 0 ) preset = range - 1 ;
      else if ( preset >= range ) preset = 0 ;
      if ( presetExists ( preset ) )
      {
        found = true ;
        break ;
      }
    }
    if ( !found ) return false ;
  }
  *result = preset ;
  return true ;
}

static void restoreRadioView()
{
  station_list_active = false ;
  station_list_sd = false ;
  station_list_scroll_needed = false ;
  if ( dsp_ok ) dsp_erase() ;
  displaytime ( "" ) ;
  #if defined(SDCARD) && defined(BLUETFT)
    if ( SD_playing )
    {
      String progressText = getSDProgressText() ;
      tftdata[0].str = progressText ;
      displayplaytime ( "" ) ;                           // Force a clean incremental redraw
      for ( uint8_t i = 0 ; i < 3 && i < TFTSECS ; i++ )
      {
        tftdata[i].update_req = true ;                   // Restore title and artist after menu
      }
      return ;
    }
  #endif
  tftset ( 0, NAME ) ;
  tftset ( 1, "" ) ;
  tftset ( 2, presetinfo.hsym ) ;
}

static void drawStationList ( bool selectedOnly = false )
{
  #ifdef BLUETFT
    const uint8_t rowCount = 7 ;
    const uint8_t middle = rowCount / 2 ;
    String rowText[rowCount] ;
    const char* rows[rowCount] ;
    int16_t rowPreset[rowCount] ;
    rowPreset[middle] = station_list_sd ? station_list_sd_index : station_list_preset ;
    if ( station_list_sd )
    {
      #ifdef SDCARD
      if ( SD_filecount <= 0 ) return ;
      if ( !selectedOnly )
      {
        for ( int8_t row = middle - 1 ; row >= 0 ; row-- )
        {
          rowPreset[row] = rowPreset[row + 1] - 1 ;
        }
        for ( uint8_t row = middle + 1 ; row < rowCount ; row++ )
        {
          rowPreset[row] = rowPreset[row - 1] + 1 ;
        }
      }
      #endif
    }
    else if ( !selectedOnly )
    {
      for ( int8_t row = middle - 1 ; row >= 0 ; row-- )
      {
        rowPreset[row] = rowPreset[row + 1] ;
        findPreset ( rowPreset[row + 1], -1, 1, &rowPreset[row] ) ;
      }
      for ( uint8_t row = middle + 1 ; row < rowCount ; row++ )
      {
        rowPreset[row] = rowPreset[row - 1] ;
        findPreset ( rowPreset[row - 1], 1, 1, &rowPreset[row] ) ;
      }
    }
    uint8_t firstRow = selectedOnly ? middle : 0 ;
    uint8_t lastRow = selectedOnly ? middle + 1 : rowCount ;
    #ifdef SDCARD
      int16_t savedSDIndex = SD_curindex ;
      bool savedRandomPlay = randomplay ;
    #endif
    for ( uint8_t row = firstRow ; row < lastRow ; row++ )
    {
      String host, name ;
      if ( station_list_sd )
      {
        #ifdef SDCARD
          if ( rowPreset[row] >= 0 && rowPreset[row] < SD_filecount )
          {
            const char* path = getSDFileName ( rowPreset[row] ) ;
            name = path ? String ( path ) : String ( "" ) ;
            int slash = name.lastIndexOf ( '/' ) ;
            if ( slash >= 0 ) name.remove ( 0, slash + 1 ) ;
            int dot = name.lastIndexOf ( '.' ) ;
            if ( dot > 0 ) name.remove ( dot ) ;
          }
        #endif
      }
      else
      {
        readhostfrompref ( rowPreset[row], &host, &name ) ;
        if ( name.isEmpty() ) name = host ;
      }
      chomp ( name ) ;
      if ( station_list_sd )
      {
        #ifdef SDCARD
          if ( rowPreset[row] >= 0 && rowPreset[row] < SD_filecount )
          {
            rowText[row] = String ( row == middle ? ">" : " " ) +
                           String ( rowPreset[row] + 1 ) + " " + name ;
          }
          else
          {
            rowText[row] = "" ;                          // Never repeat tracks into empty rows
          }
        #else
          rowText[row] = "" ;
        #endif
      }
      else
      {
        rowText[row] = String ( rowPreset[row] == presetinfo.preset ? ">" : " " ) +
                       String ( rowPreset[row] ) + " " + name ;
      }
      rows[row] = rowText[row].c_str() ;
    }
    #ifdef SDCARD
      if ( station_list_sd && savedSDIndex >= 0 && savedSDIndex < SD_filecount )
      {
        getSDFileName ( savedSDIndex ) ;                 // Browsing must not change current track
        randomplay = savedRandomPlay ;
      }
    #endif
    station_list_scroll_needed =
      bluetft_drawStationList ( rows, rowCount, middle,
                                station_list_scroll, selectedOnly ) ;
  #else
    String host, name ;
    readhostfrompref ( station_list_preset, &host, &name ) ;
    tftset ( 3, name ) ;
  #endif
}

void enterStationDigit ( uint8_t digit )
{
  if ( digit > 9 ) return ;
  if ( station_list_active ) restoreRadioView() ;
  if ( !station_number_entry ) station_number_input = "" ;
  station_number_entry = true ;
  if ( station_number_input.length() < 3 ) station_number_input += String ( digit ) ;
  station_number_time = millis() ;
  tftset ( 2, (const char*)NULL ) ;                 // Redraw name with purple entered number
}

void confirmStationNumber()
{
  if ( !station_number_entry ) return ;
  int16_t requested = station_number_input.toInt() ;
  station_number_entry = false ;
  station_number_input = "" ;
  if ( presetExists ( requested ) )
  {
    presetinfo.station_state = ST_PRESET ;
    if ( nextPreset ( requested ) )
    {
      restoreRadioView() ;
      if ( NetworkFound ) myQueueSend ( radioqueue, &startcmd ) ;
      return ;
    }
  }
  tftset ( 2, (const char*)NULL ) ;                 // Unknown number: keep current station
}

void openStationList()
{
  if ( !dsp_ok ) return ;
  #ifdef SDCARD
    SD_cover_visible = false ;                         // Any menu exits full-screen cover mode
  #endif
  station_list_sd = false ;
  station_number_entry = false ;
  station_number_input = "" ;
  station_list_preset = presetinfo.preset ;
  if ( !presetExists ( station_list_preset ) &&
       !findPreset ( 0, 1, 1, &station_list_preset ) ) return ;
  station_list_active = true ;
  station_list_scroll = 0 ;
  station_list_scroll_time = millis() ;
  drawStationList() ;
}

void openSDList()
{
  #ifdef SDCARD
    if ( !dsp_ok ) return ;
    SD_cover_visible = false ;                         // Any menu exits full-screen cover mode
    if ( !SD_okay || SD_filecount <= 0 || !sdqueue )
    {
      ESP_LOGI ( TAG, "SD track list is not ready or is empty" ) ;
      return ;
    }
    station_number_entry = false ;
    station_number_input = "" ;
    station_list_sd = true ;
    station_list_sd_index = SD_curindex ;
    if ( station_list_sd_index < 0 || station_list_sd_index >= SD_filecount )
    {
      station_list_sd_index = 0 ;
    }
    station_list_active = true ;
    station_list_scroll = 0 ;
    station_list_scroll_time = millis() ;
    drawStationList() ;
    ESP_LOGI ( TAG, "SD track list opened with %d entries", SD_filecount ) ;
  #else
    ESP_LOGI ( TAG, "SD support is disabled" ) ;
  #endif
}

void moveStationList ( int8_t direction, uint8_t count )
{
  if ( !station_list_active || !direction || !count ) return ;
  #ifdef SDCARD
    if ( station_list_sd )
    {
      if ( SD_filecount <= 0 ) return ;
      int32_t next = station_list_sd_index + ( (int32_t)direction * count ) ;
      if ( next < 0 ) next = 0 ;
      if ( next >= SD_filecount ) next = SD_filecount - 1 ;
      if ( next == station_list_sd_index ) return ;
      station_list_sd_index = next ;
      station_list_scroll = 0 ;
      station_list_scroll_time = millis() ;
      drawStationList() ;
      return ;
    }
  #endif
  int16_t next = station_list_preset ;
  if ( findPreset ( station_list_preset, direction, count, &next ) )
  {
    station_list_preset = next ;
    station_list_scroll = 0 ;
    station_list_scroll_time = millis() ;
    drawStationList() ;
  }
}

void confirmStationList()
{
  if ( !station_list_active )
  {
    openStationList() ;
    return ;
  }
  #ifdef SDCARD
    if ( station_list_sd )
    {
      int16_t requested = station_list_sd_index ;
      const char* path = getSDFileName ( requested ) ;
      if ( path && *path )
      {
        String selectedPath = String ( path ) ;
        restoreRadioView() ;
        getSDFileName ( requested ) ;                     // Restore after display redraw activity
        ESP_LOGI ( TAG, "SD list selected track %d: %s",
                   requested + 1, selectedPath.c_str() ) ;
        myQueueSend ( sdqueue, &startcmd ) ;
      }
      return ;
    }
  #endif
  int16_t requested = station_list_preset ;
  presetinfo.station_state = ST_PRESET ;
  if ( nextPreset ( requested ) )
  {
    restoreRadioView() ;
    if ( NetworkFound ) myQueueSend ( radioqueue, &startcmd ) ;
  }
}


//**************************************************************************************************
//                                       R E A D P R O G B U T T O N S                             *
//**************************************************************************************************
// Read the preferences for the programmable input pins and the touch pins.                        *
//**************************************************************************************************
void readprogbuttons()
{
  char        mykey[20] ;                                   // For numerated key
  int8_t      pinnr ;                                       // GPIO pinnumber to fill
  int         i ;                                           // Loop control
  String      val ;                                         // Contents of preference entry

  for ( i = 0 ; ( pinnr = progpin[i].gpio ) >= 0 ; i++ )    // Scan for all programmable pins
  {
    //ESP_LOGI ( TAG, "Check programmable GPIO_%02d",pinnr ) ;
    sprintf ( mykey, "gpio_%02d", pinnr ) ;                 // Form key in preferences
    if ( nvssearch ( mykey ) )
    {
      //ESP_LOGI ( TAG, "Pin in NVS" ) ;
      val = nvsgetstr ( mykey ) ;                           // Get the contents
      if ( val.length() )                                   // Does it exists?
      {
        if ( !progpin[i].reserved )                         // Do not use reserved pins
        {
          progpin[i].avail = true ;                         // This one is active now
          progpin[i].command = val ;                        // Set command
          ESP_LOGI ( TAG, "gpio_%02d will execute %s",      // Show result
                     pinnr, val.c_str() ) ;
        }
      }
    }
  }
  // Now for the touch pins 0..9, identified by their GPIO pin number
  for ( i = 0 ; ( pinnr = touchpin[i].gpio ) >= 0 ; i++ )   // Scan for all programmable pins
  {
    sprintf ( mykey, "touch_%02d", i ) ;                    // Form key in preferences
    if ( nvssearch ( mykey ) )
    {
      val = nvsgetstr ( mykey ) ;                           // Get the contents
      if ( val.length() )                                   // Does it exists?
      {
        if ( !touchpin[i].reserved )                        // Do not use reserved pins
        {
          touchpin[i].avail = true ;                        // This one is active now
          touchpin[i].command = val ;                       // Set command
          //pinMode ( touchpin[i].gpio,  INPUT ) ;          // Free floating input
          ESP_LOGI ( TAG, "touch_%02d will execute %s",     // Show result
                     i, val.c_str() ) ;
          ESP_LOGI ( TAG, "Level is now %d",
                     touchRead ( pinnr ) ) ;                // Sample the pin
        }
        else
        {
          ESP_LOGE ( TAG, "touch_%02d pin (GPIO%02d) is reserved for I/O!",
                     i, pinnr ) ;
        }
      }
    }
  }
}


//**************************************************************************************************
//                                       R E S E R V E P I N                                       *
//**************************************************************************************************
// Set I/O pin to "reserved".                                                                      *
// The pin will be unavailable for a programmable function.                                        *
//**************************************************************************************************
void reservepin ( int8_t rpinnr )
{
  uint8_t i = 0 ;                                           // Index in progpin/touchpin array
  int8_t  pin ;                                             // Pin number in progpin array

  while ( ( pin = progpin[i].gpio ) >= 0 )                  // Find entry for requested pin
  {
    if ( pin == rpinnr )                                    // Entry found?
    {
      if ( progpin[i].reserved )                            // Already reserved?
      {
        ESP_LOGE ( TAG, "Pin %d is already reserved!", rpinnr ) ;
      }
      //ESP_LOGE ( TAG, "GPIO%02d unavailabe for 'gpio_'-command",
      //           pin ) ;
      progpin[i].reserved = true ;                          // Yes, pin is reserved now
      break ;                                               // No need to continue
    }
    i++ ;                                                   // Next entry
  }
  // Also reserve touchpin numbers
  i = 0 ;
  while ( ( pin = touchpin[i].gpio ) >= 0 )                 // Find entry for requested pin
  {
    if ( pin == rpinnr )                                    // Entry found?
    {
      //ESP_LOGI ( TAG, "GPIO%02d unavailabe for touch command",
      //           pin ) ;
      touchpin[i].reserved = true ;                         // Yes, pin is reserved now
      break ;                                               // No need to continue
    }
    i++ ;                                                   // Next entry
  }
}


//**************************************************************************************************
//                                       R E A D I O P R E F S                                     *
//**************************************************************************************************
// Scan the preferences for IO-pin definitions.                                                    *
//**************************************************************************************************
void readIOprefs()
{
  struct iosetting
  {
    const char* gname ;                                   // Name in preferences
    int8_t*     gnr ;                                     // Address of target GPIO pin number
    int8_t      pdefault ;                                // Default pin
  };
  struct iosetting klist[] = {                            // List of I/O related keys
      { "pin_ir",        &ini_block.ir_pin,           -1 },
      { "pin_enc_clk",   &ini_block.enc_clk_pin,      -1 }, // Rotary encoder CLK
      { "pin_enc_dt",    &ini_block.enc_dt_pin,       -1 }, // Rotary encoder DT
      { "pin_enc_up",    &ini_block.enc_up_pin,       -1 }, // ZIPPY B5 side switch up
      { "pin_enc_dwn",   &ini_block.enc_dwn_pin,      -1 }, // ZIPPY B5 side switch down
      { "pin_enc_sw",    &ini_block.enc_sw_pin,       -1 },
      { "pin_tft_cs",    &ini_block.tft_cs_pin,       -1 }, // Display SPI version
      { "pin_tft_dc",    &ini_block.tft_dc_pin,       -1 }, // Display SPI version
      { "pin_tft_scl",   &ini_block.tft_scl_pin,      -1 }, // Display I2C version
      { "pin_tft_sda",   &ini_block.tft_sda_pin,      -1 }, // Display I2C version
      { "pin_tft_bl",    &ini_block.tft_bl_pin,       -1 }, // Display backlight
      { "pin_tft_blx",   &ini_block.tft_blx_pin,      -1 }, // Display backlight (inversed logic)
      { "pin_nxt_rx",    &ini_block.nxt_rx_pin,       -1 }, // NEXTION input pin
      { "pin_nxt_tx",    &ini_block.nxt_tx_pin,       -1 }, // NEXTION output pin
      { "pin_sd_cs",     &ini_block.sd_cs_pin,        -1 }, // SD card select
      { "pin_sd_detect", &ini_block.sd_detect_pin,    -1 }, // SD card detect
    #if defined(DEC_VS1053) || defined(DEC_VS1003)
      { "pin_vs_cs",     &ini_block.vs_cs_pin,        -1 }, // VS1053 pins
      { "pin_vs_dcs",    &ini_block.vs_dcs_pin,       -1 },
      { "pin_vs_dreq",   &ini_block.vs_dreq_pin,      -1 },
    #endif
      { "pin_shutdown",  &ini_block.shutdown_pin,     -1 }, // Amplifier shut-down pin
      { "pin_shutdownx", &ini_block.shutdownx_pin,    -1 }, // Amplifier shut-down pin (inversed logic)
    #ifdef DEC_HELIX
     #ifndef DEC_HELIX_INT
      #ifdef DEC_HELIX_SPDIF
      { "pin_i2s_spdif", &ini_block.i2s_spdif_pin,    -1 },
      #else
      { "pin_i2s_bck",   &ini_block.i2s_bck_pin,      -1 }, // I2S interface pins
      { "pin_i2s_lck",   &ini_block.i2s_lck_pin,      -1 },
      { "pin_i2s_din",   &ini_block.i2s_din_pin,      -1 },
      #endif
     #endif
    #endif
    #ifdef ETHERNET
      { "pin_spi_sck",   &ini_block.spi_sck_pin,      -1 },
      { "pin_spi_miso",  &ini_block.spi_miso_pin,     -1 },
      { "pin_spi_mosi",  &ini_block.spi_mosi_pin,     -1 },
      { "pin_eth_mdc",   &ini_block.eth_mdc_pin,      23 },
      { "pin_eth_mdio",  &ini_block.eth_mdio_pin,     18 },
      { "pin_eth_power", &ini_block.eth_power_pin,    16 },
    #else
      { "pin_spi_sck",   &ini_block.spi_sck_pin,      SCK  }, // Note: different for AI Audio kit (14)
      { "pin_spi_miso",  &ini_block.spi_miso_pin,     MISO }, // Note: different for AI Audio kit (2)
      { "pin_spi_mosi",  &ini_block.spi_mosi_pin,     MOSI }, // Note: different for AI Audio kit (15)
    #endif
      { NULL,            NULL,                        0  }  // End of list
  } ;
  int         i ;                                         // Loop control
  int         count = 0 ;                                 // Number of keys found
  String      val ;                                       // Contents of preference entry
  int8_t      ival ;                                      // Value converted to integer
  int8_t*     p ;                                         // Points to variable

  for ( i = 0 ; klist[i].gname ; i++ )                    // Loop trough all I/O related keys
  {
    p = klist[i].gnr ;                                    // Point to target variable
    ival = klist[i].pdefault ;                            // Assume pin number to be the default
    if ( nvssearch ( klist[i].gname ) )                   // Does it exist?
    {
      val = nvsgetstr ( klist[i].gname ) ;                // Read value of key
      if ( val.length() )                                 // Parameter in preference?
      {
        count++ ;                                         // Yes, count number of filled keys
        ival = val.toInt() ;                              // Convert value to integer pinnumber
        reservepin ( ival ) ;                             // Set pin to "reserved"
      }
    }
    *p = ival ;                                           // Set pinnumber in ini_block
    if ( ival >= 0 )                                      // Only show configured pins
    {
      ESP_LOGI ( TAG, "'%-13s' set to %d",      // Show result
                 klist[i].gname,
                 ival ) ;
    }
  }
}


//**************************************************************************************************
//                                       R E A D P R E F S                                         *
//**************************************************************************************************
// Read the preferences and interpret the commands.                                                *
// If output == true, the key / value pairs are returned to the caller as a String.                *
//**************************************************************************************************
String readprefs ( bool output )
{
  uint16_t    i ;                                           // Loop control
  String      val ;                                         // Contents of preference entry
  String      cmd ;                                         // Command for analyzCmd
  String      outstr = "" ;                                 // Outputstring
  char*       key ;                                         // Point to nvskeys[i]
  uint16_t    last2char = 0 ;                               // To detect paragraphs
  int         presetnr ;                                    // Preset number
 
  presetinfo.highest_preset = 0 ;                           // Number of presets may be shorter
  for ( i = 0 ; i < MAXKEYS ; i++ )                         // Loop trough all available keys
  {
    key = nvskeys[i] ;                                      // Examine next key
    //ESP_LOGI ( TAG, "Key[%d] is %s", i, key ) ;
    if ( *key == '\0' ) break ;                             // Stop on end of list
    val = nvsgetstr ( key ) ;                               // Read value of this key
    cmd = String ( key ) +                                  // Yes, form command
          String ( " = " ) +
          val ;
    if ( cmd.startsWith ( "preset_") )                      // Preset definition?
    {
      presetnr = atoi ( key + 7 ) ;                         // Yes, get preset number
      if ( presetnr > presetinfo.highest_preset )         
      {
        presetinfo.highest_preset = presetnr ;              // Found new max
      }
    }
    if ( output )
    {
      if ( ( i > 0 ) &&
           ( *(uint16_t*)key != last2char ) )               // New paragraph?
      {
        outstr += String ( "#\n" ) ;                        // Yes, add separator
      }
      last2char = *(uint16_t*)key ;                         // Save 2 chars for next compare
      outstr += String ( key ) +                            // Add to outstr
                String ( " = " ) +
                val +
                String ( "\n" ) ;                           // Add newline
    }
    else
    {
      analyzeCmd ( cmd.c_str() ) ;                          // Analyze it
    }
  }
  if ( i == 0 )                                             // Any key seen?
  {
    outstr = String ( "No preferences found.\n"
                      "Use defaults and edit options first.\n" ) ;
  }
  return outstr ;
}


//**************************************************************************************************
//                                    M Q T T R E C O N N E C T                                    *
//**************************************************************************************************
// Reconnect to broker.                                                                            *
//**************************************************************************************************
bool mqttreconnect()
{
  static uint32_t retrytime = 0 ;                         // Limit reconnect interval
  bool            res = false ;                           // Connect result
  char            clientid[20] ;                          // Client ID
  char            subtopic[60] ;                          // Topic to subscribe

  if ( ( millis() - retrytime ) < 5000 )                  // Don't try to frequently
  {
    return res ;
  }
  retrytime = millis() ;                                  // Set time of last try
  if ( mqttcount > MAXMQTTCONNECTS )                      // Tried too much?
  {
    mqtt_on = false ;                                     // Yes, switch off forever
    return res ;                                          // and quit
  }
  mqttcount++ ;                                           // Count the retries
  ESP_LOGI ( TAG, "(Re)connecting number %d to MQTT %s",  // Show some debug info
             mqttcount,
             ini_block.mqttbroker.c_str() ) ;
  sprintf ( clientid, "%s-%04d",                          // Generate client ID
            NAME, (int) random ( 10000 ) % 10000 ) ;
  res = mqttclient.connect ( clientid,                    // Connect to broker
                             ini_block.mqttuser.c_str(),
                             ini_block.mqttpasswd.c_str()
                           ) ;
  if ( res )
  {
    sprintf ( subtopic, "%s/%s",                          // Add prefix to subtopic
              ini_block.mqttprefix.c_str(),
              MQTT_SUBTOPIC ) ;
    res = mqttclient.subscribe ( subtopic ) ;             // Subscribe to MQTT
    if ( !res )
    {
      ESP_LOGE ( TAG, "MQTT subscribe failed!" ) ;        // Failure
    }
    mqttpub.trigger ( MQTT_IP ) ;                         // Publish own IP
  }
  else
  {
    ESP_LOGE ( TAG, "MQTT connection failed, rc=%d",
               mqttclient.state() ) ;

  }
  return res ;
}


//**************************************************************************************************
//                                    O N M Q T T M E S S A G E                                    *
//**************************************************************************************************
// Executed when a subscribed message is received.                                                 *
// Note that message is not delimited by a '\0'.                                                   *
// Note that cmd buffer is shared with serial input.                                               *
//**************************************************************************************************
void onMqttMessage ( char* topic, byte* payload, unsigned int len )
{
  const char*  reply ;                                // Result from analyzeCmd

  if ( strstr ( topic, MQTT_SUBTOPIC ) )              // Check on topic, maybe unnecessary
  {
    if ( len >= sizeof(cmd) )                         // Message may not be too long
    {
      len = sizeof(cmd) - 1 ;
    }
    strncpy ( cmd, (char*)payload, len ) ;            // Make copy of message
    cmd[len] = '\0' ;                                 // Take care of delimeter
    ESP_LOGI ( TAG, "MQTT message arrived [%s], lenght = %d, %s", topic, len, cmd ) ;
    reply = analyzeCmd ( cmd ) ;                      // Analyze command and handle it
    ESP_LOGI ( TAG, "%s", reply ) ;                   // Result for debugging
  }
}


//**************************************************************************************************
//                                     S C A N S E R I A L                                         *
//**************************************************************************************************
// Listen to commands on the Serial inputline.                                                     *
//**************************************************************************************************
void scanserial()
{
  static String serialcmd ;                      // Command from Serial input
  char          c ;                              // Input character
  const char*   reply = "" ;                     // Reply string from analyzeCmd
  uint16_t      len ;                            // Length of input string

  while ( Serial.available() )                   // Any input seen?
  {
    c =  (char)Serial.read() ;                   // Yes, read the next input character
    //Serial.write ( c ) ;                       // Echo
    len = serialcmd.length() ;                   // Get the length of the current string
    if ( ( c == '\n' ) || ( c == '\r' ) )
    {
      if ( len )
      {
        strncpy ( cmd, serialcmd.c_str(), sizeof(cmd) ) ;
        reply = analyzeCmd ( cmd ) ;             // Analyze command and handle it
        log_printf ( "%s", reply ) ;             // Result for debugging
        serialcmd = "" ;                         // Prepare for new command
      }
    }
    if ( c >= ' ' )                              // Only accept useful characters
    {
      serialcmd += c ;                           // Add to the command
    }
    if ( len >= ( sizeof(cmd) - 2 )  )           // Check for excessive length
    {
      serialcmd = "" ;                           // Too long, reset
    }
  }
}

#ifdef NEXTION 
//**************************************************************************************************
//                                     S C A N S E R I A L 2                                       *
//**************************************************************************************************
// Listen to commands on the 2nd Serial inputline (NEXTION).                                       *
//**************************************************************************************************
void scanserial2()
{
  static String  serialcmd ;                       // Command from Serial input
  char           c ;                               // Input character
  const char*    reply = "" ;                      // Reply string from analyzeCmd
  uint16_t       len ;                             // Length of input string
  static uint8_t ffcount = 0 ;                     // Counter for 3 tmes "0xFF"

  if ( nxtserial )                                 // NEXTION active?
  {
    while ( nxtserial->available() )               // Yes, any input seen?
    {
      c =  (char)nxtserial->read() ;               // Yes, read the next input character
      len = serialcmd.length() ;                   // Get the length of the current string
      if ( c == 0xFF )                             // End of command?
      {
        if ( ++ffcount < 3 )                       // 3 times FF?
        {
          continue ;                               // No, continue to read
        }
        ffcount = 0 ;                              // For next command
        if ( len )
        {
          strncpy ( cmd, serialcmd.c_str(), sizeof(cmd) ) ;
          ESP_LOGI ( TAG, "NEXTION command seen %02X %s",
                     cmd[0], cmd + 1 ) ;
          if ( cmd[0] == 0x70 )                    // Button pressed?
          { 
            reply = analyzeCmd ( cmd + 1 ) ;       // Analyze command and handle it
            ESP_LOGI ( TAG, "%s", reply ) ;        // Result for debugging
          }
          serialcmd = "" ;                         // Prepare for new command
        }
      }
      else if ( c >= ' ' )                         // Only accept useful characters
      {
        serialcmd += c ;                           // Add to the command
      }
      if ( len >= ( sizeof(cmd) - 2 )  )           // Check for excessive length
      {
        serialcmd = "" ;                           // Too long, reset
      }
    }
  }
}
#else
#define scanserial2()                              // Empty version if no NEXTION
#endif


//**************************************************************************************************
//                                     S C A N D I G I T A L                                       *
//**************************************************************************************************
// Scan digital inputs.                                                                            *
//**************************************************************************************************
void  scandigital()
{
  static uint32_t oldmillis = 5000 ;                        // To compare with current time
  int             i ;                                       // Loop control
  int8_t          pinnr ;                                   // Pin number to check
  bool            level ;                                   // Input level
  const char*     reply ;                                   // Result of analyzeCmd
  int16_t         tlevel ;                                  // Level found by touch pin
  const int16_t   THRESHOLD = 30 ;                          // Threshold or touch pins

  if ( ( millis() - oldmillis ) < 100 )                     // Debounce
  {
    return ;
  }
  oldmillis = millis() ;                                    // 100 msec over
  for ( i = 0 ; ( pinnr = progpin[i].gpio ) >= 0 ; i++ )    // Scan all inputs
  {
    if ( !progpin[i].avail || progpin[i].reserved )         // Skip unused and reserved pins
    {
      continue ;
    }
    level = ( digitalRead ( pinnr ) == HIGH ) ;             // Sample the pin
    if ( level != progpin[i].cur )                          // Change seen?
    {
      progpin[i].cur = level ;                              // And the new level
      if ( !level )                                         // HIGH to LOW change?
      {
        ESP_LOGI ( TAG, "GPIO_%02d is now LOW, execute %s",
                   pinnr, progpin[i].command.c_str() ) ;
        reply = analyzeCmd ( progpin[i].command.c_str() ) ; // Analyze command and handle it
        ESP_LOGI ( TAG, "%s", reply ) ;                     // Result for debugging
      }
    }
  }
  // Now for the touch pins
  for ( i = 0 ; ( pinnr = touchpin[i].gpio ) >= 0 ; i++ )   // Scan all inputs
  {
    if ( !touchpin[i].avail || touchpin[i].reserved )       // Skip unused and reserved pins
    {
      continue ;
    }
    tlevel = ( touchRead ( pinnr ) ) ;                      // Sample the pin
    level = ( tlevel >= THRESHOLD ) ;                       // True if below threshold
    if ( level )                                            // Level HIGH?
    {
      touchpin[i].count = 0 ;                               // Reset count number of times
    }
    else
    {
      if ( ++touchpin[i].count < 3 )                        // Count number of times LOW
      {
        level = true ;                                      // Not long enough: handle as HIGH
      }
    }
    if ( level != touchpin[i].cur )                         // Change seen?
    {
      touchpin[i].cur = level ;                             // And the new level
      if ( !level )                                         // HIGH to LOW change?
      {
        ESP_LOGI ( TAG, "TOUCH_%02d is now %d ( < %d ), execute %s",
                   pinnr, tlevel, THRESHOLD,
                   touchpin[i].command.c_str() ) ;
        reply = analyzeCmd ( touchpin[i].command.c_str() ); // Analyze command and handle it
        ESP_LOGI ( TAG, "%s", reply ) ;                     // Result for debugging
      }
    }
  }
}


//**************************************************************************************************
//                                     S C A N I R                                                 *
//**************************************************************************************************
// See if IR input is available.  Execute the programmed command.                                  *
//**************************************************************************************************
struct IRDefault_t
{
  uint16_t    code ;
  const char* command ;
} ;

static const IRDefault_t irConfigDefaults[] =
{
  { 0x02FD, "ok" },
  { 0x4AB5, "preset = 0" },
  { 0x6897, "preset = 1" },
  { 0x9867, "preset = 2" },
  { 0xB04F, "preset = 3" },
  { 0x30CF, "preset = 4" },
  { 0x18E7, "preset = 5" },
  { 0x7A85, "preset = 6" },
  { 0x10EF, "preset = 7" },
  { 0x38C7, "preset = 8" },
  { 0x5AA5, "preset = 9" },
  { 0x22DD, "downpreset = 1" },
  { 0xC23D, "uppreset = 1" },
  { 0xA857, "downvolume = 2" },
  { 0x629D, "upvolume = 2" },
  { 0x42BD, "sdlist" }
} ;

static const uint8_t IR_CONFIG_VERSION = 3 ;

// Copy the former built-in mappings to editable NVS preferences once.  The numeric version marker
// is not shown by readprefs(), which intentionally lists string settings only.
static bool migrateIRConfigToPreferences()
{
  uint8_t version = 0 ;
  bool    changed = false ;
  char    key[12] ;

  nvsopen() ;
  if ( nvs_get_u8 ( nvshandle, "ir_cfg_ver", &version ) == ESP_OK &&
       version >= IR_CONFIG_VERSION )
  {
    return false ;                                      // Config is already authoritative
  }

  for ( const IRDefault_t& item : irConfigDefaults )
  {
    sprintf ( key, "ir_%04X", item.code ) ;
    if ( !nvssearch ( key ) )                           // Preserve every existing custom mapping
    {
      nvssetstr ( key, String ( item.command ) ) ;
      changed = true ;
    }
  }

  if ( nvssearch ( "ir_02FD" ) )                       // Repair the old invalid OK assignment
  {
    String command = nvsgetstr ( "ir_02FD" ) ;
    command.trim() ;
    command.toLowerCase() ;
    if ( command == "stop" )
    {
      nvssetstr ( "ir_02FD", String ( "ok" ) ) ;
      changed = true ;
    }
  }
  // V17 assigns the physical '*' key to the SD menu.  Replace the older numeric/default action
  // once; after this migration a later user edit in Config remains authoritative.
  if ( nvssearch ( "ir_42BD" ) )
  {
    String command = nvsgetstr ( "ir_42BD" ) ;
    command.trim() ;
    command.toLowerCase() ;
    if ( command != "sdlist" )
    {
      nvssetstr ( "ir_42BD", String ( "sdlist" ) ) ;
      changed = true ;
    }
  }
  nvs_set_u8 ( nvshandle, "ir_cfg_ver", IR_CONFIG_VERSION ) ;
  nvs_commit ( nvshandle ) ;
  ESP_LOGI ( TAG, "IR mappings migrated to editable configuration" ) ;
  return changed ;
}

#ifdef SDCARD
static const uint8_t SD_CONFIG_VERSION = 1 ;

// Install the confirmed V1 hardware setting once.  Later Config saves are authoritative, so the
// user may change or remove pin_sd_cs without the firmware restoring it on every boot.
static bool migrateSDConfigToPreferences()
{
  uint8_t version = 0 ;
  bool    changed = false ;

  nvsopen() ;
  if ( nvs_get_u8 ( nvshandle, "sd_cfg_ver", &version ) == ESP_OK &&
       version >= SD_CONFIG_VERSION )
  {
    return false ;
  }
  if ( !nvssearch ( "pin_sd_cs" ) )
  {
    nvssetstr ( "pin_sd_cs", String ( "21" ) ) ;        // Shared SPI, dedicated chip-select
    changed = true ;
  }
  nvs_set_u8 ( nvshandle, "sd_cfg_ver", SD_CONFIG_VERSION ) ;
  nvs_commit ( nvshandle ) ;
  ESP_LOGI ( TAG, "SD configuration migrated: shared SPI, CS GPIO21" ) ;
  return changed ;
}
#endif

void scanIR()
{
  char        mykey[20] ;                                   // For numerated key
  String      val ;                                         // Contents of preference entry
  const char* reply ;                                       // Result of analyzeCmd

  if ( ir_value )                                           // Any input?
  {
    sprintf ( mykey, "ir_%04X", ir_value ) ;                // Form key in preferences
    if ( nvssearch ( mykey ) )
    {
      val = nvsgetstr ( mykey ) ;                           // Get the contents
    }

    if ( val.length() )
    {
      String normalizedCommand = val ;                       // Compare IR command reliably
      normalizedCommand.trim() ;                            // Ignore whitespace from preferences
      normalizedCommand.toLowerCase() ;                     // Commands are case-insensitive
      if ( normalizedCommand == "mute" && muteflag )         // Same IR key pressed while muted?
      {
        val = String ( "unmute" ) ;                          // Toggle sound back on
      }
      ESP_LOGI ( TAG, "IR code %04X received. Will execute %s",
                 ir_value, val.c_str() ) ;
      String ircommand = val ;
      ircommand.trim() ;
      int equal = ircommand.indexOf ( '=' ) ;
      String irargument = ircommand ;
      String irparameter = "" ;
      if ( equal >= 0 )
      {
        irargument = ircommand.substring ( 0, equal ) ;
        irparameter = ircommand.substring ( equal + 1 ) ;
      }
      irargument.toLowerCase() ;
      irargument.trim() ;
      irparameter.trim() ;
      if ( irargument == "preset" && irparameter.length() == 1 &&
           isdigit ( irparameter[0] ) )
      {
        #ifdef SDCARD
          const uint8_t digit = irparameter[0] - '0' ;
          if ( SD_playing && !station_list_active )
          {
            int16_t seconds = 0 ;
            if      ( digit == 1 ) seconds = -5 ;
            else if ( digit == 3 ) seconds =  5 ;
            else if ( digit == 4 ) seconds = -10 ;
            else if ( digit == 6 ) seconds =  10 ;
            else if ( digit == 7 ) seconds = -30 ;
            else if ( digit == 9 ) seconds =  30 ;
            if ( seconds )
            {
              if ( SD_cover_visible )
              {
                SD_cover_visible = false ;
                restoreRadioView() ;
              }
              SD_seek_seconds += seconds ;              // sdfuncs performs the safe frame-aligned seek
              reply = seconds > 0 ? "MP3 seek forward" : "MP3 seek backward" ;
            }
            #if defined(BLUETFT)
              else if ( digit == 0 )
              {
                reply = toggleSDCover() ? "MP3 cover shown" : "MP3 cover unavailable/closed" ;
              }
            #endif
            else
            {
              reply = "MP3 digit has no action" ;
            }
          }
          else
        #endif
        {
          enterStationDigit ( irparameter[0] - '0' ) ;   // Numeric station entry outside MP3 mode
          reply = "Preset digit accepted" ;
        }
      }
      else
      {
        reply = analyzeCmd ( val.c_str() ) ;                // Analyze command and handle it
      }
      ESP_LOGI ( TAG, "%s", reply ) ;                       // Result for debugging
    }
    else
    {
      ESP_LOGI ( TAG, "IR code %04X received, but not found in preferences!  Timing %d/%d",
                 ir_value, ir_0, ir_1 ) ;
    }
    ir_value = 0 ;                                          // Reset IR code received
  }
}


#ifndef ETHERNET
//**************************************************************************************************
//                                           M K _ L S A N                                         *
//**************************************************************************************************
// Make a list of all WiFi networks in preferences and FIXEDWIFI.                                  *
// Will be called only once by setup().  Credentials are retained for direct, diagnosed retries.   *
//**************************************************************************************************
void  mk_lsan()
{
  int16_t     i ;                                        // Loop control
  char        key[10] ;                                  // For example: "wifi_03"
  String      buf ;                                      // "SSID/password"
  String      lssid, lpw ;                               // Last read SSID and password from nvs
  int         inx ;                                      // Place of "/"
  WifiInfo_t  winfo ;                                    // Element to store in list

  ESP_LOGI ( TAG, "Create list with acceptable WiFi networks" ) ;
  wifilist.clear() ;
  for ( i = -1 ; i < 100 ; i++ )                         // Examine FIXEDWIFI, wifi_00 .. wifi_99
  {
    buf = String ( "" ) ;                                // Clear buffer with ssid/passwd
    if ( i == -1 )                                       // Examine FIXEDWIFI if defined
    {
      if ( *fixedwifi )                                  // FIXEDWIFI set and not empty?
      {
        buf = String ( fixedwifi ) ;
      }
    }
    else
    {
      sprintf ( key, "wifi_%02d", i ) ;                  // Form key in preferences
      if ( nvssearch ( key  ) )                          // Does it exists?
      {
        buf = nvsgetstr ( key ) ;                        // Get the contents, like "ssid/password"
      }
    }
    inx = buf.indexOf ( "/" ) ;                          // Find separator between ssid and password
    if ( inx > 0 )                                       // Separator found?
    {
      lpw = buf.substring ( inx + 1 ) ;                  // Isolate password
      lssid = buf.substring ( 0, inx ) ;                 // Holds SSID now
      winfo.ssid = lssid ;
      winfo.passphrase = lpw ;
      wifilist.push_back ( winfo ) ;                     // Retain credentials for direct retries
      ESP_LOGI ( TAG, "WiFi candidate %u: '%s'",
                 (unsigned)wifilist.size(), lssid.c_str() ) ;
    }
  }
  ESP_LOGI ( TAG, "%u configured WiFi network(s)", (unsigned)wifilist.size() ) ;
}
#endif

//**************************************************************************************************
//                                     G E T R A D I O S T A T U S                                 *
//**************************************************************************************************
// Return preset-, tone- and volume status.                                                        *
// Included are the presets, the current station, the volume and the tone settings.                *
//**************************************************************************************************
String getradiostatus()
{
  return String ( "preset=" ) +                          // Add preset setting
         String ( presetinfo.host ) +
         String ( "\nvolume=" ) +                        // Add volume setting
         String ( ini_block.reqvol ) +
         String ( "\ntoneha=" ) +                        // Add tone setting HA
         String ( ini_block.rtone[0] ) +
         String ( "\ntonehf=" ) +                        // Add tone setting HF
         String ( ini_block.rtone[1] ) +
         String ( "\ntonela=" ) +                        // Add tone setting LA
         String ( ini_block.rtone[2] ) +
         String ( "\ntonelf=" ) +                        // Add tone setting LF
         String ( ini_block.rtone[3] ) ;
}


//**************************************************************************************************
//                                           T F T L O G                                           *
//**************************************************************************************************
// Log to display.                                                                                 *
//**************************************************************************************************
void tftlog ( const char *str, bool newline )
{
  if ( dsp_ok )                                        // TFT configured?
  {
    dsp_print ( str ) ;                                // Yes, show error on TFT
    if ( newline )
    {
      dsp_print ( "\n" ) ;
    }
    dsp_update ( true ) ;                              // To physical screen
  }
}


//**************************************************************************************************
//                            B U B B L E S O R T K E Y S                                          *
//**************************************************************************************************
// Bubblesort the nvskeys.                                                                         *
//**************************************************************************************************
void bubbleSortKeys ( uint16_t n )
{
  uint16_t i, j ;                                             // Indexes in nvskeys
  char     tmpstr[NVS_KEY_NAME_MAX_SIZE] ;                    // Temp. storage for a key

  for ( i = 0 ; i < n - 1 ; i++ )                             // Examine all keys
  {
    for ( j = 0 ; j < n - i - 1 ; j++ )                       // Compare to next keys
    {
      if ( strcmp ( nvskeys[j], nvskeys[j + 1] ) > 0 )        // Next key out of order?
      {
        strcpy ( tmpstr, nvskeys[j] ) ;                       // Save current key a while
        strcpy ( nvskeys[j], nvskeys[j + 1] ) ;               // Replace current with next key
        strcpy ( nvskeys[j + 1], tmpstr ) ;                   // Replace next with saved current
      }
    }
  }
}


//**************************************************************************************************
//                                      F I L L K E Y L I S T                                      *
//**************************************************************************************************
// File the list of all relevant keys in NVS.                                                      *
// The keys will be sorted.                                                                        *
//**************************************************************************************************
void fillkeylist()
{
  nvs_iterator_t   it ;                                         // Iterator for NVS
  nvs_entry_info_t info ;                                       // Info in entry
  uint16_t         nvsinx = 0 ;                                 // Index in nvskey table

  it = nvs_entry_find ( "nvs", NAME, NVS_TYPE_ANY ) ;           // Get first entry
  while ( it )
  {
    nvs_entry_info ( it, &info ) ;                              // Get info on this entry
    ESP_LOGI ( TAG, "%s::%s type=%d",
               info.namespace_name, info.key, info.type ) ;
    if ( info.type == NVS_TYPE_STR )                            // Only string are used
    {
      strcpy ( nvskeys[nvsinx], info.key ) ;                    // Save key in table
      if ( ++nvsinx == MAXKEYS )
      {
        nvsinx-- ;                                              // Prevent excessive index
      }
    }
    it = nvs_entry_next ( it ) ;
  }
  nvs_release_iterator ( it ) ;                                 // Release resource
  nvskeys[nvsinx][0] = '\0' ;                                   // Empty key at the end
  ESP_LOGI ( TAG, "Read %d keys from NVS", nvsinx ) ;
  bubbleSortKeys ( nvsinx ) ;                                   // Sort the keys
}


//**************************************************************************************************
//                                       H A N D L E D A T A                                       *
//**************************************************************************************************
// Event callback on received data from MP3/AAC host.                                              *
// It is assumed that the first data buffer contains the full header.                              *
// Normally, the data length is 1436 bytes.                                                        *
//**************************************************************************************************
void handleData ( void* arg, AsyncClient* client, void *data, size_t len )
{
  if ( stream_redirect_request ) return ;               // A replacement connection is already queued
  uint8_t* p = (uint8_t*)data ;                         // Treat as an array of bytes

  // ESP_LOGI ( TAG, "Data received, %d bytes", len ) ;
  while ( len-- )
  {
    handlebyte_ch ( *p++ ) ;                            // Handle next byte
    if ( stream_redirect_request ) break ;              // Ignore the redirect response body
  }
}


//**************************************************************************************************
//                                  O N T I M E O U T                                              *
//**************************************************************************************************
// Event callback on p3 host connect time-out.                                                     *
//**************************************************************************************************
void onTimeout ( void* arg, AsyncClient* client, uint32_t t )
{
  ESP_LOGE ( TAG, "MP3 client connect time-out!" ) ;
}


//**************************************************************************************************
//                                           O N E R R O R                                         *
//**************************************************************************************************
// Event callback on MP3 host connect error.                                                       *
//**************************************************************************************************
void onError ( void* arg, AsyncClient* client, err_t a )
{
  ESP_LOGI ( TAG, "MP3 host error %s", client->errorToString ( a ) ) ;
}


//**************************************************************************************************
//                                           O N C O N N E C T                                     *
//**************************************************************************************************
// Event callback on MP3 host connect.                                                             *
//**************************************************************************************************
void onConnect ( void* arg, AsyncClient* client )
{
  ESP_LOGI ( TAG, "Connected to host at %s on port %d",
             client->remoteIP().toString().c_str(),
             client->remotePort() ) ;
}


//**************************************************************************************************
//                                     O N D I S C O N N E C T                                     *
//**************************************************************************************************
// Event callback on MP3 host disconnect.                                                          *
//**************************************************************************************************
void onDisConnect ( void* arg, AsyncClient* client )
{
  ESP_LOGI ( TAG, "Host disconnected" ) ;
}


//**************************************************************************************************
//                                           S E T U P                                             *
//**************************************************************************************************
// Setup for the program.                                                                          *
//**************************************************************************************************
void setup()
{
  int                        i ;                          // Loop control
  int                        pinnr ;                      // Input pinnumber
  const char*                p ;
  byte                       mac[6] ;                     // WiFi mac address
  char                       tmpstr[20] ;                 // For version and Mac address
  esp_partition_iterator_t   pi ;                         // Iterator for find
  const esp_partition_t*     ps ;                         // Pointer to partition struct

  maintask = xTaskGetCurrentTaskHandle() ;                // My taskhandle
  outchunk.datatyp = QDATA ;                              // This chunk dedicated to QDATA
  Serial.begin ( 115200 ) ;                               // For debug
  WRITE_PERI_REG ( RTC_CNTL_BROWN_OUT_REG, 0 ) ;          // Disable brownout detector
  log_printf ( "\n" ) ;
  // Print some memory and sketch info
  log_printf ( "Starting ESP32-radio running on CPU %d at %d MHz.\n",
             xPortGetCoreID(),
             ESP.getCpuFreqMHz() ) ;
  ESP_LOGI ( TAG, "Version %s.  Free memory %d",
             VERSION,
             heapspace ) ;                                // Normally about 100 kB
  ESP_LOGI ( TAG, "BUILD IR-STATIONS-SD-V21-20260914" ) ; // MP3 L/R tracks and larger counter
  ESP_LOGI ( TAG, "Display type is %s", DISPLAYTYPE ) ;   // Report display option
  
  if ( !SPIFFS.begin ( FSIF ) )                           // Mount and test SPIFFS
  {
    ESP_LOGE ( TAG, "SPIFFS Mount Error!" ) ;             // A pity...
  }
  else
  {
    ESP_LOGI ( TAG, "SPIFFS is okay, space %d, used %d",  // Show available SPIFFS space
               SPIFFS.totalBytes(),
               SPIFFS.usedBytes() ) ;
    File index_html = SPIFFS.open ( "/index.html",        // Try to read from SPIFFS file
                                    FILE_READ ) ;
    if ( index_html )                                     // Open success?
    {
      index_html.close() ;                                // Yes, close file
    }
    else
    {
      ESP_LOGE ( TAG, "Web interface incomplete!" ) ;          // No, show warning, upload data to SPIFFS
    }
  }
  pi = esp_partition_find ( ESP_PARTITION_TYPE_DATA,     // Get partition iterator for
                            ESP_PARTITION_SUBTYPE_ANY,   // All data partitions
                            NULL ) ;
  while ( pi )
  {
    ps = esp_partition_get ( pi ) ;                       // Get partition struct
    ESP_LOGI ( TAG, "Found partition '%-8s' "             // Show partition
               "at offset 0x%06X "
               "with size %8d",
               ps->label, ps->address, ps->size ) ;
    if ( strcmp ( ps->label, "nvs" ) == 0 )              // Is this the NVS partition?
    {
      nvs = ps ;                                         // Yes, remember NVS partition
    }
    pi = esp_partition_next ( pi ) ;                     // Find next
  }
  #ifdef FIXEDWIFI                                       // Set fixedwifi if defined
    fixedwifi = FIXEDWIFI ;
  #endif
  if ( nvs == NULL )
  {
    ESP_LOGE ( TAG, "Partition NVS not found!" ) ;       // Very unlikely...
    while ( true ) ;                                     // Impossible to continue
  }
  fillkeylist() ;                                        // Fill keynames with all keys
  if ( migrateIRConfigToPreferences() )                  // One-time move of IR map into Config
  {
    fillkeylist() ;                                      // Include the migrated ir_XXXX keys
  }
  #ifdef SDCARD
    if ( migrateSDConfigToPreferences() )                // Install confirmed shared-SPI CS pin
    {
      fillkeylist() ;                                    // Include pin_sd_cs in Config
    }
  #endif
  memset ( &ini_block, 0, sizeof(ini_block) ) ;          // Init ini_block
  ini_block.mqttport = 1883 ;                            // Default port for MQTT
  ini_block.mqttprefix = "" ;                            // No prefix for MQTT topics seen yet
  ini_block.clk_server = "pool.ntp.org" ;                // Default server for NTP
  ini_block.clk_offset = 1 ;                             // Default Amsterdam time zone
  ini_block.clk_dst = 1 ;                                // DST is +1 hour
  ini_block.bat0 = 2600 ;                                // Battery ADC level for 0 percent
  ini_block.bat100 = 2950 ;                              // Battery ADC level for 100 percent
  readIOprefs() ;                                        // Read pins used for SPI, TFT, VS1053, IR,
                                                         // Rotary encoder
  #ifdef SDCARD
    if ( ini_block.sd_cs_pin >= 0 )                      // Keep shared-bus SD inactive during boot
    {
      pinMode ( ini_block.sd_cs_pin, OUTPUT ) ;
      digitalWrite ( ini_block.sd_cs_pin, HIGH ) ;
    }
  #endif
  for ( i = 0 ; (pinnr = progpin[i].gpio) >= 0 ; i++ )   // Check programmable input pins
  {
    pinMode ( pinnr, INPUT_PULLUP ) ;                    // Input for control button
    vTaskDelay ( 10 / portTICK_PERIOD_MS ) ;
    // Check if pull-up active
    if ( ( progpin[i].cur = digitalRead ( pinnr ) ) == HIGH )
    {
      p = "HIGH" ;
    }
    else
    {
      p = "LOW, probably no PULL-UP" ;                   // No Pull-up
    }
    ESP_LOGI ( TAG, "GPIO%d is %s", pinnr, p ) ;
  }
  readprogbuttons() ;                                    // Program the free input pins
  if ( ini_block.spi_sck_pin >= 0 )
  {
    SPI.begin ( ini_block.spi_sck_pin,                   // Init VSPI bus with default or modified pins
                ini_block.spi_miso_pin,
                ini_block.spi_mosi_pin ) ;
  }
  if ( ini_block.ir_pin >= 0 )
  {
    ESP_LOGI ( TAG, "Enable pin %d for IR",
               ini_block.ir_pin ) ;
    pinMode ( ini_block.ir_pin, INPUT ) ;                // Pin for IR receiver VS1838B
    attachInterrupt ( ini_block.ir_pin,                  // Interrupts will be handle by isr_IR
                      isr_IR, CHANGE ) ;
  }
  ESP_LOGI ( TAG, "Start %s display", DISPLAYTYPE ) ;
  dsp_ok = dsp_begin ( INIPARS ) ;                       // Init display
  if ( dsp_ok )                                          // Init okay?
  {
    dsp_erase() ;                                        // Clear screen
    dsp_setRotation() ;                                  // Usse landscape format
    dsp_setTextSize ( DEFTXTSIZ ) ;                      // Small character font
    dsp_setTextColor ( GREY ) ;                          // Info in grey
    dsp_setCursor ( 0, 0 ) ;                             // Top of screen
    dsp_println ( "Starting......" ) ;
    strncpy ( tmpstr, VERSION, 16 ) ;                    // Limit version length
    dsp_println ( tmpstr ) ;
    dsp_println ( "By Ed Smallenburg" ) ;
    dsp_update ( enc_menu_mode == VOLUME ) ;             // Show on physical screen if needed
  }
  else
  {
    ESP_LOGE ( TAG, "Display not activated" ) ;          // Display init error
  }
  if ( ini_block.tft_bl_pin >= 0 )                       // Backlight for TFT control?
  {
    pinMode ( ini_block.tft_bl_pin, OUTPUT ) ;           // Yes, enable output
  }
  if ( ini_block.tft_blx_pin >= 0 )                      // Backlight for TFT (inversed logic) control?
  {
    pinMode ( ini_block.tft_blx_pin, OUTPUT ) ;          // Yes, enable output
  }
  blset ( true ) ;                                       // Enable backlight (if configured)
  #ifndef ETHERNET
    mk_lsan() ;                                          // Make a list of acceptable networks
                                                         // in preferences.
    WiFi.disconnect() ;                                  // After restart router could still
    vTaskDelay ( 500 / portTICK_PERIOD_MS ) ;            //   keep old connection
    WiFi.mode ( WIFI_STA ) ;                             // This ESP is a station
    // WiFi.setSleep (false ) ;                          // should prevent _poll(): pcb is NULL""error"
    vTaskDelay ( 500 / portTICK_PERIOD_MS ) ;            // ??
    WiFi.persistent ( false ) ;                          // Do not save SSID and password
  #endif
  readprefs ( false ) ;                                  // Read preferences
  radioqueue = xQueueCreate ( 10,                        // Create small queue for communication to radiofuncs
                             sizeof ( qdata_type ) ) ;
  dataqueue = xQueueCreate  ( QSIZ,                      // Create queue for data communication
                             sizeof ( qdata_struct ) ) ;
  p = "Connect to network" ;                             // Show progress
  ESP_LOGI ( TAG, "%s", p ) ;
  tftlog ( p, true ) ;                                   // On TFT too
  #ifdef ETHERNET
    WiFi.onEvent ( EthEvent ) ;                          // Set actions on ETH events
    NetworkFound = connectETH() ;                        // Connect to Ethernet
  #else
    NetworkFound = connectwifi() ;                       // Connect to WiFi network
  #endif
  tcpip_adapter_set_hostname ( TCPIP_ADAPTER_IF_STA,
                               NAME ) ;
  ESP_LOGI ( TAG, "Start web server" ) ;
  cmdserver.on ( "/getprefs",  handle_getprefs ) ;       // Handle get preferences
  cmdserver.on ( "/saveprefs", handle_saveprefs ) ;      // Handle save preferences
  cmdserver.on ( "/getdefs",   handle_getdefs ) ;        // Handle get default config
  cmdserver.on ( "/settings",  handle_settings ) ;       // Handle strings like presets/volume,...
  cmdserver.on ( "/mp3list",   handle_mp3list ) ;        // Handle request for list of tracks
  cmdserver.on ( "/reset",     handle_reset ) ;          // Handle reset command
  cmdserver.onNotFound ( handle_notfound ) ;             // For handling a simple page/file and parameters
  cmdserver.begin() ;                                    // Start http server
  if ( NetworkFound )                                    // OTA and MQTT only if Wifi network found
  {
    ESP_LOGI ( TAG, "Network found. Starting clients" ) ;
    mp3client = new AsyncClient ;                        // Create client for Shoutcast connection
    mp3client->onData ( &handleData ) ;                  // Set callback on received mp3 data
    mp3client->onConnect ( &onConnect ) ;                // Set callback on connect
    mp3client->onDisconnect ( &onDisConnect ) ;          // Set callback on disconnect
    mp3client->onError ( &onError ) ;                    // Set callback on error
    mp3client->onTimeout ( &onTimeout ) ;                // Set callback on time-out
    mqtt_on = ( ini_block.mqttbroker.length() > 0 ) &&   // Use MQTT if broker specified
              ( ini_block.mqttbroker != "none" ) ;
    #ifdef ENABLEOTA
      ArduinoOTA.setHostname ( NAME ) ;                  // Set the hostname
      ArduinoOTA.onStart ( otastart ) ;
      ArduinoOTA.onError ( otaerror ) ;
      ArduinoOTA.begin() ;                               // Allow update over the air
    #endif
    if ( mqtt_on )                                       // Broker specified?
    {
      if ( ( ini_block.mqttprefix.length() == 0 ) ||     // No prefix?
           ( ini_block.mqttprefix == "none" ) )
      {
        WiFi.macAddress ( mac ) ;                        // Get mac-adress
        sprintf ( tmpstr, "P%02X%02X%02X%02X",           // Generate string from last part
                  mac[3], mac[2],
                  mac[1], mac[0] ) ;
        ini_block.mqttprefix = String ( tmpstr ) ;       // Save for further use
      }
      ESP_LOGI ( TAG, "MQTT uses prefix %s", ini_block.mqttprefix.c_str() ) ;
      ESP_LOGI ( TAG, "Init MQTT" ) ;
      mqttclient.setServer(ini_block.mqttbroker.c_str(), // Specify the broker
                           ini_block.mqttport ) ;        // And the port
      mqttclient.setCallback ( onMqttMessage ) ;         // Set callback on receive
    }
    if ( MDNS.begin ( NAME ) )                           // Start MDNS transponder
    {
      ESP_LOGI ( TAG, "MDNS responder started" ) ;
    }
    else
    {
      ESP_LOGE ( TAG, "Error setting up MDNS responder!" ) ;
    }
  }
  timer = timerBegin ( 0, 80, true ) ;                   // User 1st timer with prescaler 80
  timerAttachInterrupt ( timer, &timer100, false ) ;     // Call timer100() on timer alarm
  timerAlarmWrite ( timer, 100000, true ) ;              // Alarm every 100 msec
  timerAlarmEnable ( timer ) ;                           // Enable the timer
  vTaskDelay ( 1000 / portTICK_PERIOD_MS ) ;             // Show IP for a while
  configTime ( ini_block.clk_offset * 3600,
               ini_block.clk_dst * 3600,
               ini_block.clk_server.c_str() ) ;          // GMT offset, daylight offset in seconds
  timeinfo.tm_year = 0 ;                                 // Set TOD to illegal
  // Init settings for rotary switch (if existing).
  #ifdef ZIPPYB5
    if ( ( ini_block.enc_up_pin + ini_block.enc_dwn_pin + ini_block.enc_sw_pin ) > 2 )
    {
      attachInterrupt ( ini_block.enc_up_pin,  isr_enc_turn,   CHANGE ) ;
      attachInterrupt ( ini_block.enc_dwn_pin, isr_enc_turn,   CHANGE ) ;
      attachInterrupt ( ini_block.enc_sw_pin,  isr_enc_switch, CHANGE ) ;
      ESP_LOGI ( TAG, "ZIPPY side switch is enabled" ) ;
    }
    else
    {
      ESP_LOGI ( TAG, "ZIPPY side switch is disabled (%d/%d/%d)",
                ini_block.enc_up_pin,
                ini_block.enc_dwn_pin,
                ini_block.enc_sw_pin ) ;
    }
  #else
    if ( ( ini_block.enc_clk_pin + ini_block.enc_dt_pin + ini_block.enc_sw_pin ) > 2 )
    {
      attachInterrupt ( ini_block.enc_clk_pin, isr_enc_turn,   CHANGE ) ;
      attachInterrupt ( ini_block.enc_dt_pin,  isr_enc_turn,   CHANGE ) ;
      attachInterrupt ( ini_block.enc_sw_pin,  isr_enc_switch, CHANGE ) ;
      ESP_LOGI ( TAG, "Rotary encoder is enabled" ) ;
    }
    else
    {
      ESP_LOGI ( TAG, "Rotary encoder is disabled (%d/%d/%d)",
                ini_block.enc_clk_pin,
                ini_block.enc_dt_pin,
                ini_block.enc_sw_pin ) ;
    }
  #endif
  if ( NetworkFound )
  {
    gettime() ;                                           // Sync time
  }
  adc1_config_width ( ADC_WIDTH_12Bit ) ;
  adc1_config_channel_atten ( ADC1_CHANNEL_0, ADC_ATTEN_DB_12 ) ;  // VP/GPIO36 (ESP32), GPIO1 (ESP32-S3)
  xTaskCreatePinnedToCore (
    playtask,                                             // Task to play data in dataqueue.
    "Playtask",                                           // Name of task.
    2100,                                                 // Stack size of task
    NULL,                                                 // parameter of the task
    2,                                                    // priority of the task
    &xplaytask,                                           // Task handle to keep track of created task
    0 ) ;                                                 // Run on CPU 0
  #if defined(DEC_VS1053) || defined(DEC_VS1003)
    for ( uint8_t wait = 0 ; !player_init_done && wait < 30 ; wait++ )
    {
      vTaskDelay ( 100 / portTICK_PERIOD_MS ) ;           // Never start SD/radio during decoder test
    }
    if ( !player_ready )
    {
      ESP_LOGE ( TAG, "VS1003 initialization failed or timed out" ) ;
    }
  #else
    vTaskDelay ( 100 / portTICK_PERIOD_MS ) ;             // Allow software decoder task to start
  #endif
#ifdef SDCARD
  sdqueue = xQueueCreate ( 10,                            // Create small queue for communication to sdfuncs
                           sizeof ( qdata_type ) ) ;
  xTaskCreatePinnedToCore (
    SDtask,                                               // Task to get filenames from SD card
    "SDtask",                                             // Name of task.
    4000,                                                 // Stack size of task
    NULL,                                                 // parameter of the task
    2,                                                    // priority of the task
    &xsdtask,                                             // Task handle to keep track of created task
    0 ) ;                                                 // Run on CPU 0
#endif
  singleclick = false ;                                   // Might be fantom click
  if ( dsp_ok )                                           // Is display okay?
  {
    vTaskDelay ( 2000 / portTICK_PERIOD_MS ) ;            // Yes, allow user to read display text
    dsp_erase() ;                                         // Clear screen
  }
  tftset ( 0, NAME ) ;                                    // Set screen segment text top line
  presetinfo.station_state = ST_PRESET ;                  // Start in preset mode
  if ( nextPreset ( nvsgetstr ( "preset" ).toInt(),       // Restore last preset
       false  ) )
  {
    if ( NetworkFound )                                     // Start with preset if network available
    {
      myQueueSend ( radioqueue, &startcmd ) ;               // Start player in radio mode
    }
  }
}


//**************************************************************************************************
//                                        W R I T E P R E F S                                      *
//**************************************************************************************************
// Update the preferences.  Called from the web interface.                                         *
// Parameter is a string with multiple HTTP key/value pairs.                                       *
//**************************************************************************************************
void writeprefs ( AsyncWebServerRequest *request )
{
  int        numargs ;                            // Number of arguments
  int        i ;                                  // Index in arguments
  String     key ;                                // Name of parameter i
  String     contents ;                           // Value of parameter i

  //timerAlarmDisable ( timer ) ;                 // Disable the timer
  nvsclear() ;                                    // Remove all preferences
  numargs = request->params() ;                   // Haal aantal parameters
  ESP_LOGI ( TAG, "writeprefs numargs is %d",
             numargs ) ;
  for ( i = 0 ; i < numargs ; i++ )               // Scan de parameters
  {
    key = request->argName ( i ) ;                // Get name (key)
    contents = request->arg ( i ) ;               // Get value
    chomp ( key ) ;                               // Remove leading/trailing spaces
    chomp ( contents ) ;                          // Remove leading/trailing spaces
    contents.replace ( "($)", "#" ) ;             // Replace comment separator
    if ( key.isEmpty() || contents.isEmpty() )    // Skip empty keys (comment line)
    {
      continue ;
    }
    if ( key == "version" )                       // Skip de "version" parameter
    {
      continue ;
    }
    ESP_LOGI ( TAG, "Handle POST %s = %s",
               key.c_str(), contents.c_str() ) ;  // Toon POST parameter
    nvssetstr ( key.c_str(), contents ) ;         // Save new pair
  }
  // A saved Config is authoritative, including intentionally removed IR mappings.
  nvs_set_u8 ( nvshandle, "ir_cfg_ver", IR_CONFIG_VERSION ) ;
  #ifdef SDCARD
    nvs_set_u8 ( nvshandle, "sd_cfg_ver", SD_CONFIG_VERSION ) ;
  #endif
  nvs_commit( nvshandle ) ;
  //timerAlarmEnable ( timer ) ;                  // Enable the timer
  fillkeylist() ;                                 // Update list with keys
}


#ifdef SDCARD
//**************************************************************************************************
//                                        C B  _ M P 3 L I S T                                     *
//**************************************************************************************************
// Callback function for handle_mp3list, will be called for every chunk to send to client.         *
// If no more data is available, this function will return 0.                                      *
//**************************************************************************************************
size_t cb_mp3list ( uint8_t *buffer, size_t maxLen, size_t index )
{
  static int         i ;                              // Index in track list
  static const char* path ;                           // Pointer in file path
  size_t             len = 0 ;                        // Number of bytes filled in buffer
  char*              p = (char*)buffer ;              // Treat as pointer to aray of char
  static bool        eolSeen ;                        // Remember if End Of List

  if ( index == 0 )                                   // First call for this page?
  {
    i = 1 ;                                           // Yes, set index (track number)
    path = getFirstSDFileName() ;                     // Force read of next path
    eolSeen = ( path == nullptr ) ;                   // Any file?
  }
  while ( ( maxLen > len ) && ( ! eolSeen ) )         // Space for another char from path?
  {
    if ( *path )                                      // End of path?
    {
      *p++ = *path++ ;                                // No, add another character to send buffer
      len++ ;                                         // Update total length
    }
    else
    {
      // End of path
      if ( i )                                        // At least one path in output?
      {
        *p++ = '\n' ;                                 // Yes, add separator
        len++ ;                                       // Update total length
      }
      path = getSDFileName ( i++ ) ;                  // Get next path from list
      if ( i > SD_filecount )                         // No more files?
      {
        eolSeen = true ;                              // Yes, stop
        break ;
      }
    }
  }
  // We arrive here if output buffer is completely full or end of tracklist is reached
  return len ;                                        // Return filled length of buffer
}


//**************************************************************************************************
//                                    H A N D L E _ M P L I S T                                    *
//**************************************************************************************************
// Called from mp3play page to list all the MP3 tracks.                                            *
// It will handle the chunks for the client.  The buffer is filled by the callback routine.        *
//**************************************************************************************************
void handle_mp3list ( AsyncWebServerRequest *request )
{
  AsyncWebServerResponse *response ;

  response = request->beginChunkedResponse ( "text/plain", cb_mp3list ) ;
  response->addHeader ( "Server", NAME ) ;
  request->send ( response ) ;
}
#else
//**************************************************************************************************
//                                    H A N D L E _ M P L I S T                                    *
//**************************************************************************************************
// Dummy version.                                                                                  *
//**************************************************************************************************
void handle_mp3list ( AsyncWebServerRequest *request )
{
  request->send ( 200, "text/plain", "<empty>" ) ;
}
#endif


//**************************************************************************************************
//                                    H A N D L E _ G E T P R E F S                                *
//**************************************************************************************************
// Called from config page to display configuration data.                                          *
//**************************************************************************************************
void handle_getprefs ( AsyncWebServerRequest *request )
{
  String prefs ;
  
  //if ( datamode != STOPPED )                         // Still playing?
  //{
  //  setdatamode (  STOPREQD ) ;                      // Stop playing
  //}
  prefs = readprefs ( true ) ;                         // Read preference values
  request->send ( 200, "text/plain; charset=utf-8", prefs ) ; // Preserve Unicode in browser
}


//**************************************************************************************************
//                                    H A N D L E _ S A V E P R E F S                              *
//**************************************************************************************************
// Called from config page to save configuration data.                                             *
//**************************************************************************************************
void handle_saveprefs ( AsyncWebServerRequest *request )
{
  const char* reply = "Config saved" ;                 // Default reply

  writeprefs ( request ) ;                             // Write to NVS
  request->send ( 200, "text/plain; charset=utf-8", reply ) ; // UTF-8 configuration response
}


//**************************************************************************************************
//                                    H A N D L E _ G E T D E F S                                  *
//**************************************************************************************************
// Called from config page to load default configuration.                                          *
//**************************************************************************************************
void handle_getdefs ( AsyncWebServerRequest *request )
{
  String ct ;                                         // Content type
  const char* path ;                                  // File with default settings

  path = "/defaultprefs.txt" ;                        // Set file name
  if ( SPIFFS.exists ( path ) )                       // Does it exist in SPIFFS?
  {
    ct = "text/plain; charset=utf-8" ;               // Default preferences contain Unicode
    request->send ( SPIFFS, path, ct ) ;              // Send to client
  }
  else
  {
    request->send ( 200, "text/plain",                // No send empty preferences
                    "<empty>" ) ;
  }
}


//**************************************************************************************************
//                                    H A N D L E _ S E T T I N G S                                *
//**************************************************************************************************
// Called from index page to load settings like presets, volume. tone....                          *
//**************************************************************************************************
void handle_settings ( AsyncWebServerRequest *request )
{
  String              val = String() ;                   // Result to send
  String              statstr ;                          // Station string
  String              hsym ;                             // Symbolic station name from comment part
  int16_t             i ;                                // Loop control, preset number

  for ( i = 0 ; i < MAXPRESETS ; i++ )                   // Max number of presets
  {
    readhostfrompref ( i, &statstr, &hsym ) ;            // Get the preset from NVS
    if ( statstr != "" )                                 // Preset available?
    {
      // Show just comment if available.  Otherwise the preset itself.
      if ( hsym != "" )                                  // hsym set?
      {
        statstr = hsym ;                                 // Yes, use it
      }
      chomp ( statstr ) ;                                // Remove garbage from description
      //ESP_LOGI ( TAG, "statstr is %s", statstr.c_str() ) ;
      val += String ( "preset_" ) +
             String ( i ) +
             String ( "=" ) +
             statstr +
             String ( "\n" ) ;                           // Add delimeter
    }
  }
  #ifdef DEC_HELIX
    val += String ( "decoder=helix\n" ) ;                // Add decoder type for helix (no volume buttons)
  #endif
  val += getradiostatus() +                              // Add radio setting
         String ( "\n\n" ) ;                             // End of reply
  request->send ( 200, "text/plain", val ) ;             // Send preferences
}


//**************************************************************************************************
//                                  H A N D L E S A V E R E Q                                      *
//**************************************************************************************************
// Handle save volume/preset/tone.  This will save current settings every 10 minutes to            *
// the preferences.  On the next restart these values will be loaded.                              *
// Note that saving prefences will only take place if contents has changed.                        *
//**************************************************************************************************
void handleSaveReq()
{
  static uint32_t savetime = 0 ;                          // Limit save to once per 10 minutes

  if ( ( millis() - savetime ) < 600000 )                 // 600 sec is 10 minutes
  {
    return ;
  }
  savetime = millis() ;                                   // Set time of last save
  nvssetstr ( "preset", String ( presetinfo.preset  ) ) ; // Save current preset
  nvssetstr ( "volume", String ( ini_block.reqvol   ) ) ; // Save current volue
  nvssetstr ( "toneha", String ( ini_block.rtone[0] ) ) ; // Save current toneha
  nvssetstr ( "tonehf", String ( ini_block.rtone[1] ) ) ; // Save current tonehf
  nvssetstr ( "tonela", String ( ini_block.rtone[2] ) ) ; // Save current tonela
  nvssetstr ( "tonelf", String ( ini_block.rtone[3] ) ) ; // Save current tonelf
}


//**************************************************************************************************
//                                    H A N D L E _ R E S E T                                      *
//**************************************************************************************************
// Called from config page to reset the radio.                                                     *
//**************************************************************************************************
void handle_reset ( AsyncWebServerRequest *request )
{
  request->send ( 200, "text/plain", "Command accepted"  ) ;         // Send the reply
  resetreq = true ;                                                  // Set the reset request
}


//**************************************************************************************************
//                                      H A N D L E I P P U B                                      *
//**************************************************************************************************
// Handle publish op IP to MQTT.  This will happen every 10 minutes.                               *
//**************************************************************************************************
void handleIpPub()
{
  static uint32_t pubtime = 300000 ;                       // Limit save to once per 10 minutes

  if ( ( millis() - pubtime ) < 600000 )                   // 600 sec is 10 minutes
  {
    return ;
  }
  pubtime = millis() ;                                     // Set time of last publish
  mqttpub.trigger ( MQTT_IP ) ;                            // Request re-publish IP
}


//**************************************************************************************************
//                                      H A N D L E V O L P U B                                    *
//**************************************************************************************************
// Handle publish of Volume to MQTT.  This will happen max every 10 seconds.                       *
//**************************************************************************************************
void handleVolPub()
{
  static uint32_t pubtime = 10000 ;                        // Limit save to once per 10 seconds
  static uint8_t  oldvol = -1 ;                            // For comparison

  if ( ( millis() - pubtime ) < 10000 )                    // 10 seconds
  {
    return ;
  }
  pubtime = millis() ;                                     // Set time of last publish
  if ( ini_block.reqvol != oldvol )                        // Volume change?
  {
    mqttpub.trigger ( MQTT_VOLUME ) ;                      // Request publish VOLUME
    oldvol = ini_block.reqvol ;                            // Remember publishe volume
  }
}


//**************************************************************************************************
//                                           C H K _ E N C                                         *
//**************************************************************************************************
// See if rotary encoder is activated and perform its functions.                                   *
//**************************************************************************************************
void chk_enc()
{
  static int16_t enc_preset ;                                 // Selected preset
  String         tmp, tmp2 ;                                  // Temporary strings

  if ( enc_menu_mode != VOLUME )                              // In default mode?
  {
    if ( enc_inactivity > 50 )                                // No, more than 5 seconds inactive
    {
      enc_inactivity = 0 ;
      enc_menu_mode = VOLUME ;                                // Return to VOLUME mode
      ESP_LOGI ( TAG, "Encoder mode back to VOLUME" ) ;
    }
  }
  if ( singleclick || doubleclick ||                          // Any activity?
       tripleclick || longclick ||
       ( rotationcount != 0 ) )
  {
    blset ( true ) ;                                          // Yes, activate display if needed
  }
  else
  {
    return ;                                                  // No, nothing to do
  }
  if ( station_list_active )                                  // Encoder controls open list
  {
    if ( singleclick || doubleclick )
    {
      singleclick = false ;
      doubleclick = false ;
      confirmStationList() ;
    }
    else if ( rotationcount )
    {
      int16_t movement = rotationcount ;
      rotationcount = 0 ;
      moveStationList ( movement > 0 ? 1 : -1,
                        movement > 0 ? movement : -movement ) ;
    }
    tripleclick = false ;
    longclick = false ;
    return ;
  }
  if ( tripleclick )                                          // First handle triple click
  {
    ESP_LOGI ( TAG, "Triple click" ) ;
    tripleclick = false ;                                     // Reset flag
    #ifdef SDCARD
      if ( SD_filecount )                                     // Tracks on SD?
      {
        enc_menu_mode = TRACK ;                               // Swich to TRACK mode
        ESP_LOGI ( TAG, "Encoder mode set to TRACK" ) ;
        tftset ( 3, "Turn to select track\n"                  // Show current option
                    "Press to confirm" ) ;
        getSDFileName ( +1 ) ;                                // Start with next file on SD
      }
      else
      {
        ESP_LOGI ( TAG, "No tracks on SD" ) ;
      }
    #endif
  }
  if ( doubleclick )                                          // Handle the doubleclick
  {
    ESP_LOGI ( TAG, "Double click") ;
    doubleclick = false ;
    enc_menu_mode = VOLUME ;
    openStationList() ;                                       // Compatibility: double click also opens list
    return ;
  }
  if ( singleclick )
  {
    ESP_LOGI ( TAG, "Single click" ) ;
    singleclick = false ;
    switch ( enc_menu_mode )                                  // Which mode (VOLUME, PRESET)?
    {
      case VOLUME :
        openStationList() ;                                   // Middle/OK opens station list
        break ;
      case PRESET :
        if ( nextPreset ( enc_preset ) )                      // Make a definite choice
        {
          enc_menu_mode = VOLUME ;                            // Back to default mode
          myQueueSend ( radioqueue, &startcmd ) ;             // Signal radiofuncs()
          tftset ( 2, icyname ) ;                             // Restore screen segment bottom part
        }
        break ;
    #ifdef SDCARD
      case TRACK :
        myQueueSend ( sdqueue, &startcmd ) ;                  // Signal SDfuncs()
        enc_menu_mode = VOLUME ;                              // Back to default mode
        tftset ( 2, icyname ) ;                               // Restore screen segment bottom part
        break ;
    #endif
      default :
        break ;
    }
  }
  if ( longclick )                                            // Check for long click
  {
    ESP_LOGI ( TAG, "Long click") ;
    myQueueSend ( sdqueue, &stopcmd ) ;                       // Stop player
    myQueueSend ( radioqueue, &stopcmd ) ;                    // Stop player
    //if ( datamode != STOPPED )
    //{
    //  setdatamode ( STOPREQD ) ;                            // Request STOP, do not touch longclick flag
    //}
    //else
    //{
    longclick = false ;                                       // Reset condition
    #ifdef SDCARD
      if ( SD_filecount )
      {
        getSDFileName ( -1 ) ;                                // Random choice
        myQueueSend ( sdqueue, &startcmd ) ;                  // Start random track
      }
    #endif
  }
  if ( rotationcount == 0 )                                   // Any rotation?
  {
    return ;                                                  // No, return
  }
  //ESP_LOGI ( TAG, "Rotation count %d", rotationcount ) ;
  switch ( enc_menu_mode )                                    // Which mode (VOLUME, PRESET, TRACK)?
  {
    case VOLUME :
      if ( ! muteflag )                                       // Do not handle if muted
      {
        rotationcount *= 4 ;                                  // Step by 4 percent
        if ( ( ini_block.reqvol + rotationcount ) < 0 )       // Limit volume
        {
          ini_block.reqvol = 0 ;                              // Limit to normal values
        }
        else if ( ( ini_block.reqvol + rotationcount ) > 100 )
        {
          ini_block.reqvol = 100 ;                            // Limit to normal values
        }
        else
        {
          ini_block.reqvol += rotationcount ;
        }
      }
      break ;
    case PRESET :
      if ( ( enc_preset + rotationcount ) < 0 )               // Negative not allowed
      {
        enc_preset = 0 ;                                      // Stay at 0
      }
      else
      {
        enc_preset += rotationcount ;                         // Next preset
      }
      readhostfrompref ( enc_preset, &tmp, &tmp2 ) ;          // Get host spec and possible comment
      if ( tmp == "" )                                        // End of presets?
      {
        enc_preset = 0 ;                                      // Yes, wrap
        readhostfrompref ( enc_preset, &tmp, &tmp2 ) ;        // Get host spec and possible comment
      }
      ESP_LOGI ( TAG, "Preset is %d", enc_preset ) ;
      // Show just comment if available.  Otherwise the preset itself.
      if ( tmp2 != "" )                                       // Symbolic name present?
      {
        tmp = tmp2 ;                                          // Yes, use it
      }
      chomp ( tmp ) ;                                         // Remove garbage from description
      tftset ( 3, tmp ) ;                                     // Set screen segment bottom part
      break ;
#ifdef SDCARD
    case TRACK :
      if ( rotationcount > 0 )
      {
        getSDFileName ( SD_curindex + rotationcount ) ;      // Select next file on SD
        ESP_LOGI ( TAG, "Select track %s",                   // Show for debug
                    getCurrentSDFileName() ) ;
        tftset ( 3, getCurrentShortSDFileName() ) ;          // Set screen segment bottom part
      }
      break ;
#endif
    default :
      break ;
  }
  rotationcount = 0 ;                                         // Reset
}


//**************************************************************************************************
//                              D I S P L A Y   S D   P R O G R E S S                              *
//**************************************************************************************************
// Update the played/length counter and the divider between artist and title once per second.      *
//**************************************************************************************************
static void displaySDProgress()
{
  #if defined(SDCARD) && defined(BLUETFT)
    if ( !SD_playing || SD_cover_visible || station_list_active || !dsp_ok ) return ;
    uint32_t playedSeconds, lengthSeconds ;
    uint8_t percent ;
    getSDProgress ( playedSeconds, lengthSeconds, percent ) ;
    String progressText = getSDProgressText() ;
    displayplaytime ( progressText.c_str() ) ;
    const int16_t y = tftdata[2].y - 2 ;
    const int16_t filled = ( dsp_getwidth() * percent ) / 100 ;
    dsp_fillRect ( 0, y, filled, 2, GREEN ) ;
    dsp_fillRect ( filled, y, dsp_getwidth() - filled, 2, GREY ) ;
  #endif
}


#if defined(SDCARD) && defined(BLUETFT)
// TJpg_Decoder callback for the Adafruit_GFX-compatible ST7735 driver.
static bool drawSDCoverBlock ( int16_t x, int16_t y, uint16_t width,
                               uint16_t height, uint16_t* pixels )
{
  if ( !bluetft_tft || y >= dsp_getheight() ) return false ;
  bluetft_tft->drawRGBBitmap ( x, y, pixels, width, height ) ;
  return true ;
}


// Toggle a JPEG stored in the current track's ID3 APIC frame.  A separate File handle preserves
// the playback position; the decoder stops at JPEG EOI, so trailing MP3 audio is never rendered.
static bool toggleSDCover()
{
  if ( !SD_playing || !dsp_ok ) return false ;
  if ( SD_cover_visible )
  {
    SD_cover_visible = false ;
    restoreRadioView() ;
    return false ;
  }
  if ( !SD_cover_offset || !SD_cover_length )
  {
    ESP_LOGI ( TAG, "Current MP3 has no supported embedded JPEG cover" ) ;
    return false ;
  }

  String path = String ( getCurrentSDFileName() ) ;
  uint16_t imageWidth = 0, imageHeight = 0 ;
  File coverInfo = SD.open ( path, FILE_READ ) ;
  if ( !coverInfo || !coverInfo.seek ( SD_cover_offset ) ||
       TJpgDec.getFsJpgSize ( &imageWidth, &imageHeight, coverInfo ) != JDR_OK )
  {
    if ( coverInfo ) coverInfo.close() ;
    ESP_LOGI ( TAG, "Cannot read embedded JPEG cover" ) ;
    return false ;
  }
  if ( coverInfo ) coverInfo.close() ;

  uint8_t scale = 1 ;
  while ( scale < 8 &&
          ( ( imageWidth + scale - 1 ) / scale > dsp_getwidth() ||
            ( imageHeight + scale - 1 ) / scale > dsp_getheight() ) ) scale *= 2 ;
  uint16_t shownWidth = ( imageWidth + scale - 1 ) / scale ;
  uint16_t shownHeight = ( imageHeight + scale - 1 ) / scale ;
  int16_t x = shownWidth < dsp_getwidth() ? ( dsp_getwidth() - shownWidth ) / 2 : 0 ;
  int16_t y = shownHeight < dsp_getheight() ? ( dsp_getheight() - shownHeight ) / 2 : 0 ;

  File cover = SD.open ( path, FILE_READ ) ;
  if ( !cover || !cover.seek ( SD_cover_offset ) )
  {
    if ( cover ) cover.close() ;
    return false ;
  }
  dsp_erase() ;
  TJpgDec.setJpgScale ( scale ) ;
  TJpgDec.setCallback ( drawSDCoverBlock ) ;
  JRESULT result = TJpgDec.drawFsJpg ( x, y, cover ) ;
  if ( cover ) cover.close() ;
  SD_cover_visible = result == JDR_OK ;
  if ( !SD_cover_visible ) restoreRadioView() ;
  ESP_LOGI ( TAG, "Embedded cover render result %d, %ux%u, scale %u",
             result, imageWidth, imageHeight, scale ) ;
  return SD_cover_visible ;
}
#endif

//**************************************************************************************************
//                                     S P F U N C S                                               *
//**************************************************************************************************
// Handles display of text, time and volume on TFT.                                                *
// Handles ADC meassurements.                                                                      *
//**************************************************************************************************
void spfuncs()
{
  if ( spftrigger )                                             // Will be set every 100 msec
  {
    spftrigger = false ;                                        // Reset trigger
    if ( station_number_entry &&
         ( millis() - station_number_time >= 1200 ) )           // Numeric-entry timeout?
    {
      confirmStationNumber() ;                                  // Validate and switch if it exists
    }
    if ( dsp_ok )                                               // Posible to update TFT?
    {
      if ( station_list_active )                                // Animate selected long list title
      {
        if ( station_list_scroll_needed &&
             millis() - station_list_scroll_time >= 300 )
        {
          station_list_scroll_time = millis() ;
          station_list_scroll++ ;
          drawStationList ( true ) ;                            // Redraw only the selected row
        }
      }
      #ifdef SDCARD
        else if ( SD_cover_visible )                            // Keep cover intact while audio continues
        {
          // Pending text updates remain queued and are painted after key 0/menu/track change.
        }
      #endif
      else                                                      // Normal radio screen
      {
        for ( uint16_t i = 0 ; i < TFTSECS ; i++ )              // Yes, handle all sections
        {
          if ( tftdata[i].update_req )                          // Refresh requested?
          {
            displayinfo ( i ) ;                                 // Yes, do the refresh
            dsp_update ( enc_menu_mode == VOLUME ) ;            // Updates to the screen
            tftdata[i].update_req = false ;                     // Reset request
            break ;                                             // Just handle 1 request
          }
        }
        dsp_update ( enc_menu_mode == VOLUME ) ;                // Be sure to paint physical screen
      }
    }
    if ( muteflag )                                             // Mute or not?
    {
      player_setVolume ( 0 ) ;                                  // Mute
    }
    else
    {
      player_setVolume ( ini_block.reqvol ) ;                   // Unmute
    }
    if ( reqtone )                                              // Request to change tone?
    {
      reqtone = false ;
      player_setTone ( ini_block.rtone ) ;                      // Set SCI_BASS to requested value
    }
    if ( time_req )                                             // Time to refresh timetxt?
    {
      if ( NetworkFound )                                       // Yes, time available?
      {
        gettime() ;                                             // Yes, get the current time
      }
      time_req = false ;                                        // Yes, clear request
      if ( !station_list_active
           #ifdef SDCARD
             && !SD_cover_visible
           #endif
         )
      {
        displaytime ( timetxt ) ;                               // Write to TFT screen
        displayvolume ( player_getVolume() ) ;                  // Show volume on display
        displaybattery ( ini_block.bat0, ini_block.bat100,      // Show battery charge on display
                         adcval ) ;
        displaySDProgress() ;                                   // SD counter and progress divider
      }
    }
    if ( mqtt_on )
    {
      if ( !mqttclient.connected() )                            // See if connected
      {
        mqttreconnect() ;                                       // No, reconnect
      }
      else
      {
        mqttpub.publishtopic() ;                                // Check if any publishing to do
      }
    }
    adcvalraw = adc1_get_raw ( ADC1_CHANNEL_0 ) ;
    adcval = ( 15 * adcval +                                    // Read ADC and do some filtering
               adcvalraw ) / 16 ;
  }
}


//**************************************************************************************************
//                                     R A D I O F U N C S                                         *
//**************************************************************************************************
// Handles commands for the connection to a icecast server.                                        *
// Commands are received in the input queue.                                                       *
// Data from the server is handle by the handleData() function.                                    *
//**************************************************************************************************
void radiofuncs()
{
  qdata_type   radiocmd ;                                         // Command from radioqueue
  static bool  connected = false ;                                // Connected to host or not

  if ( xQueueReceive ( radioqueue, &radiocmd, 0 ) )               // New command in queue?
  {
    ESP_LOGI ( TAG, "Radiofuncs cmd is %d", radiocmd ) ;
    switch ( radiocmd )                                           // Yes, examine command
    {
      case QSTARTSONG:                                            // Start a new station?
        if ( sdqueue )                                            // SD card configured?
        {
          myQueueSend ( sdqueue, &stopcmd ) ;                     // Yes, send STOP to SD queue (First Out)
          sdfuncs() ;                                             // Allow sdfuncs to react
        }
        connected = connecttohost() ;                             // Connect to stream host
        mqttpub.trigger ( MQTT_PRESET ) ;                         // Request publishing to MQTT
        break ;
      case QSTOPSONG:                                             // Stop playing?
        if ( connected )                                          // Yes, are we stiil playing?
        {
          stop_mp3client() ;                                      // Yes, stop input stream
          connected = false ;                                     // Remember connection state
        }
      default:
        break ;
    }
  }
}


//**************************************************************************************************
//                                           L O O P                                               *
//**************************************************************************************************
// Main loop of the program.                                                                       *
//**************************************************************************************************
void loop()
{
  if ( resetreq )                                   // Reset requested?
  {
    vTaskDelay ( 1000 / portTICK_PERIOD_MS ) ;      // Yes, wait some time
    timerDetachInterrupt ( timer ) ;
    timerEnd ( timer ) ;
    ESP.restart() ;                                 // Reboot
  }
  if ( sleepreq )                                   // Request for deep sleep?
  {
    if ( dsp_ok )                                   // TFT configured?
    {
      dsp_erase() ;                                 // Yes, clear screen
      dsp_update ( true ) ;                         // To physical screen
    }
    esp_deep_sleep_start() ;                        // Yes, sleep until reset
    // No return here...
  }
  scanserial() ;                                    // Handle serial input
  scanserial2() ;                                   // Handle serial input from NEXTION (if active)
  scandigital() ;                                   // Scan digital inputs
  scanIR() ;                                        // See if IR input
  #ifdef ENABLEOTA
    ArduinoOTA.handle() ;                           // Check for OTA
  #endif
  if ( mqtt_on )                                    // Need to handle MQTT?
  {
    mqttclient.loop() ;                             // Handling of MQTT connection
  }
  handleSaveReq() ;                                 // See if time to save settings
  handleIpPub() ;                                   // See if time to publish IP
  handleVolPub() ;                                  // See if time to publish volume
  chk_enc() ;                                       // Check rotary encoder functions
  pump_secure_stream() ;                            // Feed HTTPS data into the common stream parser
  radiofuncs() ;                                    // Handle start/stop commands for icecast
  spfuncs() ;                                       // Handle special functions
  sdfuncs() ;                                       // Do SD card related functions
  if ( testreq )
  {
    const char* sformat = "Stack %-8s is %4d\n" ;
    testreq = false ;
    // heap_caps_print_heap_info ( MALLOC_CAP_8BIT ) ;
    log_printf ( sformat, pcTaskGetTaskName ( maintask ),
                 uxTaskGetStackHighWaterMark ( maintask ) ) ;
    log_printf ( sformat, pcTaskGetTaskName ( xplaytask ),
                 uxTaskGetStackHighWaterMark ( xplaytask ) ) ;
    #ifdef SDCARD
      log_printf ( sformat, pcTaskGetTaskName ( xsdtask ),
                 uxTaskGetStackHighWaterMark ( xsdtask ) ) ;
    #endif
    log_printf ( "ADC reading is %d, filtered %d\n", adcvalraw, adcval ) ;
    log_printf ( "%d IR interrupts seen\n", ir_intcount ) ;
    if ( pin_exists ( ini_block.sd_detect_pin ) )
    {
      if ( digitalRead ( ini_block.sd_detect_pin ) == LOW )
      {
        log_printf ( "SD card detected\n" ) ;
      }
    }
  }
  delay ( 10 ) ;
}


//**************************************************************************************************
//                             D E C O D E _ S P E C _ C H A R S                                   *
//**************************************************************************************************
// Decode special characters like "&#39;".                                                         *
//**************************************************************************************************
String decode_spec_chars ( String str )
{
  String res ;
  int pos = 0 ;
  while ( pos < (int)str.length() )
  {
    if ( str[pos] == '&' && pos + 2 < (int)str.length() && str[pos + 1] == '#' )
    {
      int end = str.indexOf ( ';', pos + 2 ) ;
      if ( end > pos + 2 )
      {
        bool hex = str[pos + 2] == 'x' || str[pos + 2] == 'X' ;
        String digits = str.substring ( pos + 2 + ( hex ? 1 : 0 ), end ) ;
        char* tail = NULL ;
        uint32_t cp = strtoul ( digits.c_str(), &tail, hex ? 16 : 10 ) ;
        if ( tail && *tail == '\0' && cp > 0 && cp <= 0x10FFFF )
        {
          res += utf8Codepoint ( cp ) ;
          pos = end + 1 ;
          continue ;
        }
      }
    }
    res += str[pos++] ;
  }
  return res ;
}


//**************************************************************************************************
//                                    C H K H D R L I N E                                          *
//**************************************************************************************************
// Check if a line in the header is a reasonable headerline.                                       *
// Normally it should contain something like "icy-xxxx:abcdef".                                    *
//**************************************************************************************************
bool chkhdrline ( const char* str )
{
  char    b ;                                         // Byte examined
  int     len = 0 ;                                   // Lengte van de string

  while ( ( b = *str++ ) )                            // Search to end of string
  {
    len++ ;                                           // Update string length
    if ( ! isalpha ( b ) )                            // Alpha (a-z, A-Z)
    {
      if ( b != '-' )                                 // Minus sign is allowed
      {
        if ( b == ':' )                               // Found a colon?
        {
          return ( ( len > 5 ) && ( len < 70 ) ) ;    // Yes, okay if length is okay
        }
        else
        {
          return false ;                              // Not a legal character
        }
      }
    }
  }
  return false ;                                      // End of string without colon
}


//**************************************************************************************************
//                            S C A N _ C O N T E N T _ L E N G T H                                *
//**************************************************************************************************
// If the line contains content-length information: set clength (content length counter).          *
//**************************************************************************************************
void scan_content_length ( const char* metalinebf )
{
  if ( strstr ( metalinebf, "Content-Length" ) )        // Line contains content length
  {
    clength = atoi ( metalinebf + 15 ) ;                // Yes, set clength
    ESP_LOGI ( TAG, "Content-Length is %d", clength ) ; // Show for debugging purposes
  }
}


//**************************************************************************************************
//                                   H A N D L E B Y T E _ C H                                     *
//**************************************************************************************************
// Handle the next byte of data from server.                                                       *
// Chunked transfer encoding aware. Chunk extensions are not supported.                            *
//**************************************************************************************************
void handlebyte_ch ( uint8_t b )
{
  static int       chunksize = 0 ;                      // Chunkcount read from stream
  static uint16_t  playlistcnt ;                        // Counter to find right entry in playlist
  static int       LFcount ;                            // Detection of end of header
  static bool      ctseen = false ;                     // First line of header seen or not

  if ( chunked &&
       ( datamode & ( DATA |                            // Test op DATA handling
                      METADATA |
                      PLAYLISTDATA ) ) )
  {
    if ( chunkcount == 0 )                              // Expecting a new chunkcount?
    {
      if ( b == '\r' )                                  // Skip CR
      {
        return ;
      }
      else if ( b == '\n' )                             // LF ?
      {
        chunkcount = chunksize ;                        // Yes, set new count
        chunksize = 0 ;                                 // For next decode
        return ;
      }
      // We have received a hexadecimal character.  Decode it and add to the result.
      b = toupper ( b ) - '0' ;                         // Be sure we have uppercase
      if ( b > 9 )
      {
        b = b - 7 ;                                     // Translate A..F to 10..15
      }
      chunksize = ( chunksize << 4 ) + b ;
      return  ;
    }
    chunkcount-- ;                                      // Update count to next chunksize block
  }
  if ( datamode == DATA )                               // Handle next byte of MP3/AAC/Ogg data
  {
    if ( !ad_suppression_active )                       // Do not feed detected advertisements
    {
      *outqp++ = b ;
      if ( outqp == ( outchunk.buf + sizeof(outchunk.buf) ) )     // Buffer full?
      {
        // Send data to playtask queue.  If the buffer cannot be placed within 200 ticks,
        // the queue is full, while the sender tries to send more.  The chunk will be dis-
        // carded it that case.
        if ( xQueueSend ( dataqueue, &outchunk, 200 ) != pdTRUE ) // Send to queue
        {
          ESP_LOGE ( TAG, "MP3 packet dropped!" ) ;
        }
        outqp = outchunk.buf ;                          // Item empty now
      }
    }
    if ( metaint )                                      // No METADATA on Ogg streams or mp3 files
    {
      if ( --datacount == 0 )                           // End of datablock?
      {
        setdatamode ( METADATA ) ;
        metalinebfx = -1 ;                              // Expecting first metabyte (counter)
      }
    }
    return ;
  }
  if ( datamode == INIT )                               // Initialize for header receive
  {
    ctseen = false ;                                    // Contents type not seen yet
    outqp = outchunk.buf ;                              // Item empty now
    metaint = 0 ;                                       // No metaint found
    stream_charset = "" ;                               // No charset announced yet
    icyname_raw = "" ;                                  // No station name received yet
    LFcount = 0 ;                                       // For detection end of header
    bitrate = 0 ;                                       // Bitrate still unknown
    ESP_LOGI ( TAG, "Switch to HEADER" ) ;
    setdatamode ( HEADER ) ;                            // Handle header
    totalcount = 0 ;                                    // Reset totalcount
    metalinebfx = 0 ;                                   // No metadata yet
    metalinebf[0] = '\0' ;
  }
  if ( datamode == HEADER )                             // Handle next byte of MP3 header
  {
    if ( ( b == '\r' ) ||                               // Ignore CR
         ( b == '\0' ) )                                // Ignore NULL
    {
      // Yes, ignore
    }
    else if ( b == '\n' )                               // Linefeed ?
    {
      LFcount++ ;                                       // Count linefeeds
      metalinebf[metalinebfx] = '\0' ;                  // Take care of delimiter
      if ( chkhdrline ( metalinebf ) )                  // Reasonable input?
      {
        ESP_LOGI ( TAG, "Headerline: %s",               // Show headerline
                   metalinebf ) ;
        String metaline = String ( metalinebf ) ;       // Convert to string
        String lcml = metaline ;                        // Use lower case for compare
        lcml.toLowerCase() ;
        if ( lcml.startsWith ( "location:" ) )          // Redirection?
        {
          String location = metaline.substring ( 9 ) ;  // Accept Location:URL and Location: URL
          location.trim() ;
          if ( begin_stream_redirect ( location ) )
          {
            setdatamode ( INIT ) ;
            myQueueSend ( radioqueue, &startcmd ) ;
          }
          else
          {
            setdatamode ( STOPREQD ) ;
            myQueueSend ( radioqueue, &stopcmd ) ;
          }
          return ;                                      // Never parse a redirect response body
        }
        if ( lcml.startsWith ( "content-type" ) )       // Line with "Content-Type: xxxx/yyy"
        {
          ctseen = true ;                               // Yes, remember seeing this
          audio_ct = metaline.substring ( 13 ) ;        // Set contentstype
          audio_ct.trim() ;
          int cp = lcml.indexOf ( "charset=" ) ;
          if ( cp >= 0 )
          {
            stream_charset = metaline.substring ( cp + 8 ) ;
            int semicolon = stream_charset.indexOf ( ';' ) ;
            if ( semicolon >= 0 ) stream_charset.remove ( semicolon ) ;
            stream_charset.replace ( "\"", "" ) ;
            stream_charset.replace ( "'", "" ) ;
            stream_charset.trim() ;
          }
          //ESP_LOGI ( TAG, "%s seen", audio_ct.c_str() ) ;  // Like "audio/mpeg"
        }
        if ( lcml.startsWith ( "icy-br:" ) )
        {
          bitrate = metaline.substring(7).toInt() ;     // Found bitrate tag, read the bitrate
          if ( bitrate == 0 )                           // For Ogg br is like "Quality 2"
          {
            bitrate = 87 ;                              // Dummy bitrate
          }
        }
        else if ( lcml.startsWith ("icy-metaint:" ) )
        {
          metaint = metaline.substring(12).toInt() ;    // Found metaint tag, read the value
        }
        else if ( lcml.startsWith ( "icy-name:" ) )
        {
          icyname_raw = metaline.substring(9) ;         // Decode after all headers are known
        }
        else if ( lcml.startsWith ( "icy-charset:" ) )
        {
          stream_charset = metaline.substring ( 12 ) ;  // Explicit ICY metadata charset
          stream_charset.trim() ;
        }
        else if ( lcml.startsWith ( "transfer-encoding:" ) )
        {
          // Station provides chunked transfer
          if ( lcml.endsWith ( "chunked" ) )
          {
            chunked = true ;                            // Remember chunked transfer mode
            chunkcount = 0 ;                            // Expect chunkcount in DATA
          }
        }
      }
      metalinebfx = 0 ;                                 // Reset this line
      if ( LFcount == 2 )                               // Double LF marks end of header?
      {
        if ( ctseen )                                   // Content type seen?
        {
          icyname = decode_spec_chars ( decodeStreamText ( icyname_raw,
                                                           stream_charset ) ) ;
          icyname.trim() ;
          if ( icyname.isEmpty() ) icyname = presetinfo.hsym ;
          tftset ( 2, icyname ) ;
          mqttpub.trigger ( MQTT_ICYNAME ) ;
          ESP_LOGI ( TAG, "Switch to DATA, bitrate is " // Show bitrate
                    "%d kbps, metaint is %d",           // and metaint
                    bitrate, metaint ) ;
          setdatamode ( DATA ) ;                        // Expecting data now
          datacount = metaint ;                         // Number of bytes before first metadata
          queueToPt ( QSTARTSONG ) ;                    // Queue a request to start song
        }
      }
    }
    else
    {
      metalinebf[metalinebfx++] = (char)b ;             // Normal character, put new char in metaline
      if ( metalinebfx >= METASIZ )                     // Prevent overflow
      {
        metalinebfx-- ;
      }
      LFcount = 0 ;                                     // Reset double CRLF detection
    }
    return ;
  }
  if ( datamode == METADATA )                           // Handle next byte of metadata
  {
    if ( metalinebfx < 0 )                              // First byte of metadata?
    {
      metalinebfx = 0 ;                                 // Prepare to store first character
      metacount = b * 16 + 1 ;                          // New count for metadata including length byte
    }
    else
    {
      metalinebf[metalinebfx++] = (char)b ;             // Normal character, put new char in metaline
      if ( metalinebfx >= METASIZ )                     // Prevent overflow
      {
        metalinebfx-- ;
      }
    }
    if ( --metacount == 0 )
    {
      metalinebf[metalinebfx] = '\0' ;                  // Make sure line is limited
      if ( strlen ( metalinebf ) )                      // Any info present?
      {
        // metaline contains artist and song name.  For example:
        // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
        // Sometimes it is just other info like:
        // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
        // Detect ads before showstreamtitle(), which intentionally stops parsing at the first
        // semicolon and would therefore hide advertisement fields such as StreamUrl.
        if ( isAdvertisementMetadata ( metalinebf ) )
        {
          ESP_LOGI ( TAG, "Advertisement metadata detected: %s", metalinebf ) ;
          icystreamtitle = "" ;                        // Do not publish or display the advertiser title
          oldstreamtitle = "" ;
          tftset ( 1, "" ) ;                           // Remove the advertiser title from the display
          mqttpub.trigger ( MQTT_STREAMTITLE ) ;
          if ( !ad_suppression_active )
          {
            ad_suppression_active = true ;             // Continue parsing, but discard audio bytes
            outqp = outchunk.buf ;                      // Drop a partially collected audio packet
            queueToPt ( QSTOPSONG ) ;                  // Empty queued ad audio and mute the decoder
            ESP_LOGI ( TAG, "Advertisement audio suppressed until normal metadata" ) ;
          }
          else
          {
            ESP_LOGI ( TAG, "Advertisement suppression remains active" ) ;
          }
        }
        else
        {
          if ( ad_suppression_active )
          {
            ad_suppression_active = false ;            // Music metadata marks the end of the ad
            outqp = outchunk.buf ;                      // Resume on a clean 32-byte packet
            queueToPt ( QSTARTSONG ) ;
            ESP_LOGI ( TAG, "Normal metadata received, audio resumed" ) ;
          }
          // Isolate the StreamTitle, remove leading and trailing quotes if present.
          if ( showstreamtitle ( metalinebf ) )         // Show artist and title if present
          {
            mqttpub.trigger ( MQTT_STREAMTITLE ) ;      // Title change: publish through MQTT
          }
        }
      }
      if ( metalinebfx  > ( METASIZ - 10 ) )            // Unlikely metaline length?
      {
        ESP_LOGE ( TAG, "Metadata block too long!" ) ;  // Probably no metadata
        // Skipping all Metadata from now on.
        metaint = 0 ;
      }
      datacount = metaint ;                             // Reset data count
      //bufcnt = 0 ;                                    // Reset buffer count
      setdatamode ( DATA ) ;                            // Expecting data
    }
  }
  if ( datamode == PLAYLISTINIT )                       // Initialize for receive .m3u file
  {
    // We are going to use metadata to read the lines from the .m3u file
    // Sometimes this will only contain a single line
    metalinebfx = 0 ;                                   // Prepare for new line
    LFcount = 0 ;                                       // For detection end of header
    setdatamode ( PLAYLISTHEADER ) ;                    // Handle playlist header
    playlistcnt = 0 ;                                   // Reset for compare
    totalcount = 0 ;                                    // Reset totalcount
    clength = 0xFFFFFFFF ;                              // Content-length unknown
    ESP_LOGI ( TAG, "Read from playlist" ) ;
  }
  if ( datamode == PLAYLISTHEADER )                     // Read header
  {
    if ( ( b > 0x7F ) ||                                // Ignore unprintable characters
         ( b == '\r' ) ||                               // Ignore CR
         ( b == '\0' ) )                                // Ignore NULL
    {
      return ;                                          // Quick return
    }
    else if ( b == '\n' )                               // Linefeed ?
    {
      LFcount++ ;                                       // Count linefeeds
      metalinebf[metalinebfx] = '\0' ;                  // Take care of delimeter
      ESP_LOGI ( TAG, "Playlistheader: %s",             // Show playlistheader
                 metalinebf ) ;
      scan_content_length ( metalinebf ) ;              // Check if it is a content-length line
      String playlistheader = String ( metalinebf ) ;
      String lcplaylistheader = playlistheader ;
      lcplaylistheader.toLowerCase() ;
      if ( lcplaylistheader.startsWith ( "location:" ) )
      {
        String location = playlistheader.substring ( 9 ) ;
        location.trim() ;
        if ( begin_stream_redirect ( location ) )
        {
          setdatamode ( INIT ) ;
          myQueueSend ( radioqueue, &startcmd ) ;
        }
        else
        {
          setdatamode ( STOPREQD ) ;
          myQueueSend ( radioqueue, &stopcmd ) ;
        }
        return ;                                      // Do not parse the redirect response body
      }
      int cp = lcplaylistheader.indexOf ( "charset=" ) ;
      if ( cp >= 0 )
      {
        stream_charset = playlistheader.substring ( cp + 8 ) ;
        int semicolon = stream_charset.indexOf ( ';' ) ;
        if ( semicolon >= 0 ) stream_charset.remove ( semicolon ) ;
        stream_charset.replace ( "\"", "" ) ;
        stream_charset.replace ( "'", "" ) ;
        stream_charset.trim() ;
      }
      metalinebfx = 0 ;                                 // Ready for next line
      if ( LFcount == 2 )
      {
        ESP_LOGI ( TAG, "Switch to PLAYLISTDATA, "      // For debug
                   "search for entry %d",
                   presetinfo.playlistnr ) ;
        setdatamode ( PLAYLISTDATA ) ;                  // Expecting data now
        mqttpub.trigger ( MQTT_PLAYLISTPOS ) ;          // Playlistposition to MQTT
        return ;
      }
    }
    else
    {
      metalinebf[metalinebfx++] = (char)b ;             // Normal character, put new char in metaline
      if ( metalinebfx >= METASIZ )                     // Prevent overflow
      {
        metalinebfx-- ;
      }
      LFcount = 0 ;                                     // Reset double CRLF detection
    }
  }
  if ( datamode == PLAYLISTDATA )                       // Read next byte of .m3u file data
  {
    clength-- ;                                         // Decrease content length by 1
    if ( ( b > 0x7F ) ||                                // Ignore unprintable characters
         ( b == '\r' ) ||                               // Ignore CR
         ( b == '\0' ) )                                // Ignore NULL
    {
      // Yes, ignore
    }
    if ( b != '\n' )                                    // Linefeed?
    { // No, normal character in playlistdata,
      metalinebf[metalinebfx++] = (char)b ;             // add it to metaline
      if ( metalinebfx >= METASIZ )                     // Prevent overflow
      {
        metalinebfx-- ;
      }
    }
    if ( ( b == '\n' ) ||                               // linefeed?
         ( clength == 0 ) )                             // Or end of playlist data contents
    {
      int inx ;                                         // Pointer in metaline
      metalinebf[metalinebfx] = '\0' ;                  // Take care of delimeter
      ESP_LOGI ( TAG, "Playlistdata: %s",               // Show playlist data
                 metalinebf ) ;
      if ( strlen ( metalinebf ) < 5 )                  // Skip short lines
      {
        metalinebfx = 0 ;                               // Flush line
        metalinebf[0] = '\0' ;
        return ;
      }
      String metaline = String ( metalinebf ) ;         // Convert to string
      if ( metaline.indexOf ( "#EXTINF:" ) >= 0 )       // Info?
      {
        if ( presetinfo.playlistnr == playlistcnt )     // Info for this entry?
        {
          inx = metaline.indexOf ( "," ) ;              // Comma in this line?
          if ( inx > 0 )
          {
            // Show artist and title if present in metadata
            if ( showstreamtitle ( metaline.substring ( inx + 1 ).c_str(), true ) )
            {
              mqttpub.trigger ( MQTT_STREAMTITLE ) ;    // Title change: request publishing to MQTT
            }
          }
        }
      }
      if ( metaline.startsWith ( "#" ) )                // Commentline?
      {
        metalinebfx = 0 ;                               // Yes, ignore
        return ;                                        // Ignore commentlines
      }
      // Now we have an URL for a .mp3 file or stream.
      presetinfo.highest_playlistnr = playlistcnt ;
      ESP_LOGI ( TAG, "Entry %d in playlist found: %s", playlistcnt, metalinebf ) ;
      if ( presetinfo.playlistnr == playlistcnt )       // Is it the right one?
      {
        presetinfo.host = metaline ;                    // Keep http:// or https:// for transport choice
        presetinfo.hsym = metaline ;                    // Do not know symbolic name
        presetinfo.station_state = ST_PLAYLIST ;        // Set playlist mode
        setdatamode ( INIT ) ;                          // Yes, mode to INIT again
        myQueueSend ( radioqueue, &startcmd ) ;         // Restart with new found host
      }
      metalinebfx = 0 ;                                 // Prepare for next line
      playlistcnt++ ;                                   // Next entry in playlist
    }
  }
}


//**************************************************************************************************
//                                  H A N D L E _ N O T F O U N D                                  *
//**************************************************************************************************
// If parameters are present: handle them.                                                         *
// Otherwise: transfer file from SPIFFS to webserver client.                                       *
//**************************************************************************************************
void handle_notfound ( AsyncWebServerRequest *request )
{
  String       ct = String ( "text/plain" ) ;         // Default content type
  String       path ;                                 // Filename for SPIFFS
  String       reply ;                                // Reply on not file request
  const char*  p ;                                    // Reply from analyzecmd
  int          numargs ;                              // Number of arguments
  int          i ;                                    // Index in arguments
  String       key ;                                  // Name of parameter i
  String       contents ;                             // Value of parameter i
  String       cmd ;                                  // Command to analyze
  String       sndstr = String() ;                    // String to send

  numargs = request->params() ;                       // Haal aantal parameters
  for ( i = 0 ; i < numargs ; i++ )                   // Scan de parameters
  {
    key = request->argName ( i ) ;                    // Get name (key)
    contents = request->arg ( i ) ;                   // Get value
    chomp ( key ) ;                                   // Remove leading/trailing spaces
    chomp ( contents ) ;                              // Remove leading/trailing spaces
    if ( key == "version" )                           // Skip "version" parameter
    {
      continue ;
    }
    cmd = key + String ( "=" ) + contents ;           // Format command to analyze
    //ESP_LOGI ( TAG, "Http command is %s", cmd.c_str() ) ;
    p = analyzeCmd ( cmd.c_str() ) ;                  // Analyze command
    sndstr += String ( p ) ;                          // Content of HTTP response follows the header
  }
  if ( ! sndstr.isEmpty() )                           // Any argument handled?
  {
    request->send ( 200, ct, sndstr ) ;               // Send reply
    return ;                                          // Quick return
  }
  // No parameters, it is just a request for a new page, style sheet or icon
  path = request->url() ;                             // Path for requested filename
  if ( path == String ( "/" ) )                       // Default is index.html
  {
    path = String ( "/index.html" ) ;                 // Select index.html
  }
  #ifndef SDCARD
    if ( path == "/mp3play.html" )                    // MP3 player page requested?
    {
      path = "/nomp3play.html" ;                      // Yes, select dummy mp3 player page
    }
  #endif
  if ( SPIFFS.exists ( path ) )                       // Does it exist in SPIFFS?
  {
    ct = getContentType ( path.c_str() ) ;            // Get content type
    request->send ( SPIFFS, path, ct ) ;              // Send to client
  }
  else
  {
    request->send ( 200, ct, String ( "sorry" ) ) ;   // Send reply
  }
}


//**************************************************************************************************
//                                         C H O M P                                               *
//**************************************************************************************************
// Do some filtering on de inputstring:                                                            *
//  - String comment part (starting with "#").                                                     *
//  - Strip trailing CR.                                                                           *
//  - Strip leading spaces.                                                                        *
//  - Strip trailing spaces.                                                                       *
//**************************************************************************************************
void chomp ( String &str )
{
  int   inx ;                                         // Index in de input string

  if ( ( inx = str.indexOf ( "#" ) ) >= 0 )           // Comment line or partial comment?
  {
    str.remove ( inx ) ;                              // Yes, remove
  }
  str.trim() ;                                        // Remove spaces and CR
}


//**************************************************************************************************
//                                     A N A L Y Z E C M D                                         *
//**************************************************************************************************
// Handling of the various commands from remote webclient, Serial or MQTT.                         *
// Version for handling string with: <parameter>=<value>                                           *
//**************************************************************************************************
const char* analyzeCmd ( const char* str )
{
  char*        value ;                           // Points to value after equalsign in command
  const char*  res ;                             // Result of analyzeCmd

  value = strstr ( str, "=" ) ;                  // See if command contains a "="
  if ( value )
  {
    *value = '\0' ;                              // Separate command from value
    res = analyzeCmd ( str, value + 1 ) ;        // Analyze command and handle it
    *value = '=' ;                               // Restore equal sign
  }
  else
  {
    res = analyzeCmd ( str, "0" ) ;              // No value, assume zero
  }
  return res ;
}


//**************************************************************************************************
//                                     A N A L Y Z E C M D                                         *
//**************************************************************************************************
// Handling of the various commands from remote webclient, serial or MQTT.                         *
// par holds the parametername and val holds the value.                                            *
// "wifi_00" and "preset_00" may appear more than once, like wifi_01, wifi_02, etc.                *
// Examples with available parameters:                                                             *
//   preset     = 12                        // Select start preset to connect to                   *
//   track      = songname                  // Select MP3 track from SD card                       *
//   trackinx   = n                         // Select MP3 track by index from SD card.             *
//   random                                 // Select random mP3 track                             *
//   preset_00  = <mp3 stream>              // Specify station for a preset 00-max *)              *
//   volume     = 95                        // Percentage between 0 and 100                        *
//   upvolume   = 2                         // Add percentage to current volume                    *
//   downvolume = 2                         // Subtract percentage from current volume             *
//   toneha     = <0..15>                   // Setting treble gain                                 *
//   tonehf     = <0..15>                   // Setting treble frequency                            *
//   tonela     = <0..15>                   // Setting bass gain                                   *
//   tonelf     = <0..15>                   // Setting treble frequency                            *
//   station    = <mp3 stream>              // Select new station (will not be saved)              *
//   station    = <URL>.mp3                 // Play standalone .mp3 file (not saved)               *
//   station    = <URL>.m3u                 // Select playlist (will not be saved)                 *
//   resume                                 // Resume playing                                      *
//   (un)mute                               // Mute/unmute the music                               *
//   sleep                                  // Go into deep sleep mode                             *
//   wifi_00    = mySSID/mypassword         // Set WiFi SSID and password *)                       *
//   mqttbroker = mybroker.com              // Set MQTT broker to use *)                           *
//   mqttprefix = XP93g                     // Set MQTT broker to use                              *
//   mqttport   = 1883                      // Set MQTT port to use, default 1883 *)               *
//   mqttuser   = myuser                    // Set MQTT user for authentication *)                 *
//   mqttpasswd = mypassword                // Set MQTT password for authentication *)             *
//   mqttrefresh                            // Refresh all MQTT items                              *
//   clk_server = pool.ntp.org              // Time server to be used *)                           *
//   clk_offset = <-11..+14>                // Offset with respect to UTC in hours *)              *
//   clk_dst    = <1..2>                    // Offset during daylight saving time in hours *)      *
//   settings                               // Returns setting like presets and tone               *
//   status                                 // Show current URL to play                            *
//   mp3list                                // Returns list with all MP3 files                     *
//   test                                   // For test purposes                                   *
//   reset                                  // Restart the ESP32                                   *
//   bat0       = 2318                      // ADC value for an empty battery                      *
//   bat100     = 2916                      // ADC value for a fully charged battery               *
//  Commands marked with "*)" are sensible during initialization only                              *
//**************************************************************************************************
const char* analyzeCmd ( const char* par, const char* val )
{
  String             argument ;                       // Argument as string
  String             value ;                          // Value of an argument as a string
  String             tmpstr ;                         // Temporary storage of a string
  int                ivalue ;                         // Value of argument as an integer
  static char        reply[180] ;                     // Reply to client, will be returned
  bool               relative = false ;               // Relative argument (+ or -)

  blset ( true ) ;                                    // Enable backlight of TFT
  strcpy ( reply, "Command accepted" ) ;              // Default reply
  argument = String ( par ) ;                         // Get the argument
  chomp ( argument ) ;                                // Remove comment and useless spaces
  if ( argument.length() == 0 )                       // Lege commandline (comment)?
  {
    return reply ;                                    // Ignore
  }
  argument.toLowerCase() ;                            // Force to lower case
  value = String ( val ) ;                            // Get the specified value
  chomp ( value ) ;                                   // Remove comment and extra spaces
  ivalue = value.toInt() ;                            // Also as an integer
  if ( argument == "digit" )                         // Explicit numeric-entry command?
  {
    if ( value.length() == 1 && isdigit ( value[0] ) )
    {
      enterStationDigit ( value[0] - '0' ) ;
      sprintf ( reply, "Preset digit %c", value[0] ) ;
    }
    else
    {
      strcpy ( reply, "Illegal preset digit" ) ;
    }
    return reply ;
  }
  if ( argument == "ok" )                            // Middle/OK button
  {
    if ( station_number_entry ) confirmStationNumber() ;
    else confirmStationList() ;
    strcpy ( reply, "Station selection OK" ) ;
    return reply ;
  }
  if ( argument == "sdlist" )                        // Open SD track browser
  {
    if ( station_list_active )
    {
      restoreRadioView() ;
      strcpy ( reply, "Menu closed" ) ;
    }
    else
    {
      openSDList() ;
      strcpy ( reply, "SD track list" ) ;
    }
    return reply ;
  }
  if ( station_list_active )                          // Arrow keys navigate the station list
  {
    if ( argument == "up" || argument == "uppreset" )
    {
      moveStationList ( 1, 7 ) ;                     // U takes the former R/page-forward function
      return reply ;
    }
    if ( argument == "down" || argument == "downpreset" )
    {
      moveStationList ( -1, 7 ) ;                    // D takes the former L/page-back function
      return reply ;
    }
    if ( argument == "left" || argument == "downvolume" )
    {
      moveStationList ( 1, 1 ) ;                     // L takes the former D/next-row function
      return reply ;
    }
    if ( argument == "right" || argument == "upvolume" )
    {
      moveStationList ( -1, 1 ) ;                    // R takes the former U/previous-row function
      return reply ;
    }
  }
  #ifdef SDCARD
    if ( SD_playing &&
         ( argument == "left" || argument == "downpreset" ||
           argument == "right" || argument == "uppreset" ) )
    {
      const int16_t direction = ( argument == "left" || argument == "downpreset" ) ? -1 : 1 ;
      int16_t next = SD_curindex + direction ;
      if ( next < 0 ) next = SD_filecount - 1 ;
      else if ( next >= SD_filecount ) next = 0 ;
      if ( SD_filecount > 0 )
      {
        if ( SD_cover_visible )
        {
          SD_cover_visible = false ;
          restoreRadioView() ;
        }
        getSDFileName ( next ) ;
        myQueueSend ( sdqueue, &startcmd ) ;
        strcpy ( reply, direction > 0 ? "MP3 next track" : "MP3 previous track" ) ;
      }
      else
      {
        strcpy ( reply, "No MP3 tracks" ) ;
      }
      return reply ;
    }
  #endif
  if ( ( relative = argument.startsWith ( "up" ) ) )  // + relative setting?
  {
    argument = argument.substring ( 2 ) ;             // Remove the "up"-part
  }
  else if ( ( relative = 
                 argument.startsWith ( "down" )  ) )  // - relative setting?
  {
    ivalue = -ivalue ;                                // But with negative value
    argument = argument.substring ( 4 ) ;             // Remove the "down"-part
  }
  if ( value.startsWith ( "http://" ) )               // Does (possible) URL contain "http://"?
  {
    value.remove ( 0, 7 ) ;                           // Yes, remove it
  }
  else if ( argument == "volume" )                    // Volume setting?
  {
    // Volume may be of the form "upvolume", "downvolume" or "volume" for relative or absolute setting
    if ( relative )                                   // + relative setting?
    {
      ini_block.reqvol = player_getVolume() +         // Up/down by 0.5 or more dB
                         ivalue ;
    }
    else
    {
      ini_block.reqvol = ivalue ;                     // Absolue setting
    }
    if ( ini_block.reqvol > 127 )                     // Wrapped around?
    {
      ini_block.reqvol = 0 ;                          // Yes, keep at zero
    }
    if ( ini_block.reqvol > 100 )
    {
      ini_block.reqvol = 100 ;                        // Limit to normal values
    }
    muteflag = false ;                                // Stop possibly muting
    sprintf ( reply, "Volume is now %d",              // Reply new volume
              ini_block.reqvol ) ;
  }
  else if ( argument.indexOf ( "mute" ) >= 0 )        // Mute/unmute request
  {
    muteflag = ( argument == "mute" ) ;               // Request volume to zero/normal
  }
  else if ( argument.startsWith ( "ir_" ) )           // Ir setting?
  { // Do not handle here
  }
  else if ( argument.startsWith ( "preset_" ) )       // Enumerated preset?
  {
    ivalue = argument.substring ( 7 ).toInt() ;       // Only look for max
  }
  else if ( argument == "preset" )                    // (UP/DOWN)Preset station?
  {
    if ( nextPreset ( ivalue, relative ) )             // Yes, set new preset
    {
      sprintf ( reply, "Preset is now %d",            // Reply new preset
                presetinfo.preset ) ;
      if ( NetworkFound ) myQueueSend ( radioqueue, &startcmd ) ;
    }
    else
    {
      sprintf ( reply, "Preset %d does not exist", ivalue ) ;
    }
  }
#ifdef SDCARD
  else if ( argument == "track" )                     // MP3 track request?
  {
    if ( relative )                                   // Yes. "uptrack" has numeric value
    {
      getSDFileName ( SD_curindex + ivalue ) ;        // Select next file
    }
    else
    {
      setSDFileName ( value.c_str() ) ;               // Select new track by filename
    }
    myQueueSend ( sdqueue, &startcmd ) ;              // Signal SDfuncs()
  }
  else if ( argument == "trackinx" )                  // MP3 track request?
  {
    getSDFileName ( ivalue ) ;                        // Select file by index
    myQueueSend ( sdqueue, &startcmd ) ;              // Signal SDfuncs()
  }
  else if ( argument == "random" )                    // Random MP3 track request?
  {
    getSDFileName ( -1 ) ;                            // Yes, select new random track
    ESP_LOGI ( TAG, "Random file is %s",              // Show filename
               getCurrentSDFileName() ) ;
    myQueueSend ( sdqueue, &startcmd ) ;              // Signal SDfuncs()
  }
#endif
  else if ( ( value.length() > 0 ) &&
            ( argument == "station" ) )               // Station in the form address:port
  {
    presetinfo.host = value ;                         // Save it for storage and selection later
    presetinfo.hsym = value ;                         // We do not know the symbolic name
    presetinfo.station_state = ST_STATION ;           // Set station mode
    myQueueSend ( radioqueue, &startcmd ) ;           // Signal radiofuncs()
    sprintf ( reply,
              "Select %s",                            // Format reply
              value.c_str() ) ;
    utf8ascii_ip ( reply ) ;                          // Remove possible strange characters
  }
  else if ( argument == "sleep" )                     // Sleep request?
  {
    sleepreq = true ;                                 // Yes, set request flag
  }
  else if ( argument == "status" )                    // Status request
  {
    if ( datamode == STOPPED )
    {
      sprintf ( reply, "Player stopped" ) ;           // Format reply
    }
    else
    {
      sprintf ( reply, "%s - %s", icyname.c_str(),
                icystreamtitle.c_str() ) ;            // Streamtitle from metadata
    }
  }
  else if ( argument == "reset" )                     // Reset request
  {
    resetreq = true ;                                 // Reset all
  }
  else if ( argument == "test" )                      // Test command
  {
    sprintf ( reply, "Free memory is %d/%d, "         // Get some info to display
              "chunks in queue %d, bitrate %d kbps\n",
              heapspace,
              ESP.getFreeHeap(),
              uxQueueMessagesWaiting ( dataqueue ),
              mbitrate ) ;
    testreq = true ;                                  // Request to print info in main program
  }
  // Commands for bass/treble control
  else if ( argument.startsWith ( "tone" ) )          // Tone command
  {
    if ( argument.indexOf ( "ha" ) > 0 )              // High amplitue? (for treble)
    {
      ini_block.rtone[0] = ivalue ;                   // Yes, prepare to set ST_AMPLITUDE
    }
    if ( argument.indexOf ( "hf" ) > 0 )              // High frequency? (for treble)
    {
      ini_block.rtone[1] = ivalue ;                   // Yes, prepare to set ST_FREQLIMIT
    }
    if ( argument.indexOf ( "la" ) > 0 )              // Low amplitue? (for bass)
    {
      ini_block.rtone[2] = ivalue ;                   // Yes, prepare to set SB_AMPLITUDE
    }
    if ( argument.indexOf ( "lf" ) > 0 )              // High frequency? (for bass)
    {
      ini_block.rtone[3] = ivalue ;                   // Yes, prepare to set SB_FREQLIMIT
    }
    reqtone = true ;                                  // Set change request
    sprintf ( reply, "Parameter for bass/treble %s set to %d",
              argument.c_str(), ivalue ) ;
  }
  else if ( argument == "rate" )                      // Rate command?
  {
    player_AdjustRate ( ivalue ) ;                    // Yes, adjust
  }
  else if ( argument.startsWith ( "mqtt" ) )          // Parameter fo MQTT?
  {
    strcpy ( reply, "MQTT broker parameter changed. Save and restart to have effect" ) ;
    if ( argument.indexOf ( "broker" ) > 0 )          // Broker specified?
    {
      ini_block.mqttbroker = value ;                  // Yes, set broker accordingly
    }
    else if ( argument.indexOf ( "prefix" ) > 0 )     // Port specified?
    {
      ini_block.mqttprefix = value ;                  // Yes, set port user accordingly
    }
    else if ( argument.indexOf ( "port" ) > 0 )       // Port specified?
    {
      ini_block.mqttport = ivalue ;                   // Yes, set port user accordingly
    }
    else if ( argument.indexOf ( "user" ) > 0 )       // User specified?
    {
      ini_block.mqttuser = value ;                    // Yes, set user accordingly
    }
    else if ( argument.indexOf ( "passwd" ) > 0 )     // Password specified?
    {
      ini_block.mqttpasswd = value.c_str() ;          // Yes, set broker password accordingly
    }
    else if ( argument.indexOf ( "refresh" ) > 0 )    // Refresh all items?
    {
      mqttpub.triggerall() ;                          // Yes, request to republish all items
    }
  }
  else if ( argument.startsWith ( "clk_" ) )          // TOD parameter?
  {
    if ( argument.indexOf ( "server" ) > 0 )          // Yes, NTP server spec?
    {
      ini_block.clk_server = value ;                  // Yes, set server
    }
    if ( argument.indexOf ( "offset" ) > 0 )          // Offset with respect to UTC spec?
    {
      ini_block.clk_offset = value.toInt() ;          // Yes, set offset
    }
    if ( argument.indexOf ( "dst" ) > 0 )             // Offset duringe DST spec?
    {
      ini_block.clk_dst = value.toInt() ;             // Yes, set DST offset
    }
  }
  else if ( argument.startsWith ( "bat" ) )           // Battery ADC value?
  {
    if ( argument.indexOf ( "100" ) )                 // 100 percent value?
    {
      ini_block.bat100 = ivalue ;                     // Yes, set it
    }
    else if ( argument.indexOf ( "0" ) )              // 0 percent value?
    {
      ini_block.bat0 = ivalue ;                       // Yes, set it
    }
  }
  else
  {
    sprintf ( reply, "%s called with illegal parameter: %s",
              NAME, argument.c_str() ) ;
  }
  return reply ;                                      // Return reply to the caller
}


//**************************************************************************************************
//* Function that are called from spfunc().                                                        *
//* Note that some device dependent function are place in the *.h files.                           *
//**************************************************************************************************

//**************************************************************************************************
//                                      D I S P L A Y I N F O                                      *
//**************************************************************************************************
// Show a string on the LCD at a specified y-position (0..2) in a specified color.                 *
// The parameter is the index in tftdata[].                                                        *
//**************************************************************************************************
void displayinfo ( uint16_t inx )
{
  uint16_t       width = dsp_getwidth() ;                  // Normal number of colums
  scrseg_struct* p = &tftdata[inx] ;
  uint16_t len ;                                           // Length of string, later buffer length

  if ( inx == 0 )                                          // Topline is shorter
  {
    width += TIMEPOS ;                                     // Leave space for time
  }
    #ifdef BLUETFT
      #ifdef SDCARD
      if ( inx == 2 ) width -= ( SD_playing ? TRACKNUMWIDTH : STATIONNUMWIDTH ) ;
      #else
      if ( inx == 2 ) width -= STATIONNUMWIDTH ;           // Leave room for preset number
    #endif
  #endif
  if ( dsp_ok )                                            // TFT active?
  {
    dsp_fillRect ( 0, p->y, width, p->height, BLACK ) ;    // Clear the space for new info
    if ( ( dsp_getheight() > 64 ) && ( p->y > 1 ) )        // Need and space for divider?
    {
      #ifdef BLUETFT
        dsp_fillRect ( 0, p->y - 2, dsp_getwidth(), 2, GREEN ) ; // Keep divider outside text
        if ( inx == 1 )
        {
          displaybattery ( ini_block.bat0, ini_block.bat100, adcval ) ; // Restore battery bar now
        }
      #else
        dsp_fillRect ( 0, p->y - 4, width, 1, GREEN ) ;    // Legacy display position
      #endif
    }
    len = p->str.length() ;                                // Required length of buffer
    if ( len++ )                                           // Check string length, set buffer length
    {
      char buf [ len ] ;                                   // Need some buffer space
      p->str.toCharArray ( buf, len ) ;                    // Make a local copy of the string
      dsp_setTextColor ( p->color ) ;                      // Set the requested color
      #ifdef BLUETFT
        if ( !bluetft_drawSmoothText ( inx, buf, p->color, width ) )
        {
          utf8ascii_ip ( buf ) ;                           // Fallback if a font is missing
          dsp_setCursor ( 0, p->y ) ;
          dsp_println ( buf ) ;
        }
      #else
        utf8ascii_ip ( buf ) ;                             // Legacy displays use extended ASCII
        dsp_setCursor ( 0, p->y ) ;
        dsp_println ( buf ) ;
      #endif
    }
    #ifdef BLUETFT
      if ( inx == 2 )
      {
        #ifdef SDCARD
          if ( SD_playing )
          {
            String trackNumber = String ( SD_curindex + 1 ) + "/" + String ( SD_filecount ) ;
            bluetft_drawTrackNumber ( trackNumber.c_str(), p->color ) ;
            displaySDProgress() ;                          // Restore progress after title redraw
          }
          else
        #endif
        {
          String stationNumber = station_number_entry ? station_number_input :
                                 String ( presetinfo.preset ) ;
          bluetft_drawStationNumber ( stationNumber.c_str(),
                                      station_number_entry ? MAGENTA : p->color ) ;
        }
      }
    #endif
  }
}


//**************************************************************************************************
//                                         G E T T I M E                                           *
//**************************************************************************************************
// Retrieve the local time from NTP server and convert to string.                                  *
// Will be called every second.                                                                    *
//**************************************************************************************************
void gettime()
{
  static int16_t delaycount = 0 ;                         // To reduce number of NTP requests
  static int16_t retrycount = 100 ;

  if ( --delaycount <= 0 )                                // Sync every few hours
  {
    delaycount = 7200 ;                                   // Reset counter
    if ( timeinfo.tm_year )                               // Legal time found?
    {
      ESP_LOGI ( TAG, "Sync TOD, old value is %s", timetxt ) ;
    }
    ESP_LOGI ( TAG, "Sync TOD" ) ;
    if ( !getLocalTime ( &timeinfo ) )                    // Read from NTP server
    {
      ESP_LOGW ( TAG, "Failed to obtain time!" ) ;        // Error
      timeinfo.tm_year = 0 ;                              // Set current time to illegal
      if ( retrycount )                                   // Give up syncing?
      {
        retrycount-- ;                                    // No try again
        delaycount = 5 ;                                  // Retry after 5 seconds
      }
    }
    else
    {
      ESP_LOGI ( TAG, "TOD synced" ) ;                    // Time has been synced
    }
  }
  sprintf ( timetxt, "%02d:%02d:%02d",                    // Format new time to a string
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec ) ;
}

#if defined(DEC_VS1053) || defined(DEC_VS1003)

//**************************************************************************************************
//                         P L A Y T A S K  ( V S 1 0 X 3 )                                        *
//**************************************************************************************************
// Play stream data from input queue. Version for VS1003/VS1053.                                   *
// Handle all I/O to the hardware decoder during normal playing.                                  *
//**************************************************************************************************
void playtask ( void * parameter )
{
  // static bool once = true ;                                      // Show chunk once  #if defined(DEC_VS1053) || defined(DEC_VS1003)
  bool VS_okay ;                                                    // VS is okay or not
  uint32_t lastRecovery = 0 ;                                       // Avoid recovery storm on a hard fault
  uint8_t audioChunksBeforeYield = 0 ;                              // Keep IDLE0 alive while queue stays full

  ESP_LOGI ( TAG, "Starting VS1003 playtask.." ) ;
  VS_okay = VS1053_begin ( ini_block.vs_cs_pin,                     // Make instance of player and initialize
                           ini_block.vs_dcs_pin,
                           ini_block.vs_dreq_pin,
                           ini_block.shutdown_pin,
                           ini_block.shutdownx_pin ) ;
  player_ready = VS_okay ;
  player_init_done = true ;
  ESP_LOGI ( TAG, "VS1003 initialization %s", VS_okay ? "ready" : "failed" ) ;
  while ( true )
  {
    if ( xQueueReceive ( dataqueue, &inchunk, 5 ) == pdTRUE )       // Command/data from queue?
    {
      switch ( inchunk.datatyp )
      {
        case QDATA:
          if ( VS_okay )
          {
            if ( !vs1053player->playChunk ( inchunk.buf,              // Bounded DREQ wait in driver
                                             sizeof(inchunk.buf) ) )
            {
              VS_okay = false ;
              player_ready = VS_okay ;
              uint32_t now = millis() ;
              if ( lastRecovery == 0 || now - lastRecovery >= 5000 )
              {
                lastRecovery = now ;
                ESP_LOGE ( TAG, "VS1003 stopped accepting audio, trying recovery" ) ;
                VS_okay = vs1053player->recover() ;
                player_ready = VS_okay ;
                if ( VS_okay )
                {
                  vs1053player->setVolume ( ini_block.reqvol ) ;
                  vs1053player->startSong() ;
                }
              }
            }
            totalcount += sizeof(inchunk.buf) ;                       // Count the bytes
          }
          else
          {
            uint32_t now = millis() ;
            if ( lastRecovery == 0 || now - lastRecovery >= 5000 )
            {
              lastRecovery = now ;
              ESP_LOGW ( TAG, "Retrying VS1003 recovery while stream data is available" ) ;
              VS_okay = vs1053player->recover() ;
              player_ready = VS_okay ;
              if ( VS_okay )
              {
                vs1053player->setVolume ( ini_block.reqvol ) ;
                vs1053player->startSong() ;
              }
            }
          }
          break ;
        case QSTARTSONG:
          if ( !VS_okay )
          {
            VS_okay = vs1053player->recover() ;                       // Retry on each new stream/track
            player_ready = VS_okay ;
          }
          if ( VS_okay )
          {
            ESP_LOGI ( TAG, "QSTARTSONG, VS1003 volume %u", ini_block.reqvol ) ;
            playingstat = 1 ;                                         // Status for MQTT
            mqttpub.trigger ( MQTT_PLAYING ) ;                        // Request publishing to MQTT
            vs1053player->setVolume ( ini_block.reqvol ) ;            // Unmute
            vs1053player->startSong() ;                               // START, start player
          }
          break ;
        case QSTOPSONG:
          if ( VS_okay )
          {
            ESP_LOGI ( TAG, "QSTOPSONG" ) ;
            playingstat = 0 ;                                         // Status for MQTT
            mqttpub.trigger ( MQTT_PLAYING ) ;                        // Request publishing to MQTT
            vs1053player->setVolume ( 0 ) ;                           // Mute
            vs1053player->stopSong() ;                                // STOP, stop player
          }
          break ;
        case QSTOPTASK:
          vTaskDelete ( NULL ) ;                                      // Stop task
          break ;
        default:
          break ;
      }
      if ( inchunk.datatyp == QDATA && ++audioChunksBeforeYield >= 8 )
      {
        audioChunksBeforeYield = 0 ;
        vTaskDelay ( 1 ) ;                                           // Feed CPU0 idle/watchdog every 256 bytes
      }
    }
  }
  //vTaskDelete ( NULL ) ;                                          // Will never arrive here
}
#endif

#if defined(DEC_HELIX)
//**************************************************************************************************
//                               P L A Y T A S K ( I 2 S )                                         *
//**************************************************************************************************
// Play stream data from input queue. Version for I2S output or output to internal DAC.            *
// I2S output is suitable for a PCM5102A DAC.                                                      *
// Input are blocks with 32 bytes MP3/AAC data delivered in the data queue.                        *
// Internal ESP32 DAC (pin 25 and 26) is used when no pin BCK is configured.                       *
// Note that the naming of the data pin is somewhat confusing.  The data out pin in the pin        *
// configuration is called data_out_num, but this pin should be connected to the "DIN" pin of the  *
// external DAC.  The variable used to configure this pin is therefore called "i2s_din_pin".       *
// If no pin for i2s_bck is configured, output will be sent to the internal DAC.                   *
// Task will stop on OTA update.                                                                   *
//**************************************************************************************************
void playtask ( void * parameter )
{
  esp_err_t        pinss_err = ESP_FAIL ;                            // Result of i2s_set_pin
  i2s_config_t     i2s_config ;                                      // I2S configuration
  bool             playing = false ;                                 // Are we playing or not?

  memset ( &i2s_config, 0, sizeof(i2s_config) ) ;                    // Clear config struct
  i2s_config.mode                   = (i2s_mode_t)(I2S_MODE_MASTER | // I2S mode (5)
                                          I2S_MODE_TX) ;
  #ifdef DEC_HELIX_SPDIF
    i2s_config.use_apll               = true ;
    i2s_config.sample_rate            = 44100 * 2 ;                  // For spdif: biphase and 32 bits
    i2s_config.bits_per_sample        = I2S_BITS_PER_SAMPLE_32BIT ;  // and 32 bits
    #if ESP_ARDUINO_VERSION_MAJOR >= 2                               // New version?
      i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S ;  // Yes, use new definition
    #else
      i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB) ;
    #endif
  #else
    i2s_config.sample_rate            = 44100 ;                      // 44100
    i2s_config.bits_per_sample        = I2S_BITS_PER_SAMPLE_16BIT ;  // (16)
    #if ESP_ARDUINO_VERSION_MAJOR >= 2                               // New version?
      i2s_config.communication_format = I2S_COMM_FORMAT_STAND_MSB ;  // Yes, use new definition
    #else
      i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB) ;
    #endif
  #endif
  //i2s_config.channel_format     = I2S_CHANNEL_FMT_RIGHT_LEFT ;   // = 0
  i2s_config.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1 ;         // High interrupt priority
  i2s_config.dma_buf_count        = 12 ;
  i2s_config.dma_buf_len          = 256 ;
  i2s_config.tx_desc_auto_clear   = true ;                         // clear tx descriptor on underflow
  //i2s_config.fixed_mclk         = 0 ;                            // No (pin for) MCLK
  //i2s_config.mclk_multiple      = I2S_MCLK_MULTIPLE_DEFAULT ;    // = 0
  //i2s_config.bits_per_chan      = I2S_BITS_PER_CHAN_DEFAULT ;    // = 0
  #ifdef DEC_HELIX_INT
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER |               // Set I2S mode for internal DAC
                                   I2S_MODE_TX |                   // (4)
                                   I2S_MODE_DAC_BUILT_IN ) ;       // Enable internal DAC (16)
    #if ESP_ARDUINO_VERSION_MAJOR < 2
      i2s_config.communication_format = I2S_COMM_FORMAT_I2S_MSB ;
    #endif
  #endif
  ESP_LOGI ( TAG, "Starting I2S playtask.." ) ;
  #ifdef DEC_HELIX_AI                                              // For AI board?
    #define IIC_DATA 33                                            // Yes, use these I2C signals
    #define IIC_CLK  32
    if ( ! dac.begin ( IIC_DATA, IIC_CLK ) )                       // Initialize AI dac
    {
      ESP_LOGE ( TAG, "AI dac error!" ) ;
    }
    pinMode ( GPIO_PA_EN, OUTPUT ) ;
    digitalWrite ( GPIO_PA_EN, HIGH ) ;
  #endif
  MP3Decoder_AllocateBuffers() ;                                    // Init HELIX buffers
  AACDecoder_AllocateBuffers() ;                                    // Init HELIX buffers
  if ( i2s_driver_install ( I2S_NUM_0, &i2s_config, 0, NULL ) != ESP_OK )
  {
    ESP_LOGE ( TAG, "I2S install error!" ) ;
  }
  #ifdef DEC_HELIX_INT                                              // Use internal (8 bit) DAC?
    ESP_LOGI ( TAG, "Output to internal DAC" ) ;                    // Show output device
    pinss_err = i2s_set_pin ( I2S_NUM_0, NULL ) ;                   // Yes, default pins for internal DAC
    i2s_set_dac_mode ( I2S_DAC_CHANNEL_BOTH_EN ) ;
  #else
    i2s_pin_config_t pin_config ;
    #if ESP_ARDUINO_VERSION_MAJOR >= 2
      pin_config.mck_io_num   = I2S_PIN_NO_CHANGE ;                 // MCK not used
    #endif
    pin_config.data_in_num    = I2S_PIN_NO_CHANGE ;
    #ifdef DEC_HELIX_SPDIF
      pin_config.bck_io_num   = I2S_PIN_NO_CHANGE ;
      pin_config.ws_io_num    = I2S_PIN_NO_CHANGE ;
      pin_config.data_out_num = ini_block.i2s_spdif_pin ;
      pin_config.data_in_num  = I2S_PIN_NO_CHANGE ;
      ESP_LOGI ( TAG, "Output to SPDIF, pin %d",                    // Show pin used for output device
                 pin_config.data_out_num ) ;
    #else
      pin_config.bck_io_num   = ini_block.i2s_bck_pin ;             // This is BCK pin
      pin_config.ws_io_num    = ini_block.i2s_lck_pin ;             // This is L(R)CK pin
      pin_config.data_out_num = ini_block.i2s_din_pin ;             // This is DATA output pin
      ESP_LOGI ( TAG, "Output to I2S, pins %d, %d and %d",          // Show pins used for output device
                 pin_config.bck_io_num,                             // This is the BCK (bit clock) pin
                 pin_config.ws_io_num,                              // This is L(R)CK pin
                 pin_config.data_out_num ) ;                        // This is DATA output pin
    #endif
    pinss_err = i2s_set_pin ( I2S_NUM_0, &pin_config ) ;            // Set I2S pins
  #endif
  i2s_zero_dma_buffer ( I2S_NUM_0 ) ;                               // Zero the buffer
  if ( pinss_err != ESP_OK )                                        // Check error condition
  {
    ESP_LOGE ( TAG, "I2S setpin error!" ) ;                         // Rport bad pins
    while ( true)                                                   // Forever..
    {
      xQueueReceive ( dataqueue, &inchunk, 500 ) ;                  // Ignore all chunk from queue
    }
  }
  while ( true )
  {
    if ( xQueueReceive ( dataqueue, &inchunk, 5 ) == pdTRUE )       // Command/data from queue?
    {
      switch ( inchunk.datatyp )                                    // Yes, what kind of command?
      {
        case QDATA:
          if ( playing )                                            // Are we playing?
          {
            playChunk ( inchunk.buf ) ;                             // Play this chunk
          }
          totalcount += sizeof(inchunk.buf) ;                       // Count the bytes
          break ;
        case QSTARTSONG:
          ESP_LOGI ( TAG, "Playtask start song" ) ;
          playing = true ;                                          // Set local status to playing
          playingstat = 1 ;                                         // Status for MQTT
          mqttpub.trigger ( MQTT_PLAYING ) ;                        // Request publishing to MQTT
          helixInit ( ini_block.shutdown_pin,                       // Enable amplifier output
                      ini_block.shutdownx_pin ) ;                   // Init framebuffering
          break ;
        case QSTOPSONG:
          ESP_LOGI ( TAG, "Playtask stop song" ) ;
          playing = false ;                                         // Reset local play status
          playingstat = 0 ;                                         // Status for MQTT
          i2s_stop ( I2S_NUM_0 ) ;                                  // Stop DAC
          mqttpub.trigger ( MQTT_PLAYING ) ;                        // Request publishing to MQTT
          //vTaskDelay ( 500 / portTICK_PERIOD_MS ) ;               // Pause for a short time
          break ;
        case QSTOPTASK:
          ESP_LOGI ( TAG, "Stop Playtask" ) ;
          playing = false ;                                         // Reset local play status
          i2s_stop ( I2S_NUM_0 ) ;                                  // Stop DAC
          vTaskDelete ( NULL ) ;                                    // Stop task
          break ;
        default:
          break ;
      }
    }
  }
}
#endif


//**************************************************************************************************
//                                     S D F U N C S                                               *
//**************************************************************************************************
// Handles data of SD card and commands in the sdqueue.                                            *
// Commands are received in the input queue.                                                       *
//**************************************************************************************************
#ifdef SDCARD
static bool performSDSeek ( int16_t relativeSeconds )
{
  uint32_t playedSeconds, lengthSeconds ;
  uint8_t percent ;
  getSDProgress ( playedSeconds, lengthSeconds, percent ) ;
  if ( !SD_playing || !SD_totalbytes || !lengthSeconds ) return false ;

  int32_t targetSeconds = (int32_t)playedSeconds + relativeSeconds ;
  if ( targetSeconds < 0 ) targetSeconds = 0 ;
  if ( targetSeconds >= (int32_t)lengthSeconds ) targetSeconds = lengthSeconds - 1 ;
  uint32_t targetByte = ( targetSeconds * (uint64_t)SD_totalbytes ) / lengthSeconds ;
  uint32_t absolute = SD_audio_start + targetByte ;

  queueToPt ( QSTOPSONG ) ;                              // Flush already buffered old-position audio
  static uint8_t frameProbe[2048] ;
  if ( !mp3file.seek ( absolute ) ) return false ;
  size_t probeLength = mp3file.read ( frameProbe, sizeof(frameProbe) ) ;
  size_t frameOffset = 0 ;
  bool frameFound = false ;
  for ( size_t i = 0 ; i + 3 < probeLength ; i++ )
  {
    uint8_t version = ( frameProbe[i + 1] >> 3 ) & 0x03 ;
    uint8_t layer = ( frameProbe[i + 1] >> 1 ) & 0x03 ;
    uint8_t bitrateIndex = ( frameProbe[i + 2] >> 4 ) & 0x0F ;
    uint8_t sampleIndex = ( frameProbe[i + 2] >> 2 ) & 0x03 ;
    if ( frameProbe[i] == 0xFF && ( frameProbe[i + 1] & 0xE0 ) == 0xE0 &&
         version != 1 && layer == 1 && bitrateIndex > 0 && bitrateIndex < 15 &&
         sampleIndex < 3 )
    {
      frameOffset = i ;
      frameFound = true ;
      break ;
    }
  }
  uint32_t newPosition = absolute + ( frameFound ? frameOffset : 0 ) ;
  if ( !mp3file.seek ( newPosition ) ) return false ;
  uint32_t fileSize = mp3file.size() ;
  mp3filelength = newPosition < fileSize ? fileSize - newPosition : 0 ;
  displayplaytime ( "" ) ;                                // Force changed digits to redraw immediately
  queueToPt ( QSTARTSONG ) ;                              // Restart decoder on the aligned MP3 frame
  ESP_LOGI ( TAG, "MP3 seek %+d s -> %lu s, byte %lu%s",
             relativeSeconds, (unsigned long)targetSeconds,
             (unsigned long)newPosition, frameFound ? "" : " (frame not found)" ) ;
  return true ;
}
#endif


void sdfuncs()
{
#ifdef SDCARD
  qdata_type          sdcmd ;                                     // Command from sdqueue
  static bool         openfile = false ;                          // Open input file available
  static bool         autoplay = true ;                           // Play next after end
  size_t              n ;                                         // Number of bytes read from SD

  if ( openfile && SD_seek_seconds )
  {
    int16_t seconds = SD_seek_seconds ;
    SD_seek_seconds = 0 ;
    if ( !performSDSeek ( seconds ) )
    {
      ESP_LOGI ( TAG, "MP3 seek ignored: duration is unknown" ) ;
    }
  }
  if ( openfile )
  {
    while ( ( mp3filelength > 0 ) &&                              // Read until eof or dataqueue full
            ( uxQueueSpacesAvailable ( dataqueue ) > 0 ) )
    {
      n = mp3file.read ( outchunk.buf, sizeof(outchunk.buf) ) ;   // Read a block of data
      if ( n < sizeof(outchunk.buf) )                             // Incomplete chunk?
      {
        memset ( outchunk.buf + n, 0,                             // Yes, clear rest
                 sizeof(outchunk.buf) - n ) ;
      }
      xQueueSend ( dataqueue, &outchunk, 0 ) ;                    // Send to queue
      mp3filelength -= n ;                                        // Compute rest in file
      if ( mp3filelength == 0 )                                   // End of file?
      {
        vTaskDelay ( 500 / portTICK_PERIOD_MS ) ;                 // Give some time to finish song
        myQueueSend ( sdqueue, &stopcmd ) ;                       // Stop message to myself
        ESP_LOGI ( TAG, "EOF" ) ;
        queueToPt ( QSTOPSONG ) ;                                 // Tell playtask to stop song
        if ( autoplay )                                           // Continue with next track?
        {
          ESP_LOGI ( TAG, "Autoplay next track" ) ;
          getNextSDFileName() ;                                   // Select next track
          myQueueSend ( sdqueue, &startcmd ) ;                    // Start message to myself
        }
      }
    }
  }
  if ( xQueueReceive ( sdqueue, &sdcmd, 0 ) == pdTRUE )           // New command in queue?
  {
    ESP_LOGI ( TAG, "SDfuncs cmd is %d", sdcmd ) ;
    switch ( sdcmd )                                              // Yes, examine command
    {
      case QSTARTSONG:                                            // Start a new song?
        if ( openfile )                                           // Still playing?
        {
          close_SDCARD() ;                                        // Clode file
        }
        queueToPt ( QSTOPSONG ) ;                                 // Tell playtask to stop song
        if ( ( openfile = connecttofile_SD() ) )                  // Yes, connect to file, set mp3filelength
        {
          ESP_LOGI ( TAG, "File opened, track = %s",
                     getCurrentSDFileName() ) ;
          ESP_LOGI ( TAG, "File length is %d", mp3filelength ) ;
          audio_ct = String ( "audio/mpeg" ) ;                    // Force mp3 mode
          myQueueSend ( radioqueue, &stopcmd ) ;                  // Stop playing icecast station
          radiofuncs() ;                                          // Allow radiofuncs to react
          queueToPt ( QSTARTSONG ) ;                              // Tell playtask
          autoplay = true ;                                       // Set autoplay mode
        }
        else
        {
          ESP_LOGI ( TAG, "Error opening file" ) ;
        }
        break ;
      case QSTOPSONG:                                             // Stop the song?
        if ( openfile )                                           // Still playing?
        {
          close_SDCARD() ;                                        // Clode file
          mp3filelength = 0 ;                                     // Yes, force end of file
          autoplay = false ;                                      // Stop autoplay
        }
      default:
        break ;
    }
  }
#endif
}
