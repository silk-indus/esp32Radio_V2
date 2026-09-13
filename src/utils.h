// utils.h
// Some useful utilities.
//
#ifndef UTILS_H
#define UTILS_H
#include <esp_wifi_types.h>

char        utf8ascii ( char ascii ) ;                             // Convert UTF to Ascii
void        utf8ascii_ip ( char* s ) ;                             // Convert UTF to Ascii in place
String      utf8ascii ( const char* s ) ;                          // Convert UTF to Ascii as String
bool        isValidUtf8 ( const String& text ) ;                    // Validate a UTF-8 string
String      utf8Codepoint ( uint32_t codepoint ) ;                  // Encode one Unicode codepoint
String      decodeStreamText ( const String& text,                  // Convert stream text to UTF-8
                               const String& charset = "" ) ;
const char* getEncryptionType ( wifi_auth_mode_t thisType ) ;      // Get encryption type voor WiFi networks
String      getContentType ( const char* ) ;
bool        pin_exists ( uint8_t pin ) ;                           // Check GPIO pin number

#endif
