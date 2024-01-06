/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma GCC diagnostic push
#pragma clang diagnostic ignored "-Wformat-extra-args"
#pragma GCC diagnostic ignored "-Wformat-extra-args"

#include <check.h>

#include "bdj4.h"
#include "bdjstring.h"
#include "log.h"
#include "pathinfo.h"
#include "check_bdj.h"
#include "mdebug.h"

typedef struct {
  char    *path;
  size_t  dlen;
  size_t  flen;
  size_t  blen;
  size_t  elen;
} ftest_t;

static ftest_t tests [] = {
  { "profile", 0, 7, 7, 0 },
  { "01 Bolero.mp3", 0, 13, 9, 4 },
  { "/usr/binx", 4, 4, 4, 0 },
  { "/usr", 1, 3, 3, 0 },
  { "/home/bll/stuff.txt", 9, 9, 5, 4 },
  { "/home/bll/stuff/", 9, 5, 5, 0 },
  { "/home/bll/stuff", 9, 5, 5, 0 },
  { "/home/bll/stuff.x", 9, 7, 5, 2 },
  { "/home/bll/x.stuff", 9, 7, 1, 6 },
  { "/home/bll/stuff.x/", 9, 7, 7, 0 },   // trailing slash, is dir
//   1234567890123456789012345678901234567890
  { "/home/bll/s/bdj4/src", 16, 3, 3, 0 },
  { "bdjconfig.txt.g", 0, 15, 13, 2 },
  { "dances.txt.nl", 0, 13, 10, 3 },
//   1234567890123456789012345678901234567890
  { "/usr/share/themes/Adwaita-dark/gtk-3.0", 30, 7, 5, 2 },
//   123456789012345678901234567 1234567890123456789012
  { "/home/bll/s/bdj4/test-music/001-argentinetango.mp3", 27, 22, 18, 4 },
  { "/", 1, 1, 1, 0 },
};
enum {
  TCOUNT = (sizeof(tests)/sizeof (ftest_t))
};

START_TEST(pathinfo_chk)
{
  pathinfo_t    *pi;
  char          tbuff [MAXPATHLEN];
  char          tmp [MAXPATHLEN];

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- pathinfo_chk");
  mdebugSubTag ("pathinfo_chk");

  for (int64_t i = 0; i < TCOUNT; ++i) {
    pi = pathInfo (tests[i].path);
    if (pi->dlen > 0) {
      ck_assert_msg (pi->dirname != NULL,
          "dirname: %s: i:%" PRId64 " have: %" PRId64 " want: %" PRId64,
          "dlen", i, (int64_t) pi->dlen, (int64_t) tests[i].dlen);
    }
    if (pi->flen > 0) {
      ck_assert_msg (pi->filename != NULL,
          "filename: %s: i:%" PRId64 " have: %" PRId64 " want: %" PRId64,
          "flen", i, (int64_t) pi->flen, (int64_t) tests[i].flen);
    }
    if (pi->blen > 0) {
      ck_assert_msg (pi->basename != NULL,
          "basename: %s: i:%" PRId64 " have: %" PRId64 " want: %" PRId64,
          "blen", i, (int64_t) pi->blen, (int64_t) tests[i].blen);
    }
    if (pi->elen > 0) {
      ck_assert_msg (pi->extension != NULL,
          "extension: %s: i:%" PRId64 " have: %" PRId64 " want: %" PRId64,
          "elen", i, (int64_t) pi->elen, (int64_t) tests[i].elen);
    }
    ck_assert_msg (pi->dlen == tests[i].dlen,
        "dlen: %s: i:%" PRId64 " have: %" PRId64 " want: %" PRId64,
        "dlen", i, (int64_t) pi->dlen, (int64_t) tests[i].dlen);
    ck_assert_msg (pi->flen == tests[i].flen,
        "flen: %s: i:%" PRId64 " have: %" PRId64 " want: %" PRId64,
        "flen", i, (int64_t) pi->flen, (int64_t) tests[i].flen);
    ck_assert_msg (pi->blen == tests[i].blen,
        "blen: %s: i:%" PRId64 " have: %" PRId64 " want: %" PRId64,
        "blen", i, (int64_t) pi->blen, (int64_t) tests[i].blen);
    ck_assert_msg (pi->elen == tests[i].elen,
        "elen: %s: i:%" PRId64 " have: %" PRId64 " want: %" PRId64,
        "elen", i, (int64_t) pi->elen, (int64_t) tests[i].elen);

    if (pi->dlen > 0) {
      pathInfoGetDir (pi, tbuff, sizeof (tbuff));
      strlcpy (tmp, tests [i].path, sizeof (tmp));
      tmp [pi->dlen] = '\0';
      ck_assert_str_eq (tbuff, tmp);
    }

    if (pi->elen > 0) {
      pathInfoGetExt (pi, tbuff, sizeof (tbuff));
      /* pi->extension is null terminated, as it is the last */
      ck_assert_str_eq (tbuff, pi->extension);
    }

    /* the dirname pointer always points to the beginning */
    ck_assert_ptr_eq (pi->dirname, tests [i].path);

    /* check against self */
    ck_assert_int_eq (pathInfoExtCheck (pi, pi->extension), 1);
    ck_assert_int_eq (pathInfoExtCheck (pi, ".junk"), 0);

    pathInfoFree (pi);
  }
}
END_TEST

Suite *
pathinfo_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("pathinfo");
  tc = tcase_create ("pathinfo");
  tcase_set_tags (tc, "libcommon");
  tcase_add_test (tc, pathinfo_chk);
  suite_add_tcase (s, tc);
  return s;
}


#pragma clang diagnostic pop
#pragma GCC diagnostic pop