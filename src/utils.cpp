// utils.cpp
// Some useful utilities.
//
#include "Arduino.h"
#include "utils.h"
//#include <stdarg.h>
//#include <stdio.h>


//**************************************************************************************************
//                                     G E T E N C R Y P T I O N T Y P E                           *
//**************************************************************************************************
// Read the encryption type of the network and return as a 4 byte name                             *
//**************************************************************************************************
const char* getEncryptionType ( wifi_auth_mode_t thisType )
{
  switch ( thisType )
  {
    case WIFI_AUTH_OPEN:
      return "OPEN" ;
    case WIFI_AUTH_WEP:
      return "WEP" ;
    case WIFI_AUTH_WPA_PSK:
      return "WPA_PSK" ;
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2_PSK" ;
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA_WPA2_PSK" ;
    case WIFI_AUTH_MAX:
      return "MAX" ;
    default:
      break ;
  }
  return "????" ;
}


//**************************************************************************************************
//                                     G E T C O N T E N T T Y P E                                 *
//**************************************************************************************************
// Returns the contenttype of a file to send to a client.                                          *
// Note that the strstr function searches for the firsty occurence.  So the function will fail for *
// filenames like "abc.html.ico".                                                                  *
//**************************************************************************************************
String getContentType ( const char* filename )
{
  if ( strstr ( filename, ".html" ) ) return "text/html" ;
  if ( strstr ( filename, ".png"  ) ) return "image/png" ;
  if ( strstr ( filename, ".gif"  ) ) return "image/gif" ;
  if ( strstr ( filename, ".jpg"  ) ) return "image/jpeg" ;
  if ( strstr ( filename, ".ico"  ) ) return "image/x-icon" ;
  if ( strstr ( filename, ".css"  ) ) return "text/css" ;
  if ( strstr ( filename, ".mp3"  ) ) return "audio/mpeg" ;
  if ( strstr ( filename, ".js"   ) ) return "text/javascript" ;
  return "text/plain" ;
}


//**************************************************************************************************
//                                      U T F 8 A S C I I                                          *
//**************************************************************************************************
// UTF8-Decoder: convert UTF8-string to extended ASCII.                                            *
// Convert a single Character from UTF8 to Extended ASCII.                                         *
// Return "0" if a byte has to be ignored.                                                         *
//**************************************************************************************************
char utf8ascii ( char ascii )
{
  static const char lut_C3[] = { "AAAAAAACEEEEIIIIDNOOOOO#0UUUU###"
                                 "aaaaaaaceeeeiiiidnooooo##uuuuyyy" } ; 
  static const char lut_C4[] = { "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGg"
                                 "GgGgHhHhIiIiIiIiIiJjJjKkkLlLlLlL" } ;
  static const char lut_C5[] = { "lLlNnNnNnnnnOoOoOoOoRrRrRrSsSsSs"
                                 "SsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs" } ;

  static char       c1 ;              // Last character buffer
  char              res = '\0' ;      // Result, default 0

  if ( ascii <= 0x7F )                // Standard ASCII-set 0..0x7F handling
  {
    c1 = 0 ;
    res = ascii ;                     // Return unmodified
  }
  else
  {
    switch ( c1 )                     // Conversion depending on first UTF8-character
    {
      case 0xC2: res = '~' ;
        break ;
      case 0xC3: res = lut_C3[ascii - 128] ;
        break ;
      case 0xC4: res = lut_C4[ascii - 128] ;
        break ;
      case 0xC5: res = lut_C5[ascii - 128] ;
        break ;
      case 0x82: if ( ascii == 0xAC )
        {
          res = 'E' ;                 // Special case Euro-symbol
        }
    }
    c1 = ascii ;                      // Remember actual character
  }
  return res ;                        // Otherwise: return zero, if character has to be ignored
}


//**************************************************************************************************
//                                U T F 8 A S C I I _ I P                                          *
//**************************************************************************************************
// In Place conversion UTF8-string to Extended ASCII (ASCII is shorter!).                          *
//**************************************************************************************************
void utf8ascii_ip ( char* s )
{
  int  i, k = 0 ;                     // Indexes for in en out string
  char c ;

  for ( i = 0 ; s[i] ; i++ )          // For every input character
  {
    c = utf8ascii ( s[i] ) ;          // Translate if necessary
    if ( c )                          // Good translation?
    {
      s[k++] = c ;                    // Yes, put in output string
    }
  }
  s[k] = 0 ;                          // Take care of delimeter
}


