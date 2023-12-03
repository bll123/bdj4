/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>
#include <stdarg.h>

#include "bdj4.h"
#include "bdj4intl.h"
#include "bdjopt.h"
#include "bdjregex.h"
#include "bdjstring.h"
#include "configui.h"
#include "dirlist.h"
#include "dispsel.h"
#include "filedata.h"
#include "fileop.h"
#include "filemanip.h"
#include "log.h"
#include "mdebug.h"
#include "nlist.h"
#include "orgutil.h"
#include "pathbld.h"
#include "pathdisp.h"
#include "pathutil.h"
#include "slist.h"
#include "sysvars.h"
#include "webclient.h"

static nlist_t * confuiGetThemeList (void);
static slist_t * confuiGetThemeNames (slist_t *themelist, slist_t *filelist);
static char * confuiMakeQRCodeFile (char *title, char *uri);
static void confuiUpdateOrgExample (org_t *org, char *data, uiwcont_t *uiwidgetp);
static bool confuiSearchDispSel (confuigui_t *gui, int selidx, const char *disp);
static void confuiCreateTagListingMultDisp (confuigui_t *gui, slist_t *dlist, int sidx, int eidx);


/* the theme list is used by both the ui and the marquee selections */
void
confuiLoadThemeList (confuigui_t *gui)
{
  nlist_t     *tlist;
  nlistidx_t  iteridx;
  int         count;
  bool        usesys = false;
  const char  *p;

  p = bdjoptGetStr (OPT_MP_UI_THEME);
  /* use the system default if the ui theme is empty */
  if (p == NULL || ! *p) {
    usesys = true;
  }

  tlist = confuiGetThemeList ();
  nlistStartIterator (tlist, &iteridx);
  count = 0;
  while ((p = nlistIterateValueData (tlist, &iteridx)) != NULL) {
    if (strcmp (p, bdjoptGetStr (OPT_MP_MQ_THEME)) == 0) {
      gui->uiitem [CONFUI_SPINBOX_MQ_THEME].listidx = count;
    }
    if (! usesys &&
        strcmp (p, bdjoptGetStr (OPT_MP_UI_THEME)) == 0) {
      gui->uiitem [CONFUI_SPINBOX_UI_THEME].listidx = count;
    }
    if (usesys &&
        strcmp (p, sysvarsGetStr (SV_THEME_DEFAULT)) == 0) {
      gui->uiitem [CONFUI_SPINBOX_UI_THEME].listidx = count;
    }
    ++count;
  }
  /* the theme list is ordered */
  gui->uiitem [CONFUI_SPINBOX_UI_THEME].displist = tlist;
  gui->uiitem [CONFUI_SPINBOX_MQ_THEME].displist = tlist;
}

void
confuiUpdateMobmqQrcode (confuigui_t *gui)
{
  char          uridisp [MAXPATHLEN];
  char          *qruri = "";
  char          tbuff [MAXPATHLEN];
  bool          enabled;
  uiwcont_t    *uiwidgetp = NULL;

  logProcBegin (LOG_PROC, "confuiUpdateMobmqQrcode");

  enabled = bdjoptGetNum (OPT_P_MOBILEMARQUEE);

  confuiSetStatusMsg (gui, "");
  if (! enabled) {
    *tbuff = '\0';
    *uridisp = '\0';
    qruri = "";
  }
  if (enabled) {
    const char  *host;

    host = sysvarsGetStr (SV_HOSTNAME);
    snprintf (uridisp, sizeof (uridisp), "http://%s.local:%" PRId64, host,
        bdjoptGetNum (OPT_P_MOBILEMQPORT));
  }

  if (enabled) {
    /* CONTEXT: configuration: qr code: title display for mobile marquee */
    qruri = confuiMakeQRCodeFile (_("Mobile Marquee"), uridisp);
  }

  uiwidgetp = gui->uiitem [CONFUI_WIDGET_MMQ_QR_CODE].uiwidgetp;
  uiLinkSet (uiwidgetp, uridisp, qruri);
  dataFree (gui->uiitem [CONFUI_WIDGET_MMQ_QR_CODE].uri);
  gui->uiitem [CONFUI_WIDGET_MMQ_QR_CODE].uri = mdstrdup (qruri);
  if (*qruri) {
    mdfree (qruri);
  }
  logProcEnd (LOG_PROC, "confuiUpdateMobmqQrcode", "");
}

