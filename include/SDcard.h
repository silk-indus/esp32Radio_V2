// SDcard.h
// Includes for SD card interface.
// The tracklist will be written to a file on the SD card itself.
//
#if ESP_ARDUINO_VERSION_MAJOR < 2                     // File function "path()" not available in older versions
  #define path() name()                               // Use "name()" instead
#endif
#define MAXFNLEN    255                               // Max length of a full filespec
#define SD_MAXDEPTH 8                                 // Maximum MP3 folder nesting depth

struct mp3spec_t                                      // For List of mp3 file on SD card
{
  uint8_t entrylen ;                                  // Total length of this entry
  uint8_t prevlen ;                                   // Number of chars that are equal to previous
  char    filespec[MAXFNLEN] ;                        // Full file spec, the divergent end part
} ;


#ifndef SDCARD
  #define mount_SDCARD()         false                   // Dummy mount
  #define scan_SDCARD()                                  // Dummy scan files
  //#define check_SDCARD()                               // Dummy check
  #define close_SDCARD()                                 // Dummy close
  #define read_SDCARD(a,b)       0                       // Dummy read file buffer
  #define selectnextSDnode(b)    String("")
  #define getSDfilename(a)       String("")
  #define listsdtracks(a,b,c)    0
  #define connecttofile_SD()      false                  // Dummy connect to file
  #define initSDTask()                                   // Dummy start of task

