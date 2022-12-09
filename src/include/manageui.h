/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_MANAGEUI_H
#define INC_MANAGEUI_H

#include "conn.h"
#include "msgparse.h"
#include "nlist.h"
#include "playlist.h"
#include "procutil.h"
#include "ui.h"

enum {
  MANAGE_PRELOAD,
  MANAGE_STD,
  MANAGE_CREATE,    // used for create-from-playlist
};

enum {
  MANAGE_DB_COUNT_SAVE = 10,
};

/* managepl.c */
typedef struct managepl managepl_t;

managepl_t *managePlaylistAlloc (UIWidget *window, nlist_t *options,
    UIWidget *statusMsg);
void managePlaylistFree (managepl_t *managepl);
void managePlaylistSetLoadCallback (managepl_t *managepl, UICallback *uicb);
void manageBuildUIPlaylist (managepl_t *managepl, UIWidget *vboxp);
uimenu_t *managePlaylistMenu (managepl_t *managepl, UIWidget *menubar);
void managePlaylistSave (managepl_t *managepl);
void managePlaylistLoadCheck (managepl_t *managepl);
void managePlaylistLoadFile (managepl_t *managepl, const char *fn, int preloadflag);
bool managePlaylistNew (managepl_t *managepl, int preloadflag);

/* managepltree.c */
typedef struct managepltree managepltree_t;

managepltree_t *managePlaylistTreeAlloc (UIWidget *statusMsg);
void managePlaylistTreeFree (managepltree_t *managepltree);
void manageBuildUIPlaylistTree (managepltree_t *managepltree, UIWidget *vboxp,  UIWidget *tophbox);
void managePlaylistTreePopulate (managepltree_t *managepltree, playlist_t *pl);
bool managePlaylistTreeIsChanged (managepltree_t *managepltree);
void managePlaylistTreeUpdatePlaylist (managepltree_t *managepltree);

/* manageseq.c */
typedef struct manageseq manageseq_t;

manageseq_t *manageSequenceAlloc (UIWidget *window, nlist_t *options,
    UIWidget *statusMsg);
void manageSequenceFree (manageseq_t *manageseq);
void manageSequenceSetLoadCallback (manageseq_t *manageseq, UICallback *uicb);
void manageSequenceSetNewCallback (manageseq_t *manageseq, UICallback *uicb);
void manageBuildUISequence (manageseq_t *manageseq, UIWidget *vboxp);
uimenu_t *manageSequenceMenu (manageseq_t *manageseq, UIWidget *menubar);
void manageSequenceSave (manageseq_t *manageseq);
void manageSequenceLoadCheck (manageseq_t *manageseq);
void manageSequenceLoadFile (manageseq_t *manageseq, const char *fn, int preloadflag);

/* managedb.c */
typedef struct managedb managedb_t;

managedb_t *manageDbAlloc (UIWidget *window, nlist_t *options, UIWidget *statusMsg, conn_t *conn, procutil_t **processes);
void  manageDbFree (managedb_t *managedb);
void  manageBuildUIUpdateDatabase (managedb_t *managedb, UIWidget *vboxp);
bool  manageDbChg (void *udata);
void  manageDbProgressMsg (managedb_t *managedb, char *args);
void  manageDbStatusMsg (managedb_t *managedb, char *args);
void  manageDbFinish (managedb_t *managedb, int routefrom);
void  manageDbClose (managedb_t *managedb);
void  manageDbResetButtons (managedb_t *managedb);

/* managemisc.c */
bool manageCreatePlaylistCopy (UIWidget *statusMsg,
    const char *oname, const char *newname);
int manageValidateName (uientry_t *entry, void *udata);
void manageDeletePlaylist (UIWidget *statusMsg, const char *name);
char * manageTrimName (const char *name);

/* managestats.c */
typedef struct managestats managestats_t;

managestats_t *manageStatsInit (conn_t *conn, musicdb_t *musicdb);
void  manageStatsFree (managestats_t *managestats);
UIWidget *manageBuildUIStats (managestats_t *managestats);
void manageStatsProcessData (managestats_t *managestats, mp_musicqupdate_t *musicqupdate);

#endif /* INC_MANAGEUI_H */