void
confuiUpdateRemctrlQrcode (confuigui_t *gui)
{
  char          uridisp [MAXPATHLEN];
  char          *qruri = "";
  char          tbuff [MAXPATHLEN];
  bool          enabled;
  uiwcont_t    *uiwidgetp;

  logProcBegin (LOG_PROC, "confuiUpdateRemctrlQrcode");

  enabled = bdjoptGetNum (OPT_P_REMOTECONTROL);

  if (! enabled) {
    *tbuff = '\0';
    *uridisp = '\0';
    qruri = "";
  }
  if (enabled) {
    const char  *host;

    host = sysvarsGetStr (SV_HOSTNAME);
    snprintf (uridisp, sizeof (uridisp), "http://%s.local:%" PRId64, host,
        bdjoptGetNum (OPT_P_REMCONTROLPORT));
  }

  if (enabled) {
    /* CONTEXT: configuration: qr code: title display for mobile remote control */
    qruri = confuiMakeQRCodeFile (_("Mobile Remote Control"), uridisp);
  }

  uiwidgetp = gui->uiitem [CONFUI_WIDGET_RC_QR_CODE].uiwidgetp;
  uiLinkSet (uiwidgetp, uridisp, qruri);
  dataFree (gui->uiitem [CONFUI_WIDGET_RC_QR_CODE].uri);
  gui->uiitem [CONFUI_WIDGET_RC_QR_CODE].uri = mdstrdup (qruri);
  if (*qruri) {
    mdfree (qruri);
  }
  logProcEnd (LOG_PROC, "confuiUpdateRemctrlQrcode", "");
}

void
confuiUpdateOrgExamples (confuigui_t *gui, const char *orgpath)
{
  char      *data;
  org_t     *org;
  uiwcont_t*uiwidgetp;

  if (orgpath == NULL) {
    return;
  }

  logProcBegin (LOG_PROC, "confuiUpdateOrgExamples");
  org = orgAlloc (orgpath);

  data = "FILE\n..none\nDISC\n..1\nTRACKNUMBER\n..1\nALBUM\n..Smooth\nALBUMARTIST\n..Santana\nARTIST\n..Santana\nDANCE\n..Cha Cha\nGENRE\n..Ballroom Dance\nTITLE\n..Smooth\n";
  uiwidgetp = gui->uiitem [CONFUI_WIDGET_AO_EXAMPLE_1].uiwidgetp;
  confuiUpdateOrgExample (org, data, uiwidgetp);

  data = "FILE\n..none\nDISC\n..1\nTRACKNUMBER\n..2\nALBUM\n..The Ultimate Latin Album 4: Latin Eyes\nALBUMARTIST\n..WRD\nARTIST\n..Gizelle D'Cole\nDANCE\n..Rumba\nGENRE\n..Ballroom Dance\nTITLE\n..Asi\n";
  uiwidgetp = gui->uiitem [CONFUI_WIDGET_AO_EXAMPLE_2].uiwidgetp;
  confuiUpdateOrgExample (org, data, uiwidgetp);

  data = "FILE\n..none\nDISC\n..1\nTRACKNUMBER\n..3\nALBUM\n..Shaman\nALBUMARTIST\n..Santana\nARTIST\n..Santana\nDANCE\n..Waltz\nTITLE\n..The Game of Love\nGENRE\n..Latin";
  uiwidgetp = gui->uiitem [CONFUI_WIDGET_AO_EXAMPLE_3].uiwidgetp;
  confuiUpdateOrgExample (org, data, uiwidgetp);

  data = "FILE\n..none\nDISC\n..2\nTRACKNUMBER\n..2\nALBUM\n..The Ultimate Latin Album 9: Footloose\nALBUMARTIST\n..\nARTIST\n..Raphael\nDANCE\n..Rumba\nTITLE\n..Ni tú ni yo\nGENRE\n..Latin";
  uiwidgetp = gui->uiitem [CONFUI_WIDGET_AO_EXAMPLE_4].uiwidgetp;
  confuiUpdateOrgExample (org, data, uiwidgetp);

  orgFree (org);
  logProcEnd (LOG_PROC, "confuiUpdateOrgExamples", "");
}

void
confuiSetStatusMsg (confuigui_t *gui, const char *msg)
{
  uiLabelSetText (gui->statusMsg, msg);
}

