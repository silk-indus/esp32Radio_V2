//**************************************************************************************************
// bluetft.cpp                                                                                     *
//**************************************************************************************************
// Separated from the main sketch to allow several display types.                                  *
// Includes for various ST7735 displays.  Size is 160 x 128.  Select INITR_BLACKTAB                *
// for this and set dsp_getwidth() to 160.                                                         *
// Works also for the 128 x 128 version.  Select INITR_144GREENTAB for this and                    *
// set dsp_getwidth() to 128.                                                                      *
//**************************************************************************************************
#include "bluetft.h"
#include <SPIFFS.h>

namespace
{
  struct SmoothGlyph
  {
    uint32_t bitmapOffset ;
    uint16_t unicode ;
    uint8_t  height ;
    uint8_t  width ;
    uint8_t  advance ;
    int8_t   deltaY ;
    int8_t   deltaX ;
  } ;

  uint32_t read32be ( File& file )
  {
    uint8_t b[4] ;
    if ( file.read ( b, sizeof ( b ) ) != sizeof ( b ) ) return 0 ;
    return ( (uint32_t)b[0] << 24 ) | ( (uint32_t)b[1] << 16 ) |
           ( (uint32_t)b[2] << 8 ) | b[3] ;
  }

  class SmoothFont
  {
    public:
      SmoothFont() : glyphs ( NULL ), glyphCount ( 0 ), fontSize ( 0 ),
                     maxAscent ( 0 ), maxDescent ( 0 ) {}

      bool load ( const char* fileName )
      {
        path = fileName ;
        File file = SPIFFS.open ( path, FILE_READ ) ;
        if ( !file ) return false ;
        uint32_t count = read32be ( file ) ;
        uint32_t version = read32be ( file ) ;
        fontSize = read32be ( file ) ;
        read32be ( file ) ;                              // Reserved field
        read32be ( file ) ;                              // Nominal ascent
        read32be ( file ) ;                              // Descent
        if ( !count || count > 2048 || version != 11 )
        {
          file.close() ;
          return false ;
        }
        glyphs = new SmoothGlyph[count] ;
        if ( !glyphs )
        {
          file.close() ;
          return false ;
        }
        glyphCount = count ;
        uint32_t bitmap = 24 + glyphCount * 28 ;
        for ( uint16_t i = 0 ; i < glyphCount ; i++ )
        {
          SmoothGlyph& g = glyphs[i] ;
          g.unicode = (uint16_t)read32be ( file ) ;
          g.height = (uint8_t)read32be ( file ) ;
          g.width = (uint8_t)read32be ( file ) ;
          g.advance = (uint8_t)read32be ( file ) ;
          g.deltaY = (int8_t)(int32_t)read32be ( file ) ;
          g.deltaX = (int8_t)(int32_t)read32be ( file ) ;
          read32be ( file ) ;                            // Reserved field
          g.bitmapOffset = bitmap ;
          bitmap += (uint32_t)g.width * g.height ;
          if ( g.deltaY > maxAscent ) maxAscent = g.deltaY ;
          int8_t descent = g.height - g.deltaY ;
          if ( descent > maxDescent ) maxDescent = descent ;
        }
        file.close() ;
        return true ;
      }

      bool ready() const { return glyphs != NULL ; }
      uint8_t lineHeight() const { return maxAscent + maxDescent ; }
      uint8_t spaceWidth() const { return ( fontSize + 2 ) / 4 ; }

      uint16_t textWidth ( const char* text ) const
      {
        uint16_t width = 0 ;
        const char* p = text ;
        while ( p && *p )
        {
          uint16_t cp = nextCodepoint ( p ) ;
          const SmoothGlyph* glyph = find ( cp ) ;
          width += glyph ? glyph->advance : spaceWidth() ;
        }
        return width ;
      }

