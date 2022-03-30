#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <curl/curl.h>

#include "log.h"
#include "sysvars.h"
#include "webclient.h"

enum {
  INIT_NONE,
  INIT_CLIENT,
};

static size_t webclientCallback (char *ptr, size_t size,
    size_t nmemb, void *userdata);
static size_t webclientNullCallback (char *ptr, size_t size,
    size_t nmemb, void *userdata);
static int webclientDebugCallback (CURL *curl, curl_infotype type, char *data,
    size_t size, void *userptr);
static void webclientSetUserAgent (CURL *curl);

static int initialized = INIT_NONE;

webclient_t *
webclientPost (webclient_t *webclient, char *uri, char *query,
    void *userdata, webclientcb_t callback)
{
  if (webclient == NULL) {
    webclient = malloc (sizeof (webclient_t));
    assert (webclient != NULL);
    webclient->userdata = userdata;
    webclient->callback = callback;

    if (! initialized) {
      curl_global_init (CURL_GLOBAL_ALL);
      initialized = INIT_CLIENT;
    }
    webclient->curl = curl_easy_init ();
    assert (webclient->curl != NULL);
    if (logCheck (LOG_DBG, LOG_WEBCLIENT)) {
      curl_easy_setopt (webclient->curl, CURLOPT_DEBUGFUNCTION, webclientDebugCallback);
      curl_easy_setopt (webclient->curl, CURLOPT_VERBOSE, 1);
    }
    curl_easy_setopt (webclient->curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt (webclient->curl, CURLOPT_TCP_KEEPALIVE, 1);
    curl_easy_setopt (webclient->curl, CURLOPT_WRITEDATA, webclient);
    curl_easy_setopt (webclient->curl, CURLOPT_WRITEFUNCTION, webclientCallback);
    webclientSetUserAgent (webclient->curl);
  }

  curl_easy_setopt (webclient->curl, CURLOPT_URL, uri);
  curl_easy_setopt (webclient->curl, CURLOPT_POSTFIELDS, query);
  curl_easy_perform (webclient->curl);
  return webclient;
}

void
webclientClose (webclient_t *webclient)
{
  if (webclient != NULL) {
    if (webclient->curl != NULL) {
      curl_easy_cleanup (webclient->curl);
    }
    curl_global_cleanup ();
    free (webclient);
    initialized = INIT_NONE;
  }
}

char *
webclientGetLocalIP (void)
{
  char  *ip;
  CURL  *curl;

  if (initialized == INIT_NONE) {
    curl_global_init (CURL_GLOBAL_ALL);
  }
  curl = curl_easy_init ();
  curl_easy_setopt (curl, CURLOPT_URL, "https://ballroomdj.org/bdj4version.txt");
  curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt (curl, CURLOPT_WRITEDATA, NULL);
  curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, webclientNullCallback);
  webclientSetUserAgent (curl);
  if (logCheck (LOG_DBG, LOG_WEBCLIENT)) {
    curl_easy_setopt (curl, CURLOPT_DEBUGFUNCTION, webclientDebugCallback);
    curl_easy_setopt (curl, CURLOPT_VERBOSE, 1);
  }
  curl_easy_perform (curl);
  curl_easy_getinfo (curl, CURLINFO_LOCAL_IP, &ip);
  if (initialized == INIT_NONE) {
    curl_global_cleanup ();
  }

  return ip;
}

/* internal routines */

static size_t
webclientCallback (char *ptr, size_t size, size_t nmemb, void *userdata)
{
  webclient_t     *webclient = userdata;

  webclient->callback (webclient->userdata, ptr, nmemb);
  return nmemb;
}

static size_t
webclientNullCallback (char *ptr, size_t size, size_t nmemb, void *userdata)
{
  return nmemb;
}

static int
webclientDebugCallback (CURL *curl, curl_infotype type, char *data,
    size_t size, void *userptr)
{
  char    *p;
  char    *tokstr;
  char    *tag;

  if (type == CURLINFO_SSL_DATA_OUT ||
      type == CURLINFO_SSL_DATA_IN) {
    return 0;
  }

  switch (type) {
    case CURLINFO_SSL_DATA_IN:
    case CURLINFO_SSL_DATA_OUT:
    case CURLINFO_END: {
      break;
    }
    case CURLINFO_TEXT: {
      tag = "Info";
      break;
    }
    case CURLINFO_HEADER_IN: {
      tag = "Header-In";
      break;
    }
    case CURLINFO_HEADER_OUT: {
      tag = "Header-Out";
      break;
    }
    case CURLINFO_DATA_IN: {
      tag = "Data-In";
      break;
    }
    case CURLINFO_DATA_OUT: {
      tag = "Data-Out";
      break;
    }
  }

  p = strtok_r (data, "\r\n", &tokstr);
  while (p != NULL && p < data + size) {
    logMsg (LOG_DBG, LOG_WEBCLIENT, "curl: %s: %s", tag, p);
    p = strtok_r (NULL, "\r\n", &tokstr);
  }
  return 0;
}

static void
webclientSetUserAgent (CURL *curl)
{
  char  tbuff [100];

  snprintf (tbuff, sizeof (tbuff),
      "BDJ4/%s ( %s/ )", sysvarsGetStr (SV_BDJ4_VERSION),
      sysvarsGetStr (SV_WEB_HOST));
  curl_easy_setopt (curl, CURLOPT_USERAGENT, tbuff);
}