void
confuiSelectDirDialog (uisfcb_t *sfcb, const char *startpath,
    const char *title)
{
  char        *fn = NULL;
  uiselect_t  *selectdata;

  logProcBegin (LOG_PROC, "confuiSelectDirDialog");
  selectdata = uiDialogCreateSelect (sfcb->window, title, startpath, NULL, NULL, NULL);
  fn = uiSelectDirDialog (selectdata);
  if (fn != NULL) {
    uiEntrySetValue (sfcb->entry, fn);
    logMsg (LOG_INSTALL, LOG_IMPORTANT, "selected loc: %s", fn);
    mdfree (fn);
  }
  mdfree (selectdata);
  logProcEnd (LOG_PROC, "confuiSelectDirDialog", "");
}

void
confuiSelectFileDialog (uisfcb_t *sfcb, const char *startpath,
    const char *mimefiltername, const char *mimetype)
{
  char        *fn = NULL;
  uiselect_t  *selectdata;

  logProcBegin (LOG_PROC, "confuiSelectFileDialog");
  selectdata = uiDialogCreateSelect (sfcb->window,
      /* CONTEXT: configuration: file selection dialog: window title */
      _("Select File"), startpath, NULL, mimefiltername, mimetype);
  fn = uiSelectFileDialog (selectdata);
  if (fn != NULL) {
    uiEntrySetValue (sfcb->entry, fn);
    logMsg (LOG_INSTALL, LOG_IMPORTANT, "selected loc: %s", fn);
    mdfree (fn);
  }
  mdfree (selectdata);
  logProcEnd (LOG_PROC, "confuiSelectFileDialog", "");
}

void
confuiCreateTagListingDisp (confuigui_t *gui)
{
  dispselsel_t  selidx;

  logProcBegin (LOG_PROC, "confuiCreateTagListingDisp");

  selidx = uiSpinboxTextGetValue (gui->uiitem [CONFUI_SPINBOX_DISP_SEL].spinbox);

  if (selidx == DISP_SEL_SONGEDIT_A ||
      selidx == DISP_SEL_SONGEDIT_B ||
      selidx == DISP_SEL_SONGEDIT_C) {
    confuiCreateTagListingMultDisp (gui, gui->edittaglist,
        DISP_SEL_SONGEDIT_A, DISP_SEL_SONGEDIT_C);
  } else if (selidx == DISP_SEL_AUDIOID_LIST ||
      selidx == DISP_SEL_AUDIOID) {
    uiduallistSet (gui->dispselduallist, gui->audioidtaglist,
        DUALLIST_TREE_SOURCE);
  } else {
    uiduallistSet (gui->dispselduallist, gui->listingtaglist,
        DUALLIST_TREE_SOURCE);
  }
  logProcEnd (LOG_PROC, "confuiCreateTagListingDisp", "");
}

void
confuiCreateTagSelectedDisp (confuigui_t *gui)
{
  dispselsel_t  selidx;
  slist_t       *sellist;
  dispsel_t     *dispsel;

  logProcBegin (LOG_PROC, "confuiCreateTagSelectedDisp");

  selidx = uiSpinboxTextGetValue (
      gui->uiitem [CONFUI_SPINBOX_DISP_SEL].spinbox);

  dispsel = gui->dispsel;
  sellist = dispselGetList (dispsel, selidx);

  uiduallistSet (gui->dispselduallist, sellist, DUALLIST_TREE_TARGET);
  logProcEnd (LOG_PROC, "confuiCreateTagSelectedDisp", "");
}

char *
confuiGetLocalIP (confuigui_t *gui)
{
  char    *ip;

  if (gui->localip == NULL) {
    ip = webclientGetLocalIP ();
    gui->localip = mdstrdup (ip);
    mdfree (ip);
  }

  return gui->localip;
}

void
confuiLoadIntfcList (confuigui_t *gui, slist_t *interfaces,
    int svidx, int spinboxidx)
{
  slistidx_t  iteridx;
  const char  *desc;
  const char  *intfc;
  int         count;
  nlist_t     *tlist;
  nlist_t     *llist;

  tlist = nlistAlloc ("cu-i-list", LIST_UNORDERED, NULL);
  llist = nlistAlloc ("cu-i-list-l", LIST_UNORDERED, NULL);

  slistStartIterator (interfaces, &iteridx);
  count = 0;
  while ((desc = slistIterateKey (interfaces, &iteridx)) != NULL) {
    intfc = slistGetStr (interfaces, desc);
    if (intfc != NULL && strcmp (intfc, bdjoptGetStr (svidx)) == 0) {
      gui->uiitem [spinboxidx].listidx = count;
    }
    nlistSetStr (tlist, count, desc);
    nlistSetStr (llist, count, intfc);
    ++count;
  }
  nlistSort (tlist);
  nlistSort (llist);
  slistFree (interfaces);

  gui->uiitem [spinboxidx].displist = tlist;
  gui->uiitem [spinboxidx].sbkeylist = llist;
}

