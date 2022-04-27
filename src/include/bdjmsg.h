#ifndef INC_BDJMSG_H
#define INC_BDJMSG_H

/* when a new route is added, update: */
/* conn.c : needs to know the port for each route */
/* bdjmsg.c: debugging information for the route */
/* bdjvars.h: port enum */
/* lock.c: lock name */
typedef enum {
  ROUTE_NONE,       // anonymous
  ROUTE_CONFIGUI,
  ROUTE_DBUPDATE,   // the main db update process
  ROUTE_DBTAG,      // the db tag reader process
  ROUTE_MAIN,
  ROUTE_MANAGEUI,
  ROUTE_MARQUEE,
  ROUTE_MOBILEMQ,
  ROUTE_PLAYER,
  ROUTE_PLAYERUI,
  ROUTE_RAFFLE,
  ROUTE_REMCTRL,
  ROUTE_STARTERUI,
  ROUTE_MAX,
} bdjmsgroute_t;

typedef enum {
  MSG_NULL,
  MSG_EXIT_REQUEST,         // standard shutdown
  MSG_HANDSHAKE,
  MSG_SOCKET_CLOSE,

  /* to main */
  MSG_GET_STATUS,           // get main/player status
  MSG_MUSICQ_INSERT,        // args: music-q-idx, idx, song name
  MSG_MUSICQ_MOVE_DOWN,     // args: music-q-idx, idx
  MSG_MUSICQ_MOVE_TOP,      // args: music-q-idx, idx
  MSG_MUSICQ_MOVE_UP,       // args: music-q-idx, idx
  MSG_MUSICQ_REMOVE,        // args: music-q-idx, idx
  MSG_MUSICQ_SET_PLAYBACK,  // args: music queue for playback
  MSG_MUSICQ_TOGGLE_PAUSE,  // args: music-q-idx
  MSG_MUSICQ_TRUNCATE,      // args: music-q-idx, idx
  MSG_PLAYLIST_CLEARPLAY,   // args: playlist name
  MSG_PLAY_PLAY,            // always to main
                            //    starts playback, passed on to player.
  MSG_PLAY_PLAYPAUSE,       // always to main
  MSG_QUEUE_CLEAR,          // args: music-q-idx
  MSG_QUEUE_DANCE_5,        // args: music-q-idx, dance idx
  MSG_QUEUE_DANCE,          // args: music-q-idx, dance idx
  MSG_QUEUE_PLAYLIST,       // args: music-q-idx, playlist name
  MSG_QUEUE_PLAY_ON_ADD,    // args: true/false
  MSG_QUEUE_SWITCH_EMPTY,   // args: true/false

  /* to player */
  MSG_PLAYER_VOL_MUTE,      // to player. toggle.
  MSG_PLAYER_VOLUME,        // to player. args: volume as percentage.
  MSG_PLAY_FADE,            // to player.
  MSG_PLAY_NEXTSONG,        // to player.
  MSG_PLAY_PAUSEATEND,      // to player: toggle
  MSG_PLAY_PAUSE,           // to player
  MSG_PLAY_REPEAT,          // to player. toggle
  MSG_PLAY_SEEK,            // to player. args: position
  MSG_PLAY_SONG_BEGIN,      // to player.
  MSG_PLAY_SPEED,           // to player. args: rate as percentage.
  MSG_PLAY_STOP,            // to player.
  MSG_SONG_PLAY,            // args: song fname
  MSG_SONG_PREP,            // args: song fname, duration, song-start
                            //    song-end, volume-adjustment-perc, gap

  /* from player */
  MSG_PLAY_PAUSEATEND_STATE,// args: 0/1
  MSG_PLAYBACK_BEGIN,       // to main: player has started playing the
                            //    song.  This message is sent after the
                            //    any announcement has finished.
  MSG_PLAYBACK_STOP,        // to main: player has stopped playing song
                            //    do not continue.
  MSG_PLAYBACK_FINISH,      // to main: player has finished playing song
                            //    args: song fname
  MSG_PLAYER_STATE,         // args: player state
  MSG_PLAYER_STATUS_DATA,   // response to get_status; to main

  /* to/from ui */
  MSG_MUSIC_QUEUE_DATA,
  MSG_QUEUE_SWITCH,         // args: queue number
  MSG_SONG_SELECT,          // args: queue number, position
  MSG_FINISHED,             // also sent to marquee

  /* to/from starterui */
  MSG_START_MAIN,           // arg: true for --hidemarquee
  MSG_STOP_MAIN,

  /* to/from web servers */
  MSG_DANCE_LIST_DATA,      // args: html option list
  MSG_GET_DANCE_LIST,
  MSG_GET_PLAYLIST_LIST,    //
  MSG_MARQUEE_DATA,         // args: mq json data ; also for msg to marquee
  MSG_MUSICQ_STATUS_DATA,   // main response to remote control
  MSG_PLAYLIST_LIST_DATA,   // args: html option list

  /* to/from marquee */
  MSG_MARQUEE_TIMER,        // args: played time, duration
  MSG_MARQUEE_SET_FONT_SZ,  // args: font-size
  MSG_MARQUEE_IS_MAX,       // args: boolean flag
  MSG_MARQUEE_FONT_SIZES,   // args: font-size, font-size-fs

  /* to/from dbudpate */
  MSG_DB_FILE_TAGS,         // args: filename, tag data
  MSG_DB_PROGRESS,          // args: % complete
  MSG_DB_STATUS_MSG,        // args: status message
  MSG_DB_FINISH,            //
  /* to dbtag */
  MSG_DB_FILE_CHK,          // args: filename to check

  /* when a new message is added, update: */
  /* bdjmsg.c: debugging information for the msg */
  MSG_MAX,
} bdjmsgmsg_t;

typedef enum {
  PREP_SONG,
  PREP_ANNOUNCE,
} bdjmsgprep_t;

#define BDJMSG_MAX_ARGS     8192
#define BDJMSG_MAX          (8 * 2 + 3 + BDJMSG_MAX_ARGS)

#define MSG_ARGS_RS         0x1E      // RS
#define MSG_ARGS_RS_STR     "\x1E"
#define MSG_ARGS_EMPTY      0x03      // ETX
#define MSG_ARGS_EMPTY_STR  "\x03"

size_t    msgEncode (bdjmsgroute_t routefrom, bdjmsgroute_t route,
              bdjmsgmsg_t msg, char *args, char *msgbuff, size_t mlen);
void      msgDecode (char *msgbuff, bdjmsgroute_t *routefrom,
              bdjmsgroute_t *route, bdjmsgmsg_t *msg, char *args, size_t alen);
char *    msgDebugText (bdjmsgmsg_t msg);
char *    msgRouteDebugText (bdjmsgroute_t route);

#endif /* INC_BDJMSG_H */
