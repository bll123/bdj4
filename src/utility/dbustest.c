/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "dbus.h"

enum {
  MAXSZ = 80,
};

static void getstr (char *str, size_t sz);
static void prompt (const char *str);
static void getstd (char *bus, char *orgpath, char *intfc, char *method);

int
main (int argc, char *argv [])
{
  dbus_t    *dbus;
  char      cmd [MAXSZ];
  char      bus [MAXSZ];
  char      orgpath [MAXSZ];
  char      intfc [MAXSZ];
  char      method [MAXSZ];
  char      data [MAXSZ];

  dbus = dbusConnInit ();
  if (dbus == NULL) {
    fprintf (stderr, "could not connect\n");
    return 1;
  }

  while (1) {
//    prompt (": ");
    getstr (cmd, sizeof (cmd));

    if (strcmp (cmd, "exit") == 0 ||
        strcmp (cmd, "quit") == 0) {
      break;
    }
    if (strcmp (cmd, "init") == 0) {
      dbusMessageInit (dbus);
    }
    if (strcmp (cmd, "data") == 0) {
      getstr (data, sizeof (data));
      dbusMessageSetData (dbus, data, NULL);
    }
    if (strcmp (cmd, "call") == 0) {
      getstd (bus, orgpath, intfc, method);
      if (! *bus || ! *orgpath || ! *intfc || ! *method) {
        continue;
      }

      dbusMessage (dbus, bus, orgpath, intfc, method);
    }
  }

  dbusConnClose (dbus);
  return 0;
}

static void
getstr (char *str, size_t sz)
{
  size_t    len;

  *str = '\0';
  (void) ! fgets (str, sz, stdin);
  len = strlen (str);
  --len;
  str [len] = '\0';
}

static void
prompt (const char *str)
{
  fprintf (stdout, "%s", str);
  fflush (stdout);
}

static void
getstd (char *bus, char *orgpath, char *intfc, char *method)
{
//  prompt ("bus: ");
  getstr (bus, MAXSZ);
//  prompt ("orgpath: ");
  getstr (orgpath, MAXSZ);
//  prompt ("intfc: ");
  getstr (intfc, MAXSZ);
//  prompt ("method: ");
  getstr (method, MAXSZ);
}