/* internal routines */

static nlist_t *
confuiGetThemeList (void)
{
  slist_t     *filelist = NULL;
  nlist_t     *themelist = NULL;
  char        tbuff [MAXPATHLEN];
  slist_t     *sthemelist = NULL;
  slistidx_t  iteridx;
  const char  *nm;
  int         count;


  logProcBegin (LOG_PROC, "confuiGetThemeList");
  sthemelist = slistAlloc ("cu-themes-s", LIST_ORDERED, NULL);
  themelist = nlistAlloc ("cu-themes", LIST_ORDERED, NULL);

  if (isWindows ()) {
    snprintf (tbuff, sizeof (tbuff), "%s/plocal/share/themes",
        sysvarsGetStr (SV_BDJ4_DIR_MAIN));
    filelist = dirlistRecursiveDirList (tbuff, DIRLIST_DIRS);
    confuiGetThemeNames (sthemelist, filelist);
    slistFree (filelist);
  } else {
    /* for macos */
    filelist = dirlistRecursiveDirList ("/opt/local/share/themes", DIRLIST_DIRS);
    confuiGetThemeNames (sthemelist, filelist);
    slistFree (filelist);

    filelist = dirlistRecursiveDirList ("/usr/share/themes", DIRLIST_DIRS);
    confuiGetThemeNames (sthemelist, filelist);
    slistFree (filelist);

    snprintf (tbuff, sizeof (tbuff), "%s/.themes", sysvarsGetStr (SV_HOME));
    filelist = dirlistRecursiveDirList (tbuff, DIRLIST_DIRS);
    confuiGetThemeNames (sthemelist, filelist);
    slistFree (filelist);
  }
  /* make sure the built-in themes are present */
  slistSetStr (sthemelist, "Adwaita", 0);
  /* adwaita-dark does not appear to work on macos w/macports */
  /* 4.3.3 adwaita-dark does not appear to work on windos either */
  if (isLinux ()) {
    slistSetStr (sthemelist, "Adwaita-dark", 0);
  }
  slistSetStr (sthemelist, "HighContrast", 0);
  slistSetStr (sthemelist, "HighContrastInverse", 0);
  slistSort (sthemelist);

  slistStartIterator (sthemelist, &iteridx);
  count = 0;
  while ((nm = slistIterateKey (sthemelist, &iteridx)) != NULL) {
    nlistSetStr (themelist, count++, nm);
  }

  slistFree (sthemelist);

  logProcEnd (LOG_PROC, "confuiGetThemeList", "");
  return themelist;
}

static slist_t *
confuiGetThemeNames (slist_t *themelist, slist_t *filelist)
{
  slistidx_t    iteridx;
  const char    *fn;
  pathinfo_t    *pi;
  static char   *srchdir = "gtk-3.0";
  char          tbuff [MAXPATHLEN];
  char          tmp [MAXPATHLEN];

  logProcBegin (LOG_PROC, "confuiGetThemeNames");
  if (filelist == NULL) {
    logProcEnd (LOG_PROC, "confuiGetThemeNames", "no-filelist");
    return NULL;
  }

  slistStartIterator (filelist, &iteridx);

  /* the key value used here is meaningless */
  while ((fn = slistIterateKey (filelist, &iteridx)) != NULL) {
    if (fileopIsDirectory (fn)) {
      pi = pathInfo (fn);
      if (pi->flen == strlen (srchdir) &&
          strncmp (pi->filename, srchdir, strlen (srchdir)) == 0) {
        strlcpy (tbuff, pi->dirname, pi->dlen + 1);
        pathInfoFree (pi);
        pi = pathInfo (tbuff);
        strlcpy (tmp, pi->filename, pi->flen + 1);
        slistSetStr (themelist, tmp, 0);
      }
      pathInfoFree (pi);
    } /* is directory */
  } /* for each file */

  logProcEnd (LOG_PROC, "confuiGetThemeNames", "");
  return themelist;
}

