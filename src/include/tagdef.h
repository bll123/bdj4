/*
 * Copyright 2021-2024 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_TAGDEF_H
#define INC_TAGDEF_H

#include <stdbool.h>

#include "bdjstring.h"
#include "datafile.h"
#include "slist.h"

#if defined (__cplusplus) || defined (c_plusplus)
extern "C" {
#endif

typedef enum {
  ET_COMBOBOX,
  ET_ENTRY,
  ET_LABEL,
  ET_NA,
  ET_SCALE,
  ET_SPINBOX,
  ET_SPINBOX_TIME,
  ET_SPINBOX_TEXT,
  ET_SWITCH,
} tagedittype_t;

enum {
  TAG_TYPE_VORBIS,    // .ogg, .flac, .opus
  TAG_TYPE_MP4,       // MP4 .m4a, et.al.
  TAG_TYPE_ID3,       // .mp3
  TAG_TYPE_WMA,       // ASF .wma
  TAG_TYPE_WAV,       // RIFF .wav
  TAG_TYPE_MAX,
};

typedef struct {
  const char  *tag;
  const char  *base;
  const char  *desc;
} tagaudiotag_t;

typedef struct {
  const char          *tag;
  const char          *displayname;
  const char          *shortdisplayname;
  tagaudiotag_t       audiotags [TAG_TYPE_MAX];
  const char          *itunesName;
  tagedittype_t       editType;
  valuetype_t         valueType;
  dfConvFunc_t        convfunc;
  bool                listingDisplay : 1;
  /* a secondary display is a special tag to display additional information */
  /* in the song editor */
  bool                secondaryDisplay : 1;
  bool                ellipsize : 1;
  bool                alignend : 1;
  bool                isBDJTag : 1;
  bool                isNormTag : 1;
  bool                allEdit : 1;
  bool                isEditable : 1;
  bool                isAudioID : 1;
  bool                marqueeDisp : 1;
  bool                pluiDisp : 1;
  bool                textSearchable : 1;
  bool                vorbisMulti : 1;
} tagdef_t;

typedef enum {
  TAG_ADJUSTFLAGS,            //
  TAG_ALBUM,                  //
  TAG_ALBUMARTIST,            //
  TAG_ARTIST,                 //
  TAG_AUDIOID_IDENT,          //
  TAG_AUDIOID_SCORE,          //
  TAG_BPM,                    //
  TAG_BPM_DISPLAY,            //
  TAG_COMMENT,                //
  TAG_COMPOSER,               //
  TAG_CONDUCTOR,              //
  TAG_DANCE,                  //
  TAG_DANCELEVEL,             //
  TAG_DANCERATING,            //
  TAG_DATE,                   //
  TAG_DBADDDATE,              // only in the database, treated as special case
  TAG_DBIDX,                  // not saved to db or af
  TAG_DB_LOC_LOCK,            // only in the database
  TAG_DISCNUMBER,             //
  TAG_DISCTOTAL,              //
  TAG_DURATION,               // not saved to af
  TAG_FAVORITE,               //
  TAG_URI,                   //
  TAG_GENRE,                  //
  TAG_KEYWORD,                //
  TAG_MQDISPLAY,              //
  TAG_NOTES,                  //
  TAG_PREFIX_LEN,             // used for secondary directories
  TAG_RECORDING_ID,           // musicbrainz_trackid
  TAG_RRN,                    // not saved to db or af
  TAG_SAMESONG,               //
  TAG_SONGEND,                //
  TAG_SONGSTART,              //
  TAG_SORT_ALBUM,
  TAG_SORT_ALBUMARTIST,
  TAG_SORT_ARTIST,
  TAG_SORT_COMPOSER,
  TAG_SORT_TITLE,
  TAG_SPEEDADJUSTMENT,        //
  TAG_STATUS,                 //
  TAG_TAGS,                   //
  TAG_DB_FLAGS,               // not saved to db of af
  TAG_TITLE,                  //
  TAG_TRACK_ID,               // musicbrainz_releasetrackid
  TAG_TRACKNUMBER,            //
  TAG_TRACKTOTAL,             //
  TAG_LAST_UPDATED,           //
  TAG_VOLUMEADJUSTPERC,       //
  TAG_WORK_ID,                // musicbrainz_workid
  TAG_KEY_MAX,
} tagdefkey_t;

enum {
  /* this can be approximate.  it controls how many itunes fields are */
  /* placed in the first column in the configuration ui */
  TAG_ITUNES_MAX = 18,
};

extern tagdef_t tagdefs [TAG_KEY_MAX];

void        tagdefInit (void);
void        tagdefCleanup (void);
tagdefkey_t tagdefLookup (const char *str);

#if defined (__cplusplus) || defined (c_plusplus)
} /* extern C */
#endif

#endif /* INC_TAGDEF_H */