//**************************************************************************************************
//                                      U T F 8 A S C I I                                          *
//**************************************************************************************************
// Conversion UTF8-String to Extended ASCII String.                                                *
//**************************************************************************************************
String utf8ascii ( const char* s )
{
  int  i ;                            // Index for input string
  char c ;
  String res = "" ;                   // Result string

  for ( i = 0 ; s[i] ; i++ )          // For every input character
  {
    c = utf8ascii ( s[i] ) ;          // Translate if necessary
    if ( c )                          // Good translation?
    {
      res += String ( c ) ;           // Yes, put in output string
    }
  }
  return res ;
}


//**************************************************************************************************
//                                  D E C O D E S T R E A M T E X T                               *
//**************************************************************************************************
// ICY metadata is a byte string.  Keep UTF-8 intact and convert explicitly announced legacy       *
// encodings to UTF-8 before the text reaches the display, web interface or MQTT.                   *
//**************************************************************************************************
static const uint16_t iso8859_2[] PROGMEM = {
  0x0080,0x0081,0x0082,0x0083,0x0084,0x0085,0x0086,0x0087,
  0x0088,0x0089,0x008A,0x008B,0x008C,0x008D,0x008E,0x008F,
  0x0090,0x0091,0x0092,0x0093,0x0094,0x0095,0x0096,0x0097,
  0x0098,0x0099,0x009A,0x009B,0x009C,0x009D,0x009E,0x009F,
  0x00A0,0x0104,0x02D8,0x0141,0x00A4,0x013D,0x015A,0x00A7,
  0x00A8,0x0160,0x015E,0x0164,0x0179,0x00AD,0x017D,0x017B,
  0x00B0,0x0105,0x02DB,0x0142,0x00B4,0x013E,0x015B,0x02C7,
  0x00B8,0x0161,0x015F,0x0165,0x017A,0x02DD,0x017E,0x017C,
  0x0154,0x00C1,0x00C2,0x0102,0x00C4,0x0139,0x0106,0x00C7,
  0x010C,0x00C9,0x0118,0x00CB,0x011A,0x00CD,0x00CE,0x010E,
  0x0110,0x0143,0x0147,0x00D3,0x00D4,0x0150,0x00D6,0x00D7,
  0x0158,0x016E,0x00DA,0x0170,0x00DC,0x00DD,0x0162,0x00DF,
  0x0155,0x00E1,0x00E2,0x0103,0x00E4,0x013A,0x0107,0x00E7,
  0x010D,0x00E9,0x0119,0x00EB,0x011B,0x00ED,0x00EE,0x010F,
  0x0111,0x0144,0x0148,0x00F3,0x00F4,0x0151,0x00F6,0x00F7,
  0x0159,0x016F,0x00FA,0x0171,0x00FC,0x00FD,0x0163,0x02D9
} ;

static const uint16_t windows1250[] PROGMEM = {
  0x20AC,0xFFFD,0x201A,0xFFFD,0x201E,0x2026,0x2020,0x2021,
  0xFFFD,0x2030,0x0160,0x2039,0x015A,0x0164,0x017D,0x0179,
  0xFFFD,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
  0xFFFD,0x2122,0x0161,0x203A,0x015B,0x0165,0x017E,0x017A,
  0x00A0,0x02C7,0x02D8,0x0141,0x00A4,0x0104,0x00A6,0x00A7,
  0x00A8,0x00A9,0x015E,0x00AB,0x00AC,0x00AD,0x00AE,0x017B,
  0x00B0,0x00B1,0x02DB,0x0142,0x00B4,0x00B5,0x00B6,0x00B7,
  0x00B8,0x0105,0x015F,0x00BB,0x013D,0x02DD,0x013E,0x017C,
  0x0154,0x00C1,0x00C2,0x0102,0x00C4,0x0139,0x0106,0x00C7,
  0x010C,0x00C9,0x0118,0x00CB,0x011A,0x00CD,0x00CE,0x010E,
  0x0110,0x0143,0x0147,0x00D3,0x00D4,0x0150,0x00D6,0x00D7,
  0x0158,0x016E,0x00DA,0x0170,0x00DC,0x00DD,0x0162,0x00DF,
  0x0155,0x00E1,0x00E2,0x0103,0x00E4,0x013A,0x0107,0x00E7,
  0x010D,0x00E9,0x0119,0x00EB,0x011B,0x00ED,0x00EE,0x010F,
  0x0111,0x0144,0x0148,0x00F3,0x00F4,0x0151,0x00F6,0x00F7,
  0x0159,0x016F,0x00FA,0x0171,0x00FC,0x00FD,0x0163,0x02D9
} ;

