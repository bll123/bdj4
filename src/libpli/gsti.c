/*
 * Copyright 2024 Brad Lanam Pleasant Hill CA
 *
 * gst-launch-1.0 -vv playbin \
 *    uri=file://$HOME/s/bdj4/test-music/001-argentinetango.mp3
 *
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#if _hdr_gst_gst

# include <gst/gst.h>
# include <glib.h>

#include "audiosrc.h"     // for audio-source type
# include "bdj4.h"
# include "bdjstring.h"
# include "mdebug.h"
# include "gsti.h"
# include "pli.h"

enum {
  GSTI_IDENT = 0x6773746900aabbcc,
};

/* gstreamer doesn't define these */
typedef enum {
  GST_PLAY_FLAG_VIDEO = (1 << 0),
  GST_PLAY_FLAG_AUDIO = (1 << 1),
  GST_PLAY_FLAG_TEXT  = (1 << 2),
  GST_PLAY_FLAG_VIS  = (1 << 3),
  GST_PLAY_FLAG_SOFT_VOL  = (1 << 4),
  GST_PLAY_FLAG_NATIVE_AUDIO = (1 << 5),
  GST_PLAY_FLAG_NATIVE_VIDEO = (1 << 6),
  GST_PLAY_FLAG_DOWNLOAD = (1 << 7),
  GST_PLAY_FLAG_BUFFERING = (1 << 8),
  GST_PLAY_FLAG_DEINTERLACE = (1 << 9),
  GST_PLAY_FLAG_SOFT_COL_BAL = (1 << 10),
  GST_PLAY_FLAG_FORCE_FILTER = (1 << 11),
  GST_PLAY_FLAG_FORCE_SW_DECODE = (1 << 12),
} GstPlayFlags;

typedef struct gsti {
  int64_t       ident;
  GMainContext  *mainctx;
  GstElement    *pipeline;
  guint         busId;
  plistate_t    state;
  double        rate;
  bool          isstopping : 1;
} gsti_t;

static void gstiRunOnce (gsti_t *gsti);
static gboolean gstiBusCallback (GstBus * bus, GstMessage * message, void *udata);
static void gstiProcessState (gsti_t *gsti, GstState state);
static void gstiWaitState (gsti_t *gsti, GstState want);

gsti_t *
gstiInit (const char *plinm)
{
  gsti_t        *gsti;
  GstBus        *bus;
  GstPlayFlags  flags;
  double        vol = 1.0;

  gst_init (NULL, 0);

  gsti = mdmalloc (sizeof (gsti_t));
  gsti->ident = GSTI_IDENT;
  gsti->pipeline = NULL;
  gsti->busId = 0;
  gsti->state = PLI_STATE_IDLE;
  gsti->rate = 1.0;
  gsti->isstopping = false;

  gsti->pipeline = gst_element_factory_make ("playbin", "play");
  g_object_get (G_OBJECT (gsti->pipeline), "flags", &flags, NULL);
  flags |= GST_PLAY_FLAG_AUDIO;
  flags &= ~GST_PLAY_FLAG_VIDEO;
  flags &= ~GST_PLAY_FLAG_TEXT;
  flags &= ~GST_PLAY_FLAG_VIS;
  g_object_set (G_OBJECT (gsti->pipeline), "flags", flags, NULL);
  g_object_set (G_OBJECT (gsti->pipeline), "volume", vol, NULL);
  // audio-sink
  // audio-filter, probably not needed

  bus = gst_pipeline_get_bus (GST_PIPELINE (gsti->pipeline));
  gsti->busId = gst_bus_add_watch (bus, gstiBusCallback, gsti);
  g_object_unref (bus);

  gsti->mainctx = g_main_context_default ();

  gstiRunOnce (gsti);

  return gsti;
}

void
gstiFree (gsti_t *gsti)
{
  if (gsti == NULL || gsti->ident != GSTI_IDENT) {
    return;
  }

  gsti->ident = 0;

  gstiRunOnce (gsti);

  if (gsti->pipeline != NULL) {
    gsti->isstopping = true;
    g_object_set (G_OBJECT (gsti->pipeline), "volume", 0.0, NULL);
    gstiRunOnce (gsti);
    gst_element_set_state (gsti->pipeline, GST_STATE_READY);
    gstiWaitState (gsti, GST_STATE_READY);
    gst_element_set_state (gsti->pipeline, GST_STATE_NULL);
    gstiWaitState (gsti, GST_STATE_NULL);
    gst_object_unref (gsti->pipeline);
  }
  gstiRunOnce (gsti);

  mdfree (gsti);
}

void
gstiCleanup (void)
{
  return;
}