      uint16_t requiredLines ( const char* text, uint16_t maxWidth ) const
      {
        if ( !text || !*text || !maxWidth ) return 0 ;
        uint16_t lines = 1, used = 0 ;
        bool pendingSpace = false ;
        const char* p = text ;
        while ( *p )
        {
          const char* word = p ;
          uint16_t cp = nextCodepoint ( p ) ;
          if ( cp == '\r' ) continue ;
          if ( cp == '\n' )
          {
            lines++ ;
            used = 0 ;
            pendingSpace = false ;
            continue ;
          }
          if ( cp == ' ' || cp == '\t' )
          {
            if ( used ) pendingSpace = true ;
            continue ;
          }

          const char* wordEnd = p ;
          uint16_t wordWidth = glyphAdvance ( cp ) ;
          while ( *wordEnd )
          {
            const char* next = wordEnd ;
            uint16_t nextCp = nextCodepoint ( next ) ;
            if ( nextCp == '\r' || nextCp == '\n' ||
                 nextCp == ' ' || nextCp == '\t' ) break ;
            wordWidth += glyphAdvance ( nextCp ) ;
            wordEnd = next ;
          }

          uint16_t gap = pendingSpace && used ? spaceWidth() : 0 ;
          if ( used && used + gap + wordWidth > maxWidth )
          {
            lines++ ;
            used = 0 ;
            gap = 0 ;
          }
          used += gap ;

          const char* q = word ;
          while ( q < wordEnd )
          {
            uint16_t advance = glyphAdvance ( nextCodepoint ( q ) ) ;
            if ( used && used + advance > maxWidth )
            {
              lines++ ;
              used = 0 ;
            }
            used += advance ;
          }
          p = wordEnd ;
          pendingSpace = false ;
        }
        return lines ;
      }

      bool fits ( const char* text, uint16_t maxWidth, uint16_t maxHeight ) const
      {
        uint16_t lines = requiredLines ( text, maxWidth ) ;
        return lines && lines * lineHeight() <= maxHeight ;
      }

      const SmoothGlyph* find ( uint16_t codepoint ) const
      {
        int low = 0, high = glyphCount - 1 ;
        while ( low <= high )
        {
          int mid = ( low + high ) / 2 ;
          if ( glyphs[mid].unicode == codepoint ) return &glyphs[mid] ;
          if ( glyphs[mid].unicode < codepoint ) low = mid + 1 ;
          else high = mid - 1 ;
        }
        return NULL ;
      }

      void draw ( Adafruit_ST7735& tft, const char* text, int16_t left, int16_t top,
                  uint16_t maxWidth, uint16_t maxHeight, uint16_t color,
                  uint16_t background = BLACK ) const
      {
        if ( !ready() || !text ) return ;
        File file = SPIFFS.open ( path, FILE_READ ) ;
        if ( !file ) return ;
        int16_t x = left, y = top ;
        const char* p = text ;
        bool pendingSpace = false ;
        tft.startWrite() ;
        while ( *p && y + lineHeight() <= top + maxHeight )
        {
          const char* word = p ;
          uint16_t cp = nextCodepoint ( p ) ;
          if ( cp == '\r' ) continue ;
          if ( cp == '\n' )
          {
            x = left ;
            y += lineHeight() ;
            pendingSpace = false ;
            continue ;
          }
          if ( cp == ' ' || cp == '\t' )
          {
            if ( x != left ) pendingSpace = true ;
            continue ;
          }

          const char* wordEnd = p ;
          uint16_t wordWidth = glyphAdvance ( cp ) ;
          while ( *wordEnd )
          {
            const char* next = wordEnd ;
            uint16_t nextCp = nextCodepoint ( next ) ;
            if ( nextCp == '\r' || nextCp == '\n' ||
                 nextCp == ' ' || nextCp == '\t' ) break ;
            wordWidth += glyphAdvance ( nextCp ) ;
            wordEnd = next ;
          }

          uint16_t gap = pendingSpace && x != left ? spaceWidth() : 0 ;
          if ( x != left && x + gap + wordWidth > left + maxWidth )
          {
            x = left ;
            y += lineHeight() ;
            gap = 0 ;
          }
          if ( y + lineHeight() > top + maxHeight ) break ;
          x += gap ;

          const char* q = word ;
          while ( q < wordEnd )
          {
            uint16_t wordCp = nextCodepoint ( q ) ;
            const SmoothGlyph* glyph = find ( wordCp ) ;
            if ( !glyph ) glyph = find ( '?' ) ;
            uint8_t advance = glyph ? glyph->advance : spaceWidth() ;
            if ( x != left && x + advance > left + maxWidth )
            {
              x = left ;
              y += lineHeight() ;
              if ( y + lineHeight() > top + maxHeight ) break ;
            }
            if ( glyph ) drawGlyph ( tft, file, *glyph, x, y, color, background ) ;
            x += advance ;
          }
          if ( y + lineHeight() > top + maxHeight ) break ;
          p = wordEnd ;
          pendingSpace = false ;
        }
        tft.endWrite() ;
        file.close() ;
      }