static const uint16_t windows1252[] PROGMEM = {
  0x20AC,0xFFFD,0x201A,0x0192,0x201E,0x2026,0x2020,0x2021,
  0x02C6,0x2030,0x0160,0x2039,0x0152,0xFFFD,0x017D,0xFFFD,
  0xFFFD,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
  0x02DC,0x2122,0x0161,0x203A,0x0153,0xFFFD,0x017E,0x0178,
  0x00A0,0x00A1,0x00A2,0x00A3,0x00A4,0x00A5,0x00A6,0x00A7,
  0x00A8,0x00A9,0x00AA,0x00AB,0x00AC,0x00AD,0x00AE,0x00AF,
  0x00B0,0x00B1,0x00B2,0x00B3,0x00B4,0x00B5,0x00B6,0x00B7,
  0x00B8,0x00B9,0x00BA,0x00BB,0x00BC,0x00BD,0x00BE,0x00BF,
  0x00C0,0x00C1,0x00C2,0x00C3,0x00C4,0x00C5,0x00C6,0x00C7,
  0x00C8,0x00C9,0x00CA,0x00CB,0x00CC,0x00CD,0x00CE,0x00CF,
  0x00D0,0x00D1,0x00D2,0x00D3,0x00D4,0x00D5,0x00D6,0x00D7,
  0x00D8,0x00D9,0x00DA,0x00DB,0x00DC,0x00DD,0x00DE,0x00DF,
  0x00E0,0x00E1,0x00E2,0x00E3,0x00E4,0x00E5,0x00E6,0x00E7,
  0x00E8,0x00E9,0x00EA,0x00EB,0x00EC,0x00ED,0x00EE,0x00EF,
  0x00F0,0x00F1,0x00F2,0x00F3,0x00F4,0x00F5,0x00F6,0x00F7,
  0x00F8,0x00F9,0x00FA,0x00FB,0x00FC,0x00FD,0x00FE,0x00FF
} ;

static const uint16_t dos852[] PROGMEM = {
  0x00C7,0x00FC,0x00E9,0x00E2,0x00E4,0x016F,0x0107,0x00E7,
  0x0142,0x00EB,0x0150,0x0151,0x00EE,0x0179,0x00C4,0x0106,
  0x00C9,0x0139,0x013A,0x00F4,0x00F6,0x013D,0x013E,0x015A,
  0x015B,0x00D6,0x00DC,0x0164,0x0165,0x0141,0x00D7,0x010D,
  0x00E1,0x00ED,0x00F3,0x00FA,0x0104,0x0105,0x017D,0x017E,
  0x0118,0x0119,0x00AC,0x017A,0x010C,0x015F,0x00AB,0x00BB,
  0x2591,0x2592,0x2593,0x2502,0x2524,0x00C1,0x00C2,0x011A,
  0x015E,0x2563,0x2551,0x2557,0x255D,0x017B,0x017C,0x2510,
  0x2514,0x2534,0x252C,0x251C,0x2500,0x253C,0x0102,0x0103,
  0x255A,0x2554,0x2569,0x2566,0x2560,0x2550,0x256C,0x00A4,
  0x0111,0x0110,0x010E,0x00CB,0x010F,0x0147,0x00CD,0x00CE,
  0x011B,0x2518,0x250C,0x2588,0x2584,0x0162,0x016E,0x2580,
  0x00D3,0x00DF,0x00D4,0x0143,0x0144,0x0148,0x0160,0x0161,
  0x0154,0x00DA,0x0155,0x0170,0x00FD,0x00DD,0x0163,0x00B4,
  0x00AD,0x02DD,0x02DB,0x02C7,0x02D8,0x00A7,0x00F7,0x00B8,
  0x00B0,0x00A8,0x02D9,0x0171,0x0158,0x0159,0x25A0,0x00A0
} ;