void
gstiMedia (gsti_t *gsti, const char *fulluri, int sourceType)
{
  char    tbuff [MAXPATHLEN];

  if (gsti == NULL || gsti->ident != GSTI_IDENT || gsti->mainctx == NULL) {
    return;
  }

  gstiRunOnce (gsti);

  if (sourceType == AUDIOSRC_TYPE_FILE) {
    snprintf (tbuff, sizeof (tbuff), "file://%s", fulluri);
  } else {
    strlcpy (tbuff, fulluri, sizeof (tbuff));
  }

  gsti->rate = 1.0;
  g_object_set (G_OBJECT (gsti->pipeline), "uri", tbuff, NULL);
  gst_element_set_state (GST_ELEMENT (gsti->pipeline), GST_STATE_PAUSED);
  gsti->state = PLI_STATE_OPENING;
  gstiRunOnce (gsti);
  return;
}

int64_t
gstiGetDuration (gsti_t *gsti)
{
  gint64      ctm;
  int64_t     tm = 0;

  if (gsti == NULL || gsti->ident != GSTI_IDENT || gsti->mainctx == NULL) {
    return 0;
  }

  if (! gst_element_query_duration (gsti->pipeline,
      GST_FORMAT_TIME, &ctm)) {
    return 0;
  }

  tm = ctm / 1000 / 1000;
  return tm;
}

int64_t
gstiGetPosition (gsti_t *gsti)
{
  gint64      ctm;
  int64_t     tm = 0;

  if (gsti == NULL || gsti->ident != GSTI_IDENT || gsti->mainctx == NULL) {
    return 0;
  }

  if (! gst_element_query_position (gsti->pipeline,
      GST_FORMAT_TIME, &ctm)) {
    return 0;
  }

  tm = ctm / 1000 / 1000;
fprintf (stderr, "pos: %ld\n", (long) tm);
  return tm;
}

plistate_t
gstiState (gsti_t *gsti)
{
  GstState    state, pending;
  int         rc;

  if (gsti == NULL || gsti->ident != GSTI_IDENT || gsti->mainctx == NULL) {
    return PLI_STATE_NONE;
  }

  gstiRunOnce (gsti);
  rc = gst_element_get_state (GST_ELEMENT (gsti->pipeline), &state, &pending, 1);
  gstiProcessState (gsti, state);

  return gsti->state;
}

void
gstiPause (gsti_t *gsti)
{
  if (gsti == NULL || gsti->ident != GSTI_IDENT || gsti->mainctx == NULL) {
    return;
  }

  gstiRunOnce (gsti);
  gst_element_set_state (GST_ELEMENT (gsti->pipeline), GST_STATE_PAUSED);
  gstiRunOnce (gsti);
  return;
}

void
gstiPlay (gsti_t *gsti)
{
  if (gsti == NULL || gsti->ident != GSTI_IDENT || gsti->mainctx == NULL) {
    return;
  }

  gstiRunOnce (gsti);
  gst_element_set_state (GST_ELEMENT (gsti->pipeline), GST_STATE_PLAYING);
  gstiRunOnce (gsti);
  return;
}

void
gstiStop (gsti_t *gsti)
{
  if (gsti == NULL || gsti->ident != GSTI_IDENT || gsti->mainctx == NULL) {
    return;
  }

  gsti->isstopping = true;
  g_object_set (G_OBJECT (gsti->pipeline), "volume", 0.0, NULL);
  gstiRunOnce (gsti);
  gst_element_set_state (GST_ELEMENT (gsti->pipeline), GST_STATE_READY);
  gstiWaitState (gsti, GST_STATE_READY);

  gstiRunOnce (gsti);
  g_object_set (G_OBJECT (gsti->pipeline), "volume", 1.0, NULL);
  gstiRunOnce (gsti);

  gsti->isstopping = false;

  return;
}