    private:
      uint8_t glyphAdvance ( uint16_t cp ) const
      {
        const SmoothGlyph* glyph = find ( cp ) ;
        if ( !glyph && cp != ' ' ) glyph = find ( '?' ) ;
        return glyph ? glyph->advance : spaceWidth() ;
      }

      static uint16_t nextCodepoint ( const char*& p )
      {
        uint8_t c = (uint8_t)*p++ ;
        if ( c < 0x80 ) return c ;
        if ( ( c & 0xE0 ) == 0xC0 && ( (uint8_t)*p & 0xC0 ) == 0x80 )
        {
          uint16_t cp = ( ( c & 0x1F ) << 6 ) | ( (uint8_t)*p++ & 0x3F ) ;
          return cp ;
        }
        if ( ( c & 0xF0 ) == 0xE0 &&
             ( (uint8_t)p[0] & 0xC0 ) == 0x80 &&
             ( (uint8_t)p[1] & 0xC0 ) == 0x80 )
        {
          uint8_t b1 = (uint8_t)*p++ ;
          uint8_t b2 = (uint8_t)*p++ ;
          uint16_t cp = ( ( c & 0x0F ) << 12 ) |
                        ( ( b1 & 0x3F ) << 6 ) |
                        ( b2 & 0x3F ) ;
          return cp ;
        }
        while ( ( (uint8_t)*p & 0xC0 ) == 0x80 ) p++ ;
        return '?' ;
      }

      static uint16_t blend ( uint16_t color, uint16_t background, uint8_t alpha )
      {
        if ( alpha == 255 ) return color ;
        uint16_t inverse = 255 - alpha ;
        uint16_t r = ( ( ( color >> 11 ) & 0x1F ) * alpha +
                       ( ( background >> 11 ) & 0x1F ) * inverse + 127 ) / 255 ;
        uint16_t g = ( ( ( color >> 5 ) & 0x3F ) * alpha +
                       ( ( background >> 5 ) & 0x3F ) * inverse + 127 ) / 255 ;
        uint16_t b = ( ( color & 0x1F ) * alpha +
                       ( background & 0x1F ) * inverse + 127 ) / 255 ;
        return ( r << 11 ) | ( g << 5 ) | b ;
      }

      void drawGlyph ( Adafruit_ST7735& tft, File& file, const SmoothGlyph& glyph,
                       int16_t x, int16_t y, uint16_t color,
                       uint16_t background ) const
      {
        if ( !file.seek ( glyph.bitmapOffset ) ) return ;
        int16_t gx = x + glyph.deltaX ;
        int16_t gy = y + maxAscent - glyph.deltaY ;
        for ( uint8_t row = 0 ; row < glyph.height ; row++ )
        {
          for ( uint8_t col = 0 ; col < glyph.width ; col++ )
          {
            int alpha = file.read() ;
            if ( alpha > 0 ) tft.writePixel ( gx + col, gy + row,
                                              blend ( color, background, alpha ) ) ;
          }
        }
      }

      String path ;
      SmoothGlyph* glyphs ;
      uint16_t glyphCount ;
      uint8_t fontSize ;
      uint8_t maxAscent ;
      uint8_t maxDescent ;
  } ;

  SmoothFont fontArial12 ;
  SmoothFont fontArialBold11 ;
  SmoothFont fontArialBold16 ;
  SmoothFont fontArialBold8 ;
  SmoothFont fontNums ;
}

Adafruit_ST7735*     bluetft_tft ;                          // For instance of display driver
scrseg_struct        bluetft_tftdata[TFTSECS] =             // Screen divided in 3 segments + 1 overlay
                      {
                        { false, WHITE,   0, 14, "" },      // Name and clock, small smooth font
                        { false, CYAN,   16, 64, "" },      // Artist/title, 3 lines
                        { false, YELLOW, 84, 42, "" },      // Station, 3 lines
                        { false, GREEN,  84, 42, "" }       // Rotary encoder overlay
                      } ;


