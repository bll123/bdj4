/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma gcc diagnostic push
#pragma clang diagnostic ignored "-Wformat-extra-args"
#pragma gcc diagnostic ignored "-Wformat-extra-args"

#include <check.h>

#include "check_bdj.h"
#include "log.h"
#include "vsencdec.h"

typedef struct {
  const char  *d;
} vsec_t;

vsec_t tests [] = {
  { "abcdef", },
  { "12345693", },
  { ".,mdfvda", },
};

enum {
  TEST_SZ = sizeof (tests) / sizeof (vsec_t),
};

START_TEST(vsencdec_encdec)
{
  char  dbuff [50];
  char  ebuff [50];

  logMsg (LOG_DBG, LOG_IMPORTANT, "--chk-- vsencdec_encdec");

  for (int i = 0; i < TEST_SZ; ++i) {
    vsencdec (tests [i].d, ebuff, sizeof (ebuff));
    vsencdec (ebuff, dbuff, sizeof (dbuff));
    // fprintf (stderr, "t: %s e: %s d: %s\n", tests [i].d, ebuff, dbuff);
    ck_assert_str_eq (tests [i].d, dbuff);
  }
}
END_TEST


Suite *
vsencdec_suite (void)
{
  Suite     *s;
  TCase     *tc;

  s = suite_create ("vsencdec");
  tc = tcase_create ("vsencdec");
  tcase_add_test (tc, vsencdec_encdec);
  suite_add_tcase (s, tc);
  return s;
}