static char *
confuiMakeQRCodeFile (char *title, char *uri)
{
  char          *data;
  char          *ndata;
  char          *qruri;
  char          baseuri [MAXPATHLEN];
  char          tbuff [MAXPATHLEN];
  FILE          *fh;
  size_t        dlen;

  logProcBegin (LOG_PROC, "confuiMakeQRCodeFile");
  qruri = mdmalloc (MAXPATHLEN);

  pathbldMakePath (baseuri, sizeof (baseuri),
      "", "", PATHBLD_MP_DIR_TEMPLATE);
  pathbldMakePath (tbuff, sizeof (tbuff),
      "qrcode", BDJ4_HTML_EXT, PATHBLD_MP_DIR_TEMPLATE);

  data = filedataReadAll (tbuff, &dlen);
  ndata = regexReplaceLiteral (data, "#TITLE#", title);
  mdfree (data);
  data = ndata;
  ndata = regexReplaceLiteral (data, "#BASEURL#", baseuri);
  mdfree (data);
  data = ndata;
  ndata = regexReplaceLiteral (data, "#QRCODEURL#", uri);
  mdfree (data);

  pathbldMakePath (tbuff, sizeof (tbuff),
      "qrcode", BDJ4_HTML_EXT, PATHBLD_MP_DREL_TMP);
  fh = fileopOpen (tbuff, "w");
  dlen = strlen (ndata);
  fwrite (ndata, dlen, 1, fh);
  mdextfclose (fh);
  fclose (fh);
  /* windows requires an extra slash in front, and it doesn't hurt on linux */
  snprintf (qruri, MAXPATHLEN, "file:///%s/%s",
      sysvarsGetStr (SV_BDJ4_DIR_DATATOP), tbuff);

  mdfree (ndata);
  logProcEnd (LOG_PROC, "confuiMakeQRCodeFile", "");
  return qruri;
}

static void
confuiUpdateOrgExample (org_t *org, char *data, uiwcont_t *uiwidgetp)
{
  song_t    *song;
  char      *tdata;
  char      *disp;

  if (data == NULL || org == NULL) {
    return;
  }

  logProcBegin (LOG_PROC, "confuiUpdateOrgExample");
  tdata = mdstrdup (data);
  song = songAlloc ();
  songParse (song, tdata, 0);
  disp = orgMakeSongPath (org, song);
  pathDisplayPath (disp, strlen (disp));
  uiLabelSetText (uiwidgetp, disp);
  songFree (song);
  mdfree (disp);
  mdfree (tdata);
  logProcEnd (LOG_PROC, "confuiUpdateOrgExample", "");
}

bool
confuiOrgPathSelect (void *udata, long idx)
{
  confuigui_t *gui = udata;
  char        *sval = NULL;
  int         widx;

  logProcBegin (LOG_PROC, "confuiOrgPathSelect");
  widx = CONFUI_COMBOBOX_ORGPATH;
  sval = slistGetDataByIdx (gui->uiitem [widx].displist, idx);
  gui->uiitem [widx].listidx = idx;
  if (sval != NULL && *sval) {
    bdjoptSetStr (OPT_G_ORGPATH, sval);
  }
  confuiUpdateOrgExamples (gui, sval);
  logProcEnd (LOG_PROC, "confuiOrgPathSelect", "");
  return UICB_CONT;
}

static bool
confuiSearchDispSel (confuigui_t *gui, int selidx, const char *disp)
{
  dispsel_t     *dispsel;
  slist_t       *sellist;
  slistidx_t    iteridx;
  const char    *tkey;
  bool          found = false;

  dispsel = gui->dispsel;

  sellist = dispselGetList (dispsel, selidx);
  slistStartIterator (sellist, &iteridx);
  while ((tkey = slistIterateKey (sellist, &iteridx)) != NULL) {
    if (strcmp (tkey, disp) == 0) {
      found = true;
      break;
    }
  }

  return found;
}

static void
confuiCreateTagListingMultDisp (confuigui_t *gui, slist_t *dlist,
    int sidx, int eidx)
{
  slist_t       *editlist = NULL;
  const char    *disp;
  slistidx_t    iteridx;

  editlist = slistAlloc ("dyn-edit-tag-list", LIST_ORDERED, NULL);

  slistStartIterator (dlist, &iteridx);
  while ((disp = slistIterateKey (dlist, &iteridx)) != NULL) {
    bool    found;

    /* this is all very inefficient, as the dispsel lists are unordered */
    /* but the lists are relatively short */
    found = false;
    for (int i = sidx; i <= eidx; ++i) {
      found |= confuiSearchDispSel (gui, i, disp);
    }

    if (! found) {
      slistSetNum (editlist, disp, slistGetNum (dlist, disp));
    }
  }

  uiduallistSet (gui->dispselduallist, editlist, DUALLIST_TREE_SOURCE);
  slistFree (editlist);
}