bool bluetft_dsp_begin ( int8_t cs, int8_t dc )
{
  if ( cs < 0 || dc < 0 )
  {
    return false ;                                                  // Wrong pin configuration
  }
  if ( ( bluetft_tft = new Adafruit_ST7735 ( cs, dc , -1 ) ) )      // Create an instant for TFT
  {
    // Uncomment one of the following initR lines for ST7735R displays
    //tft->initR ( INITR_GREENTAB ) ;                               // Init TFT interface
    //tft->initR ( INITR_REDTAB ) ;                                 // Init TFT interface
    bluetft_tft->initR ( INITR_BLACKTAB ) ;                         // Init TFT interface
    //tft->initR ( INITR_144GREENTAB ) ;                            // Init TFT interface
    //tft->initR ( INITR_MINI160x80 ) ;                             // Init TFT interface
    //tft->initR ( INITR_BLACKTAB ) ;                               // Init TFT interface (160x128)
    // Uncomment the next line for ST7735B displays
    //tft_initB() ;
    fontArial12.load ( "/arial12.vlw" ) ;
    fontArialBold11.load ( "/arialbd11.vlw" ) ;
    fontArialBold16.load ( "/arialbd16.vlw" ) ;
    fontArialBold8.load ( "/arialbd8.vlw" ) ;
    fontNums.load ( "/nums.vlw" ) ;
  }
  return ( bluetft_tft != NULL ) ;
}


//**************************************************************************************************
//                                      D I S P L A Y B A T T E R Y                                *
//**************************************************************************************************
// Show the current battery charge level on the screen.                                            *
// It will overwrite the top divider.                                                              *
// No action if bat0/bat100 not defined in the preferences.                                        *
//**************************************************************************************************
void bluetft_displaybattery ( uint16_t bat0, uint16_t bat100, uint16_t adcval )
{
  if ( bluetft_tft )
  {
    if ( bat0 < bat100 )                                  // Levels set in preferences?
    {
      static uint16_t oldpos = 0 ;                        // Previous charge level
      uint16_t        ypos ;                              // Position on screen
      uint16_t        v ;                                 // Constrainted ADC value
      uint16_t        newpos ;                            // Current setting

      v = constrain ( adcval, bat0, bat100 ) ;            // Prevent out of scale
      newpos = map ( v, bat0, bat100, 0,                  // Compute length of green bar
                     dsp_getwidth() ) ;
      if ( newpos != oldpos )                             // Value changed?
      {
        oldpos = newpos ;                                 // Remember for next compare
        ypos = bluetft_tftdata[1].y - 5 ;                 // Just before 1st divider
        dsp_fillRect ( 0, ypos, newpos, 2, GREEN ) ;      // Paint green part
        dsp_fillRect ( newpos, ypos,
                       dsp_getwidth() - newpos,
                       2, RED ) ;                          // Paint red part
      }
    }
  }
}


//**************************************************************************************************
//                                      D I S P L A Y V O L U M E                                  *
//**************************************************************************************************
// Show the current volume as an indicator on the screen.                                          *
// The indicator is 2 pixels heigh.                                                                *
//**************************************************************************************************
void bluetft_displayvolume ( uint8_t vol )
{
  if ( bluetft_tft )
  {
    static uint8_t oldvol = 0 ;                         // Previous volume
    uint16_t       pos ;                                // Positon of volume indicator

    if ( vol != oldvol )                                // Volume changed?
    {
      oldvol = vol ;                                    // Remember for next compare
      pos = map ( vol, 0, 100, 0, dsp_getwidth() ) ;    // Compute position on TFT
      dsp_fillRect ( 0, dsp_getheight() - 2,
                     pos, 2, RED ) ;                    // Paint red part
      dsp_fillRect ( pos, dsp_getheight() - 2,
                     dsp_getwidth() - pos, 2, GREEN ) ; // Paint green part
    }
  }
}


//**************************************************************************************************
//                                      D I S P L A Y T I M E                                      *
//**************************************************************************************************
// Show the time on the LCD at a fixed position in a specified color                               *
// To prevent flickering, only the changed part of the timestring is displayed.                    *
// An empty string will force a refresh on next call.                                              *
// A character on the screen is 8 pixels high and 6 pixels wide.                                   *
//**************************************************************************************************
void bluetft_displaytime ( const char* str, uint16_t color )
{
  static char oldstr[9] = "........" ;             // For compare
  uint16_t    pos = dsp_getwidth() + TIMEPOS ;     // X-position of text, TIMEPOS is negative

  if ( str[0] == '\0' )                            // Empty string?
  {
    strcpy ( oldstr, "........" ) ;
    return ;                                       // No actual display yet
  }
  if ( bluetft_tft )                               // TFT active?
  {
    if ( strncmp ( str, oldstr, 8 ) != 0 )
    {
      dsp_fillRect ( pos, 0, -TIMEPOS, 14, BLACK ) ;
      if ( fontArialBold11.ready() )
      {
        fontArialBold11.draw ( *bluetft_tft, str, pos, 0, -TIMEPOS, 14, color ) ;
      }
      else
      {
        dsp_setTextColor ( color ) ;
        dsp_setCursor ( pos, 0 ) ;
        dsp_print ( str ) ;
      }
      strncpy ( oldstr, str, 8 ) ;
      oldstr[8] = '\0' ;
    }
  }
}