bool isValidUtf8 ( const String& text )
{
  const uint8_t* p = (const uint8_t*) text.c_str() ;
  while ( *p )
  {
    if ( *p < 0x80 )
    {
      p++ ;
      continue ;
    }
    uint8_t n = ( *p >= 0xC2 && *p <= 0xDF ) ? 1 :
                ( *p >= 0xE0 && *p <= 0xEF ) ? 2 :
                ( *p >= 0xF0 && *p <= 0xF4 ) ? 3 : 0 ;
    if ( !n ) return false ;
    uint8_t lead = *p++ ;
    for ( uint8_t i = 0 ; i < n ; i++ )
    {
      if ( ( *p & 0xC0 ) != 0x80 ) return false ;
      if ( i == 0 && ( ( lead == 0xE0 && *p < 0xA0 ) ||
                       ( lead == 0xED && *p >= 0xA0 ) ||
                       ( lead == 0xF0 && *p < 0x90 ) ||
                       ( lead == 0xF4 && *p >= 0x90 ) ) ) return false ;
      p++ ;
    }
  }
  return true ;
}

static void appendUtf8 ( String& result, uint32_t cp )
{
  if ( cp < 0x80 )
  {
    result += (char)cp ;
  }
  else if ( cp < 0x800 )
  {
    result += (char)( 0xC0 | ( cp >> 6 ) ) ;
    result += (char)( 0x80 | ( cp & 0x3F ) ) ;
  }
  else if ( cp < 0x10000 )
  {
    result += (char)( 0xE0 | ( cp >> 12 ) ) ;
    result += (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) ) ;
    result += (char)( 0x80 | ( cp & 0x3F ) ) ;
  }
  else if ( cp <= 0x10FFFF )
  {
    result += (char)( 0xF0 | ( cp >> 18 ) ) ;
    result += (char)( 0x80 | ( ( cp >> 12 ) & 0x3F ) ) ;
    result += (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) ) ;
    result += (char)( 0x80 | ( cp & 0x3F ) ) ;
  }
}

String utf8Codepoint ( uint32_t codepoint )
{
  String result ;
  appendUtf8 ( result, codepoint ) ;
  return result ;
}

String decodeStreamText ( const String& text, const String& charset )
{
  String enc = charset ;
  enc.toLowerCase() ;
  enc.trim() ;
  enc.replace ( "_", "-" ) ;
  if ( enc.isEmpty() )
  {
    if ( isValidUtf8 ( text ) ) return text ;
    enc = "windows-1252" ;                 // ICY's historic one-byte fallback
  }
  if ( enc == "utf-8" || enc == "utf8" || enc == "us-ascii" ) return text ;

  const uint16_t* table = NULL ;
  if ( enc == "iso-8859-2" || enc == "iso8859-2" || enc == "latin2" ) table = iso8859_2 ;
  else if ( enc == "windows-1250" || enc == "cp1250" || enc == "win-1250" ) table = windows1250 ;
  else if ( enc == "windows-1252" || enc == "cp1252" || enc == "win-1252" ) table = windows1252 ;
  else if ( enc == "cp852" || enc == "ibm852" || enc == "dos-852" ) table = dos852 ;

  String result ;
  result.reserve ( text.length() * 2 + 1 ) ;
  const uint8_t* p = (const uint8_t*) text.c_str() ;
  while ( *p )
  {
    uint16_t cp = *p++ ;
    if ( cp >= 0x80 )
    {
      if ( table ) cp = pgm_read_word ( table + cp - 0x80 ) ;
      // ISO-8859-1 and unknown declared single-byte encodings keep U+0080..U+00FF.
    }
    appendUtf8 ( result, cp ) ;
  }
  return result ;
}


//**************************************************************************************************
//                                    P I N _ E X I S T S                                          *
//**************************************************************************************************
// Checks if GPIO pin exists.                                                                      *
//**************************************************************************************************
bool pin_exists ( uint8_t pin )
{
  #ifdef CONFIG_IDF_TARGET_ESP32
    return ( pin <= 39 ) ;
  #endif
  #ifdef CONFIG_IDF_TARGET_ESP32S3
    return ( pin <= 48 ) ;
  #endif
}