#else
  #include <SPI.h>
  #include <SD.h>
  #include <FS.h>
  #include <strings.h>                                  // strcasecmp for .mp3/.MP3 variants
  #define SDSPEED   2000000                             // SPI speed of SD card
  #define TRACKLIST "/tracklist.dat"                    // File with tracklist on SD card

  bool            SD_okay = false ;                     // SD is okay
  bool            SD_mounted = false ;                  // SD is mounted
  int             SD_filecount = 0 ;                    // Number of filenames in SD_nodelist
  int             SD_curindex ;                         // Current index in mp3names
  RTC_NOINIT_ATTR mp3spec_t   mp3entry ;                // Entry with track spec
  RTC_NOINIT_ATTR char        SD_lastmp3spec[512] ;     // Previous full file spec
  File            mp3file ;                             // File containing mp3 on SD card
  int             mp3filelength = 0 ;                   // Length of file
  uint32_t        SD_totalbytes = 0 ;                   // Audio bytes at start of current track
  uint16_t        SD_bitrate = 0 ;                      // Bitrate detected from first MP3 frame
  uint32_t        SD_duration = 0 ;                     // Exact VBR duration, if present
  uint32_t        SD_audio_start = 0 ;                  // Absolute offset of the first MP3 frame
  uint32_t        SD_cover_offset = 0 ;                 // Absolute offset of embedded JPEG cover
  uint32_t        SD_cover_length = 0 ;                 // Length of embedded JPEG cover
  int16_t         SD_seek_seconds = 0 ;                 // Pending relative seek request
  bool            SD_playing = false ;                  // Local track playback is active
  bool            SD_cover_visible = false ;            // Cover currently occupies the TFT
  bool            randomplay = false ;                  // Switch for random play
  File            trackfile ;                           // File for tracknames
  bool            trackfile_isopen = false ;            // True if trackfile is open for read
  
  // Forward declaration
  void setdatamode ( datamode_t newmode ) ;
  void SDtask      ( void * parameter ) ;

  const char*      STAG = "SDcard" ;


  //**************************************************************************************************
  //                               C L O S E T R A C K F I L E                                       *
  //**************************************************************************************************
  void closeTrackfile()
  {
    if ( trackfile_isopen )                             // Trackfile still open?
    {
      trackfile.close() ;
    }
    trackfile_isopen = false ;                          // Reset open status
  }


  //**************************************************************************************************
  //                                  O P E N T R A C K F I L E                                       *
  //**************************************************************************************************
  bool openTrackfile ( const char* mode = FILE_READ )
  {
    closeTrackfile() ;                                  // Close if already open
    trackfile = SD.open ( TRACKLIST, mode ) ;           // Open the tracklist file on SD card
    ESP_LOGI ( STAG, "OpenTrackfile(%s) result is %d",
               mode, (int)trackfile ) ;
    trackfile_isopen = ( trackfile != 0 ) ;             // Check result
    return trackfile_isopen ;                           // Return open status
  }


  //**************************************************************************************************
  //                               G E T F I R S T S D F I L E N A M E                               *
  //**************************************************************************************************
  // Get the first filespec from track file on SD card.                                              *
  //**************************************************************************************************
  const char* getFirstSDFileName()
  {
    if ( ! openTrackfile() )                            // Try to open track file
    {
      return NULL ;                                     // File not available
    }
    if ( trackfile.available() < 3 )                    // No complete entry available?
    {
      closeTrackfile() ;
      SD_filecount = 0 ;
      return NULL ;
    }
    uint8_t* p = (uint8_t*)&mp3entry ;                  // Point to entrylength of mp3entry
    trackfile.read ( p, sizeof(mp3entry.entrylen) ) ;   // Get total size of entry
    p += sizeof(mp3entry.entrylen) ;                    // Bump pointer
    trackfile.read ( p, mp3entry.entrylen -             // Read rest of entry
                        sizeof(mp3entry.entrylen) ) ;
    strcpy ( SD_lastmp3spec, mp3entry.filespec ) ;      // Copy filename into SD_last
    SD_curindex = 0 ;                                   // Set current index to 0
    return SD_lastmp3spec ;                             // Return pointer to filename
  }


  //**************************************************************************************************
  //                                   S E T S D F I L E N A M E                                     *
  //**************************************************************************************************
  // Set the current filespec.                                                                       *
  //**************************************************************************************************
  void setSDFileName ( const char* fnam )
  {
    if ( strlen ( fnam ) < MAXFNLEN )                   // Guard against long filenames
    {
      strcpy ( SD_lastmp3spec,  fnam ) ;                // Copy filename into SD_last
    }
  }


  //**************************************************************************************************
  //                                   G E T S D F I L E N A M E                                     *
  //**************************************************************************************************
  // Get a filespec from tracklist file at index.                                                    *
  // If index is negative, a random track is selected.                                               *
  //**************************************************************************************************
  char* getSDFileName ( int inx )
  {
    if ( ! trackfile_isopen )                             // Track file available?
    {
      return NULL ;                                       // No, return empty name
    }
    if ( inx < 0 )                                        // Negative track number?
    {
      inx = (int) random ( SD_filecount ) ;               // Yes, pick random track
      randomplay = true ;                                 // Set random play flag
    }
    else
    {
      randomplay = false ;                                // Not random, reset flag
    }
    if ( inx >= SD_filecount )                            // Protect against going beyond last track
    {
      inx = 0 ;                                           // Beyond last track: rewind to begin
    }
    if ( inx < SD_curindex )                              // Going backwards?
    {
      getFirstSDFileName() ;                              // Yes, start all over
    }
    while ( SD_curindex < inx )                           // Move forward until at required position
    {
      uint8_t* pm = (uint8_t*)&mp3entry ;                 // Point to entrylen of mp3entry
      trackfile.read ( pm, sizeof(mp3entry.entrylen) ) ;  // Get total size of entry
      pm += sizeof(mp3entry.entrylen) ;                   // Bump pointer
      trackfile.read ( pm, mp3entry.entrylen -            // Read rest of entry
                         sizeof ( mp3entry.entrylen ) ) ;
      strcpy ( SD_lastmp3spec + mp3entry.prevlen,         // Copy filename into SD_last
               mp3entry.filespec ) ;
      SD_curindex++ ;                                     // Set current index
    }
    return SD_lastmp3spec ;                               // Return pointer to filename
  }


  //**************************************************************************************************
  //                               G E T N E X T S D F I L E N A M E                                 *
  //**************************************************************************************************
  // Get next filespec from mp3names.                                                                *
  //**************************************************************************************************
  char* getNextSDFileName()
  {
    int inx = SD_curindex + 1 ;                          // By default next track

    if ( randomplay )                                    // Playing random tracks?
    {
      inx = -1 ;                                         // Yes, next track will be random too
    }
    return ( getSDFileName ( inx ) ) ;                   // Select the track
  }


  //**************************************************************************************************
  //                           G E T C U R R E N T S D F I L E N A M E                               *
  //**************************************************************************************************
  // Get current filespec from mp3names.                                                             *
  //**************************************************************************************************
  char* getCurrentSDFileName()
  {
    return SD_lastmp3spec ;                             // Return pointer to filename
  }


  //**************************************************************************************************
  //                      G E T C U R R E N T S H O R T S D F I L E N A M E                          *
  //**************************************************************************************************
  // Get last part of current filespec from mp3names.                                                *
  //**************************************************************************************************
  char* getCurrentShortSDFileName()
  {
    return strrchr ( SD_lastmp3spec, '/' ) + 1 ;        // Last part of filespec
  }


  //**************************************************************************************************
  //                                  A D D T O F I L E L I S T                                      *
  //**************************************************************************************************
  // Add a filename to the trackfile on SD card.                                                     *
  // Example:                                                                                        *
  // Entry entry prev   filespec                             Interpreted result                      *
  //  num   len   len                   
  // ----- ----- ----   --------------------------------     ------------------------------------    *
  // [0]      32    0   /Fleetwood Mac/Albatross.mp3         /Fleetwood Mac/Albatross.mp3            *
  // [1]      15   15   Hold Me.mp3                          /Fleetwood Mac/Hold Me.mp3              *
  //**************************************************************************************************
  bool addToFileList ( const char* newfnam )
  {
    char*             lnam = SD_lastmp3spec ;           // Set pointer to compare
    uint8_t           n = 0 ;                           // Counter for number of equal characters
    uint16_t          l = strlen ( newfnam ) ;          // Length of new file name
    uint8_t           entrylen ;                        // Length of entry
    bool              res = false ;                     // Function result

    //ESP_LOGI ( STAG, "Full filename is %s", newfnam ) ;
    if ( l >= ( sizeof ( SD_lastmp3spec ) - 1 ) )       // Block very long filenames
    {
      ESP_LOGE ( STAG, "SD filename too long (%d)!", l ) ;
      return false ;                                    // Filename too long: skip
    }
    while ( *lnam == *newfnam )                         // Compare next character of filename
    {
      if ( *lnam == '\0' )                              // End of name?
      {
        break ;                                         // Yes, stop
      }
      n++ ;                                             // Equal: count
      lnam++ ;                                          // Update pointers
      newfnam++ ;
    }
    mp3entry.prevlen = n ;                              // This part is equal to previous name
    l -= n ;                                            // Length of rest of filename
    if ( l >= ( sizeof ( mp3spec_t ) - 5 ) )            // Block very long filenames
    {
      ESP_LOGE ( STAG, "SD filename too long (%d)!", l ) ;
      return false ;                                    // Filename too long: skip
    }
    strcpy ( mp3entry.filespec, newfnam ) ;             // This is last part of new filename
    entrylen = sizeof(mp3entry.entrylen) +              // Size of entry including string delimeter
               sizeof(mp3entry.prevlen) +
               strlen (newfnam) + 1 ;
    mp3entry.entrylen = entrylen ;
    strcpy ( lnam, newfnam ) ;                          // Set a new lastmp3spec
    ESP_LOGI ( STAG, "Added %3u : %s", n,               // Show last part of filename
               getCurrentShortSDFileName() ) ;
    if ( trackfile )                                    // Outputfile open?
    {
      res = true ;                                      // Yes, positive result
      trackfile.write ( (uint8_t*)&mp3entry,            // Yes, add to list
                         entrylen ) ; 
      SD_filecount++ ;                                  // Count number of files in list
    }
    return res ;                                        // Return result of adding name
  }


  //**************************************************************************************************
  //                                      G E T S D T R A C K S                                      *
  //**************************************************************************************************
  // Search all MP3 files on directory of SD card.                                                   *
  // Will be called recursively.                                                                     *
  //**************************************************************************************************
  bool getsdtracks ( const char * dirname, uint8_t levels )
  {
    File       root ;                                     // Work directory
    File       file ;                                     // File in work directory

    //ESP_LOGI ( STAG, "getsdt dir is %s", dirname ) ;
    root = SD.open ( dirname ) ;                          // Open directory
    if ( !root )                                          // Check on open
    {
      ESP_LOGI ( STAG, "Failed to open directory" ) ;
      return false ;
    }
    if ( !root.isDirectory() )                            // Really a directory?
    {
      ESP_LOGI ( STAG, "Not a directory" ) ;
      return false ;
    }
    file = root.openNextFile() ;
    while ( file )
    {
      vTaskDelay ( 1 ) ;                                  // Yield without slowing large folder scans
      if ( file.isDirectory() )                           // Is it a directory?
      {
        String childPath = String ( file.path() ) ;        // Keep path after closing child handle
        file.close() ;                                     // Limit simultaneous handles while recursing
        if ( levels )                                     // Dig in subdirectory?
        {
          const char* basename = strrchr ( childPath.c_str(), '/' ) ;
          if ( basename && basename[1] != '.' )            // Skip hidden directories
          {
            if ( ! getsdtracks ( childPath.c_str(),       // Non hidden directory: call recursive
                                  levels -1 ) )
            {
              return false ;                              // File I/O error
            }
          }
        }
      }
      else                                                // It is a file
      {
        const char* ext = file.name() ;                   // Point to begin of name
        size_t namelen = strlen ( ext ) ;
        ext = namelen >= 4 ? ext + namelen - 4 : ext ;    // Point safely to extension
        if ( namelen >= 4 && strcasecmp ( ext, ".mp3" ) == 0 )
        {
          if ( ! addToFileList ( file.path() ) )          // Add file to the list
          {
            file.close() ;
            break ;                                       // No need to continue
          }
        }
        file.close() ;                                     // Release before opening the next entry
      }
      file = root.openNextFile() ;
    }
    return true ;
  }


  // Return MPEG Layer III bitrate in kbps from the first valid frame header.
  static uint16_t detectSDMP3Bitrate ( const uint8_t* data, size_t len )
  {
    static const uint16_t brMpeg1Layer3[16] =
      { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 } ;
    static const uint16_t brMpeg2Layer3[16] =
      { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 } ;
    for ( size_t i = 0 ; i + 3 < len ; i++ )
    {
      if ( data[i] != 0xFF || ( data[i + 1] & 0xE0 ) != 0xE0 ) continue ;
      uint8_t version = ( data[i + 1] >> 3 ) & 0x03 ;
      uint8_t layer = ( data[i + 1] >> 1 ) & 0x03 ;
      uint8_t brIndex = ( data[i + 2] >> 4 ) & 0x0F ;
      uint8_t sampleIndex = ( data[i + 2] >> 2 ) & 0x03 ;
      if ( version == 1 || layer != 1 || brIndex == 0 || brIndex == 15 ||
           sampleIndex == 3 ) continue ;
      return version == 3 ? brMpeg1Layer3[brIndex] : brMpeg2Layer3[brIndex] ;
    }
    return 0 ;
  }


  // Return exact duration from Xing/Info or VBRI, otherwise zero for bitrate-based fallback.
  static uint32_t detectSDMP3Duration ( const uint8_t* data, size_t len )
  {
    static const uint32_t sampleRates[3] = { 44100, 48000, 32000 } ;
    for ( size_t i = 0 ; i + 40 < len ; i++ )
    {
      if ( data[i] != 0xFF || ( data[i + 1] & 0xE0 ) != 0xE0 ) continue ;
      uint8_t version = ( data[i + 1] >> 3 ) & 0x03 ;
      uint8_t layer = ( data[i + 1] >> 1 ) & 0x03 ;
      uint8_t sampleIndex = ( data[i + 2] >> 2 ) & 0x03 ;
      if ( version == 1 || layer != 1 || sampleIndex == 3 ) continue ;
      uint32_t sampleRate = sampleRates[sampleIndex] ;
      if ( version == 2 ) sampleRate /= 2 ;
      else if ( version == 0 ) sampleRate /= 4 ;
      bool mono = ( ( data[i + 3] >> 6 ) & 0x03 ) == 3 ;
      size_t sideInfo = version == 3 ? ( mono ? 17 : 32 ) : ( mono ? 9 : 17 ) ;
      size_t xing = i + 4 + sideInfo ;
      if ( xing + 12 <= len &&
           ( !memcmp ( data + xing, "Xing", 4 ) || !memcmp ( data + xing, "Info", 4 ) ) )
      {
        uint32_t flags = ( uint32_t ( data[xing + 4] ) << 24 ) |
                         ( uint32_t ( data[xing + 5] ) << 16 ) |
                         ( uint32_t ( data[xing + 6] ) << 8 ) |
                           uint32_t ( data[xing + 7] ) ;
        if ( flags & 1 )
        {
          uint32_t frames = ( uint32_t ( data[xing + 8] ) << 24 ) |
                            ( uint32_t ( data[xing + 9] ) << 16 ) |
                            ( uint32_t ( data[xing + 10] ) << 8 ) |
                              uint32_t ( data[xing + 11] ) ;
          uint32_t samplesPerFrame = version == 3 ? 1152 : 576 ;
          if ( frames && sampleRate )
          {
            return ( frames * (uint64_t)samplesPerFrame + sampleRate / 2 ) / sampleRate ;
          }
        }
      }
      size_t vbri = i + 4 + 32 ;
      if ( vbri + 18 <= len && !memcmp ( data + vbri, "VBRI", 4 ) )
      {
        uint32_t frames = ( uint32_t ( data[vbri + 14] ) << 24 ) |
                          ( uint32_t ( data[vbri + 15] ) << 16 ) |
                          ( uint32_t ( data[vbri + 16] ) << 8 ) |
                            uint32_t ( data[vbri + 17] ) ;
        uint32_t samplesPerFrame = version == 3 ? 1152 : 576 ;
        if ( frames && sampleRate )
        {
          return ( frames * (uint64_t)samplesPerFrame + sampleRate / 2 ) / sampleRate ;
        }
      }
      return 0 ;                                         // First valid audio frame checked
    }
    return 0 ;
  }


  void getSDProgress ( uint32_t& playedSeconds, uint32_t& lengthSeconds,
                       uint8_t& percent )
  {
    playedSeconds = 0 ;
    lengthSeconds = 0 ;
    percent = 0 ;
    if ( !SD_playing || !SD_totalbytes ) return ;
    uint32_t left = mp3filelength > (int)SD_totalbytes ? SD_totalbytes : mp3filelength ;
    uint32_t playedBytes = SD_totalbytes - left ;
    uint64_t percentage = ( playedBytes * 100ULL ) / SD_totalbytes ;
    percent = percentage > 100 ? 100 : (uint8_t)percentage ;
    if ( SD_duration )
    {
      lengthSeconds = SD_duration ;
      playedSeconds = ( playedBytes * (uint64_t)SD_duration ) / SD_totalbytes ;
    }
    else if ( SD_bitrate )
    {
      playedSeconds = ( playedBytes * 8ULL ) / ( (uint32_t)SD_bitrate * 1000ULL ) ;
      lengthSeconds = ( SD_totalbytes * 8ULL ) / ( (uint32_t)SD_bitrate * 1000ULL ) ;
    }
  }


  String getSDProgressText()
  {
    uint32_t playedSeconds, lengthSeconds ;
    uint8_t percent ;
    char text[28] ;
    getSDProgress ( playedSeconds, lengthSeconds, percent ) ;
    snprintf ( text, sizeof(text), "%lu:%02lu/%lu:%02lu",
               (unsigned long)( playedSeconds / 60 ),
               (unsigned long)( playedSeconds % 60 ),
               (unsigned long)( lengthSeconds / 60 ),
               (unsigned long)( lengthSeconds % 60 ) ) ;
    return String ( text ) ;
  }


  //**************************************************************************************************
  //                                  H A N D L E _ I D 3 _ S D                                      *
  //**************************************************************************************************
  // Check file on SD card for ID3 tags and use them to display some info.                           *
  // Extended headers are not parsed.                                                                *
  //**************************************************************************************************
  void handle_ID3_SD ( String &path )
  {
    char*  p ;                                                // Pointer to filename
    struct ID3head_t                                          // First part of ID3 info
    {
      char    fid[3] ;                                        // Should be filled with "ID3"
      uint8_t majV, minV ;                                    // Major and minor version
      uint8_t hflags ;                                        // Headerflags
      uint8_t ttagsize[4] ;                                   // Total tag size
    } ID3head ;
    uint8_t  exthsiz[4] = { 0, 0, 0, 0 } ;                    // Extended header size
    uint32_t stx ;                                            // Ext header size converted
    uint32_t sttg ;                                           // Total tagsize converted
    uint32_t stg ;                                            // Size of a single tag
    uint32_t audioOffset = 0 ;                                // First byte after complete ID3v2 tag
    struct ID3tag_t                                           // Tag in ID3 info
    {
      char    tagid[4] ;                                      // Things like "TCON", "TYER", ...
      uint8_t tagsize[4] ;                                    // Size of the tag
      uint8_t tagflags[2] ;                                   // Tag flags
    } ID3tag ;
    uint8_t   tmpbuf[4] ;                                     // Scratch buffer
    uint8_t   tenc ;                                          // Text encoding
    String    artist = String() ;                              // Artist shown below the title
    bool      tpe1 ;                                          // Tag is TPE1 (artist)

    SD_cover_offset = 0 ;                                      // No cover until APIC proves otherwise
    SD_cover_length = 0 ;
    SD_cover_visible = false ;
    tftset ( 2, "" ) ;                                        // Assume no artist tag
    p = (char*)path.c_str() + 1 ;                             // Point to filename (after the slash)
    showstreamtitle ( p, true ) ;                             // Show the filename as title (middle part)
    mp3file = SD.open ( path ) ;                              // Open the file
    mp3file.read ( (uint8_t*)&ID3head, sizeof(ID3head) ) ;    // Read first part of ID3 info
    if ( strncmp ( ID3head.fid, "ID3", 3 ) == 0 )
    {
      sttg = ssconv ( ID3head.ttagsize ) ;                    // Convert tagsize
      audioOffset = sizeof(ID3head) + sttg ;                  // Skip payload, padding and cover art
      if ( ID3head.hflags & 0x10 ) audioOffset += 10 ;        // Optional ID3v2 footer
      ESP_LOGI ( STAG, "Found ID3 info" ) ;
      if ( ID3head.hflags & 0x40 )                            // Extended header?
      {
        mp3file.read ( exthsiz, sizeof(exthsiz) ) ;           // Read its encoded size first
        stx = ssconv ( exthsiz ) ;                            // Get size of extended header
        if ( stx > sttg ) stx = sttg ;                        // Reject corrupt lengths
        while ( stx-- )
        {
          mp3file.read () ;                                   // Skip next byte of extended header
        }
      }
      while ( sttg > 10 )                                     // Now handle the tags
      {
        sttg -= mp3file.read ( (uint8_t*)&ID3tag,
                              sizeof(ID3tag) ) ;              // Read first part of a tag
        if ( ID3tag.tagid[0] == 0 )                           // Reached the end of the list?
        {
          break ;                                             // Yes, quit the loop
        }
        stg = ID3head.majV >= 4 ? ssconv ( ID3tag.tagsize ) : // ID3v2.4 uses synchsafe sizes
              ( (uint32_t)ID3tag.tagsize[0] << 24 ) |
              ( (uint32_t)ID3tag.tagsize[1] << 16 ) |
              ( (uint32_t)ID3tag.tagsize[2] << 8 ) |
                (uint32_t)ID3tag.tagsize[3] ;                 // ID3v2.3 uses big endian
        if ( stg > sttg ) break ;                             // Corrupt frame length
        if ( ID3tag.tagflags[1] & 0x08 )                      // Compressed?
        {
          sttg -= mp3file.read ( tmpbuf, 4 ) ;                // Yes, ignore 4 bytes
          stg -= 4 ;                                          // Reduce tag size
        }
        if ( ID3tag.tagflags[1] & 0x044 )                     // Encrypted or grouped?
        {
          sttg -= mp3file.read ( tmpbuf, 1 ) ;                // Yes, ignore 1 byte
          stg-- ;                                             // Reduce tagsize by 1
        }
        uint32_t payloadStart = mp3file.position() ;
        if ( strncmp ( ID3tag.tagid, "APIC", 4 ) == 0 && stg > 5 )
        {
          // APIC: encoding, MIME\0, picture type, description terminator, JPEG bytes.
          // Store an offset only; key 0 opens a second SD handle and streams the JPEG to the TFT.
          uint32_t left = stg ;
          uint8_t encoding = mp3file.read() ; left-- ;
          String mime ;
          while ( left )
          {
            int c = mp3file.read() ; left-- ;
            if ( c <= 0 ) break ;
            if ( mime.length() < 31 ) mime += (char)c ;
          }
          if ( left ) { mp3file.read() ; left-- ; }           // Picture type
          if ( encoding == 0 || encoding == 3 )               // ISO-8859-1 or UTF-8 description
          {
            while ( left && mp3file.read() != 0 ) left-- ;
            if ( left ) left-- ;
          }
          else                                                // UTF-16 description ends in 00 00
          {
            int previous = -1 ;
            while ( left )
            {
              int c = mp3file.read() ; left-- ;
              if ( previous == 0 && c == 0 ) break ;
              previous = c ;
            }
          }
          mime.toLowerCase() ;
          uint32_t imageOffset = mp3file.position() ;
          uint8_t jpegMagic[2] = { 0, 0 } ;
          bool jpeg = left > 2 && mp3file.read ( jpegMagic, sizeof(jpegMagic) ) == sizeof(jpegMagic) &&
                      jpegMagic[0] == 0xFF && jpegMagic[1] == 0xD8 ;
          mp3file.seek ( imageOffset ) ;
          if ( jpeg )                                          // Trust JPEG SOI even if MIME is nonstandard
          {
            SD_cover_offset = imageOffset ;
            SD_cover_length = left ;
            ESP_LOGI ( STAG, "Embedded JPEG cover found, %lu bytes, MIME %s",
                       (unsigned long)SD_cover_length, mime.c_str() ) ;
          }
          mp3file.seek ( payloadStart + stg ) ;
          sttg -= stg ;
          continue ;
        }
        if ( stg > ( sizeof(metalinebf) - 1 ) )                // Room for text tag?
        {
          mp3file.seek ( payloadStart + stg ) ;                // Skip large unknown/binary frame
          sttg -= stg ;
          continue ;
        }
        sttg -= mp3file.read ( (uint8_t*)metalinebf,
                               stg ) ;                        // Read tag contents
        metalinebf[stg] = '\0' ;                              // Add delimeter
        tenc = metalinebf[0] ;                                // First byte is encoding type
        if ( tenc == '\0' )                                   // Debug all tags with encoding 0
        {
          ESP_LOGI ( STAG, "ID3 %s = %s", ID3tag.tagid,
                    metalinebf + 1 ) ;
        }
        tpe1 = ( strncmp ( ID3tag.tagid, "TPE1", 4 ) == 0 ) ; // Artist?
        if ( tpe1 )                                           // Artist?
        {
          artist = String ( metalinebf + 1 ) ;                // Artist is the lower field
          icyname = artist ;                                  // Also use in web interface
        }
        if ( strncmp ( ID3tag.tagid, "TIT2", 4 ) == 0 )       // Songtitle?
        {
          tftset ( 1, metalinebf + 1 ) ;                      // Song title is the upper field
          icystreamtitle = String ( metalinebf + 1 ) ;        // For status in webinterface
        }
      }
      tftset ( 2, artist ) ;                                  // Artist is below the song title
      if ( audioOffset < mp3file.size() )
      {
        mp3file.seek ( audioOffset ) ;                        // Probe and play from first audio frame
      }
    }
    else
    {
      mp3file.seek ( 0 ) ;                                    // No ID3: restore bytes used for probing
    }
  }


  //**************************************************************************************************
  //                                  C O N N E C T T O F I L E _ S D                                *
  //**************************************************************************************************
  // Open the local mp3-file.                                                                        *
  //**************************************************************************************************
  bool connecttofile_SD()
  {
    String path ;                                           // Full file spec

    stop_mp3client() ;                                      // Disconnect if still connected
    SD_playing = false ;                                    // Reset progress until file is ready
    SD_totalbytes = 0 ;
    SD_bitrate = 0 ;
    SD_duration = 0 ;
    #ifdef BLUETFT
      displayplaytime ( "" ) ;                           // New track starts a new counter
    #endif
    tftset ( 0, "" ) ;                                      // Top-left field will show played/length
    displaytime ( "" ) ;                                    // Clear time on TFT screen
    setdatamode ( DATA ) ;                                  // Start in datamode 
    path = String ( getCurrentSDFileName() ) ;              // Set path to file to play
    icystreamtitle = path ;                                 // If no ID3 available
    icyname = String ( "" ) ;                               // If no ID3 available
    handle_ID3_SD ( path ) ;                                // See if there are ID3 tags in this file
    if ( !mp3file )
    {
      ESP_LOGI ( STAG, "Error opening file %s",             // No luck
                 path.c_str() ) ;
      return false ;
    }
    size_t audioStart = mp3file.position() ;                // Position after the ID3 tag
    SD_audio_start = audioStart ;                            // Required for relative seeking
    static uint8_t probe[1024] ;                            // First MPEG frame and VBR headers
    size_t probeLength = mp3file.read ( probe, sizeof(probe) ) ;
    SD_bitrate = detectSDMP3Bitrate ( probe, probeLength ) ;
    SD_duration = detectSDMP3Duration ( probe, probeLength ) ;
    mp3file.seek ( audioStart ) ;                           // Probe must not consume audio
    mp3filelength = mp3file.available() ;                   // Remaining audio bytes
    SD_totalbytes = mp3filelength ;
    SD_playing = true ;
    String progressText = getSDProgressText() ;
    tftset ( 0, progressText ) ;                            // Replace "MP3 Player"
    ESP_LOGI ( STAG, "MP3 progress: %s, bitrate %u kbps%s",
               progressText.c_str(), SD_bitrate,
               SD_duration ? ", exact VBR length" : "" ) ;
    mqttpub.trigger ( MQTT_STREAMTITLE ) ;                  // Request publishing to MQTT
    chunked = false ;                                       // File not chunked
    metaint = 0 ;                                           // No metadata
    return true ;
  }


  //**************************************************************************************************
  //                                       M O U N T _ S D C A R D                                   *
  //**************************************************************************************************
  // Initialize the SD card.                                                                         *
  //**************************************************************************************************
  bool mount_SDCARD ( int8_t csPin )
  {
    bool       okay = false ;                              // True if SD card in place and readable

    if ( csPin >= 0 )                                      // SD configured?
    {
      SD_mounted = SD.begin ( csPin, SPI,                  // Yes, try to init SD card driver
                              SDSPEED, "/sd", 12 ) ;       // Enough handles for nested folders
      if ( !SD_mounted )                                   // Init (mount) okay?
      {
        //ESP_LOGE ( STAG, "SD Card Mount Failed!" ) ;     // No success, check formatting (FAT)
      }
      else
      {
        okay = ( SD.cardType() != CARD_NONE ) ;            // See if known card
      }
    }
    if ( !okay )
    {
      ESP_LOGI ( STAG, "No SD card attached" ) ;           // Card not readable
    }
    return okay ;
  }


  //**************************************************************************************************
  //                                       C L O S E _ S D C A R D                                   *
  //**************************************************************************************************
  // Close file on SD card.                                                                          *
  //**************************************************************************************************
  void close_SDCARD()
  {
    ESP_LOGI ( STAG, "Close SD file" ) ;
    SD_playing = false ;
    SD_cover_visible = false ;
    SD_seek_seconds = 0 ;
    mp3file.close() ;                                     // Close the file
  }


  //**************************************************************************************************
  //                                   S D I N S E R T C H E C K                                     *
  //**************************************************************************************************
  // Check if new SD card is inserted and can be read.                                               *
  //**************************************************************************************************
  bool SDInsertCheck()
  {
    static uint32_t nextCheckTime = 0 ;                     // To prevent checking too often
    uint32_t        newmillis ;                             // Current timestamp
    bool            sdinsNew ;                              // Result of insert check
    void*           p ;                                     // Pointer to item from ringbuffer
    size_t          f0 ;                                    // Length of item from ringbuffer
    static bool     sdInserted = false ;                    // Yes, flag for inserted SD
    int8_t          dpin = ini_block.sd_detect_pin ;        // SD inserted detect pin

    if ( ( newmillis = millis() ) < nextCheckTime )         // Time to check?
    {
      return false ;                                        // No, return "no new insert"
    }
    nextCheckTime = newmillis + 5000 ;                      // Yes, set new check time
    if ( dpin >= 0 )                                        // Hardware detection possible?
    {
      sdinsNew = ( digitalRead ( dpin ) == LOW ) ;          // Yes, see if card inserted
      if ( sdinsNew == sdInserted )                         // Situation changed?
      {
        return false ;                                      // No, return "no new insert"
      }
      else
      {
        sdInserted = sdinsNew ;                             // Remember status
        if ( ! sdInserted )                                 // Card out?
        {
          ESP_LOGI ( STAG, "SD card removed" ) ;
          if ( SD_mounted )                                 // Still mounted?
          {
            SD.end() ;                                      // Unmount SD card
            SD_okay = false ;                               // Not okay anymore
            SD_mounted = false ;                            // And not mounted anymore
          }
        }
        else                                                // Card inserted
        {
          ESP_LOGI ( STAG, "SD card inserted" ) ;
          SD_okay = mount_SDCARD ( ini_block.sd_cs_pin ) ;  // Try to mount
        }
      }
    }
    else                                                    // Handle SD without detect pin
    {
      ESP_LOGI ( STAG, "Try to mount SD card" ) ;
      SD_okay = mount_SDCARD ( ini_block.sd_cs_pin ) ;      // Try to mount
    }
    return SD_okay ;                                        // Return result
  }


  //**************************************************************************************************
  //                                    C O U N T F I L E S                                          *
  //**************************************************************************************************
  // Count number of tracks in the track list.                                                       *
  //**************************************************************************************************
  int countfiles()
  {
    int count = 0 ;                                         // Files found
  
    while ( trackfile.available() > 1 )
    {
      uint8_t* pm = (uint8_t*)&mp3entry ;                   // Point to entrylength of mp3entry
      trackfile.read ( pm, sizeof(mp3entry.entrylen) ) ;    // Get total size of entry
      pm += sizeof(mp3entry.entrylen) ;                     // Bump pointer
      trackfile.read ( pm, mp3entry.entrylen -              // Read rest of entry
                        sizeof ( mp3entry.entrylen ) ) ;
      count++ ;                                             // count entries
      vTaskDelay ( 10 / portTICK_PERIOD_MS  ) ;             // Allow other tasks
    }
    ESP_LOGI ( STAG, "%d files on SD card", count ) ;
    return count ;                                          // Return number of tracks
  }


  //**************************************************************************************************
  //                                       S D T A S K                                               *
  //**************************************************************************************************
  // This task will constantly try to fill the ringbuffer with filenames on SD.                      *
  // if the SD detect pin is defined, a test will on SD change will be performed every 5 seconds.    *
  // Otherwise, the check is made only once, after reset.                                            *
  //**************************************************************************************************
  void SDtask ( void * parameter )
  {
    const char* ffn ;                                     // First filename on SD card
    int8_t      dpin = ini_block.sd_detect_pin ;          // SD inserted detect pin
    bool        once = true ;                             // Always check once
  
    vTaskDelay ( 1000 / portTICK_PERIOD_MS ) ;            // Start delay
    while ( true )                                        // Endless task
    {
      vTaskDelay ( 200 / portTICK_PERIOD_MS ) ;           // Allow other tasks
      if ( ( dpin < 0 ) && ( once == false ) )            // Just one check if no detect pin
      {
        continue ;
      }
      once = false ;                                      // Stop detect without detect pin
      if ( SDInsertCheck() )                              // See if new card is inserted
      {
        SD_lastmp3spec[0] = '\0' ;                        // No last track
        SD_filecount = 0 ;
        closeTrackfile() ;
        if ( SD.exists ( TRACKLIST ) && !SD.remove ( TRACKLIST ) )
        {
          ESP_LOGE ( STAG, "Cannot replace stale %s", TRACKLIST ) ;
          SD_okay = false ;
        }
        else
        {
          ESP_LOGI ( STAG, "Scan MP3 folders and rebuild %s", TRACKLIST ) ;
          if ( openTrackfile ( FILE_WRITE ) )             // Try to open trackfile for write
          {
            SD_okay = getsdtracks ( "/", SD_MAXDEPTH ) ;  // Get filenames, store on the SD card
            closeTrackfile() ;                            // Close the tracklist file
          }
          else
          {
            SD_okay = false ;
          }
        }
        ESP_LOGI ( STAG, "%d tracks in folders on SD card", SD_filecount ) ;
        ffn = getFirstSDFileName() ;
        if ( ffn )
        {
          ESP_LOGI ( STAG, "First file on SD card is %s", // Show the first track name
                     ffn ) ;
        }
      }
    }
  }
#endif