bool
gstiSetPosition (gsti_t *gsti, int64_t pos)
{
  int       rc = false;
  gint64    gpos;

  if (gsti == NULL || gsti->ident != GSTI_IDENT || gsti->mainctx == NULL) {
    return false;
  }

fprintf (stderr, "try set-pos: %ld\n", pos);

  if (gsti->state == PLI_STATE_PAUSED ||
      gsti->state == PLI_STATE_PLAYING) {
fprintf (stderr, "set-pos: %ld\n", pos);
    gpos = pos;
    gpos *= 1000;
    gpos *= 1000;

    /* all seeks are based on the song's original duration, */
    /* not the adjusted duration */
    if (gst_element_seek (gsti->pipeline, 1.0,
        GST_FORMAT_TIME,
        GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
        GST_SEEK_TYPE_SET, gpos,
        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
fprintf (stderr, "  seek-ok\n");
      rc = true;
    } else {
fprintf (stderr, "  seek-ng\n");
    }
  }

  gstiRunOnce (gsti);
  return rc;
}

bool
gstiSetVolume (gsti_t *gsti, double vol)
{
  if (gsti == NULL || gsti->ident != GSTI_IDENT || gsti->mainctx == NULL) {
    return false;
  }

  gstiRunOnce (gsti);
  return false;
}

bool
gstiSetRate (gsti_t *gsti, double rate)
{
  int     rc = false;

  if (gsti == NULL || gsti->ident != GSTI_IDENT || gsti->mainctx == NULL) {
    return false;
  }

  if (gsti->state == PLI_STATE_PAUSED ||
      gsti->state == PLI_STATE_PLAYING) {
    gsti->rate = rate;
fprintf (stderr, "set-rate %.2f\n", rate);

    if (gst_element_seek (gsti->pipeline, gsti->rate,
        GST_FORMAT_TIME, 0,
        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE,
        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
fprintf (stderr, "  rate-ok\n");
      rc = true;
    } else {
fprintf (stderr, "  rate-ng\n");
    }
  }

  gstiRunOnce (gsti);
  return rc;
}

/* internal routines */

static void
gstiRunOnce (gsti_t *gsti)
{
  while (g_main_context_iteration (gsti->mainctx, FALSE)) {
    ;
  }
}

static gboolean
gstiBusCallback (GstBus * bus, GstMessage * message, void *udata)
{
  gsti_t    *gsti = udata;

  // fprintf (stderr, "message %s\n", GST_MESSAGE_TYPE_NAME (message));

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_INFO:
    case GST_MESSAGE_ERROR:
    case GST_MESSAGE_WARNING: {
      GError *err;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      fprintf (stderr, "%d: %s\n", GST_MESSAGE_TYPE (message), err->message);
      g_error_free (err);
      g_free (debug);
      break;
    }
    case GST_MESSAGE_BUFFERING: {
      gint percent = 0;

      gst_message_parse_buffering (message, &percent);
      if (percent < 100 && gsti->state == PLI_STATE_PLAYING) {
        gstiPause (gsti);
        gsti->state = PLI_STATE_BUFFERING;
      }
      if (percent == 100 &&
          (gsti->state == PLI_STATE_BUFFERING ||
          gsti->state == PLI_STATE_PAUSED)) {
        gstiPlay (gsti);
      }
      break;
    }
    case GST_MESSAGE_STATE_CHANGED: {
      GstState old_state, new_state, pending_state;

      gst_message_parse_state_changed (message, &old_state, &new_state, &pending_state);
fprintf (stderr, "state: %d\n", new_state);
      gstiProcessState (gsti, new_state);
      break;
    }
    case GST_MESSAGE_DURATION_CHANGED: {
      break;
    }
    case GST_MESSAGE_EOS: {
      gsti->state = PLI_STATE_STOPPED;
      break;
    }
    default: {
      /* unhandled message */
      break;
    }
  }

  return TRUE;
}

/* while stopping states get messed up */
static void
gstiProcessState (gsti_t *gsti, GstState state)
{
  switch (state) {
    case GST_STATE_VOID_PENDING: {
      break;
    }
    case GST_STATE_NULL: {
      if (gsti->state != PLI_STATE_OPENING) {
        gsti->state = PLI_STATE_IDLE;
fprintf (stderr, "null / idle\n");
      }
      break;
    }
    case GST_STATE_READY: {
      if (gsti->state != PLI_STATE_IDLE &&
          gsti->state != PLI_STATE_OPENING) {
        gsti->state = PLI_STATE_STOPPED;
fprintf (stderr, "ready / stopped\n");
      }
      break;
    }
    case GST_STATE_PLAYING: {
fprintf (stderr, "playing\n");
      gsti->state = PLI_STATE_PLAYING;
      break;
    }
    case GST_STATE_PAUSED: {
      if (gsti->state == PLI_STATE_IDLE ||
          gsti->state == PLI_STATE_STOPPED ||
          gsti->state == PLI_STATE_OPENING) {
        if (! gsti->isstopping) {
fprintf (stderr, "%d idle,stop,open / opening\n", gsti->state);
          gsti->state = PLI_STATE_OPENING;
        }
      }
      if (gsti->state == PLI_STATE_PLAYING) {
fprintf (stderr, "playing / paused\n");
        gsti->state = PLI_STATE_PAUSED;
      }
      break;
    }
  }
}

static void
gstiWaitState (gsti_t *gsti, GstState want)
{
  GstState    state;

  gst_element_get_state (GST_ELEMENT (gsti->pipeline), &state, NULL, 1);
  while (state != want) {
    gstiRunOnce (gsti);
    gst_element_get_state (GST_ELEMENT (gsti->pipeline), &state, NULL, 1);
  }
  gstiProcessState (gsti, state);
}

#endif /* _hdr_gst */