//**************************************************************************************************
//                                  D R A W S M O O T H T E X T                                   *
//**************************************************************************************************
bool bluetft_drawSmoothText ( uint16_t section, const char* str,
                              uint16_t color, uint16_t width )
{
  if ( !bluetft_tft || section >= TFTSECS ) return false ;
  SmoothFont* fonts[4] ;
  uint8_t count = 0 ;
  if ( section == 0 )
  {
    fonts[count++] = &fontArialBold11 ;
    fonts[count++] = &fontArialBold8 ;
  }
  else if ( section == 1 )
  {
    fonts[count++] = &fontArialBold16 ;
    fonts[count++] = &fontArial12 ;
    fonts[count++] = &fontArialBold11 ;
    fonts[count++] = &fontArialBold8 ;
  }
  else if ( section == 2 )
  {
    fonts[count++] = &fontArialBold11 ;
    fonts[count++] = &fontArialBold8 ;
  }
  else
  {
    fonts[count++] = &fontArial12 ;
    fonts[count++] = &fontArialBold11 ;
    fonts[count++] = &fontArialBold8 ;
  }

  SmoothFont* font = NULL ;
  for ( uint8_t i = 0 ; i < count ; i++ )
  {
    if ( fonts[i]->ready() ) font = fonts[i] ;
    if ( fonts[i]->ready() &&
         fonts[i]->fits ( str, width, bluetft_tftdata[section].height ) )
    {
      font = fonts[i] ;
      break ;
    }
  }
  if ( !font ) return false ;
  font->draw ( *bluetft_tft, str, 0, bluetft_tftdata[section].y,
               width, bluetft_tftdata[section].height, color ) ;
  return true ;
}


//**************************************************************************************************
//                              D R A W S T A T I O N N U M B E R                                 *
//**************************************************************************************************
void bluetft_drawStationNumber ( const char* str, uint16_t color )
{
  if ( !bluetft_tft ) return ;
  const int16_t left = dsp_getwidth() - STATIONNUMWIDTH ;
  dsp_fillRect ( left, bluetft_tftdata[2].y, STATIONNUMWIDTH,
                 bluetft_tftdata[2].height, BLACK ) ;
  if ( !str || !*str || !fontNums.ready() ) return ;
  uint16_t width = fontNums.textWidth ( str ) ;
  int16_t x = dsp_getwidth() - width ;
  if ( x < left ) x = left ;
  int16_t y = bluetft_tftdata[2].y +
              ( bluetft_tftdata[2].height - fontNums.lineHeight() ) / 2 ;
  fontNums.draw ( *bluetft_tft, str, x, y, STATIONNUMWIDTH,
                  fontNums.lineHeight(), color ) ;
}


//**************************************************************************************************
//                                D R A W S T A T I O N L I S T                                   *
//**************************************************************************************************
void bluetft_drawStationList ( const char* const* rows, uint8_t count,
                               uint8_t selectedRow )
{
  if ( !bluetft_tft || !rows || !fontArialBold11.ready() ) return ;
  dsp_erase() ;
  uint8_t lineHeight = fontArialBold11.lineHeight() ;
  int16_t y = ( dsp_getheight() - count * lineHeight ) / 2 ;
  for ( uint8_t row = 0 ; row < count ; row++ )
  {
    bool selected = row == selectedRow ;
    uint16_t background = selected ? YELLOW : BLACK ;
    uint16_t foreground = selected ? BLACK : WHITE ;
    if ( selected ) dsp_fillRect ( 0, y, dsp_getwidth(), lineHeight, background ) ;
    SmoothFont* rowFont = &fontArialBold11 ;
    if ( rowFont->textWidth ( rows[row] ) > dsp_getwidth() - 2 &&
         fontArialBold8.ready() ) rowFont = &fontArialBold8 ;
    int16_t rowY = y + ( lineHeight - rowFont->lineHeight() ) / 2 ;
    rowFont->draw ( *bluetft_tft, rows[row], 1, rowY,
                    dsp_getwidth() - 2, rowFont->lineHeight(),
                    foreground, background ) ;
    y += lineHeight ;
  }
}
