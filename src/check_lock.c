#include <stdio.h>
#include <check.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lock.h"
#include "bdjstring.h"
#include "check_bdj.h"

#define LOCK_FN "tmp/test_lock.lck"

START_TEST(lock_acquire_release)
{
  int           rc;
  struct stat   statbuf;
  FILE          *fh;
  pid_t         pid;
  pid_t         fpid;

  pid = getpid ();
  unlink (LOCK_FN);
  rc = lockAcquirePid (LOCK_FN, pid);
  ck_assert (rc >= 0);
  rc = stat (LOCK_FN, &statbuf);
  ck_assert (rc == 0);
  ck_assert (statbuf.st_size > 0);
  fh = fopen (LOCK_FN, "r");
  fscanf (fh, "%d", &fpid);
  fclose (fh);
  ck_assert (fpid == pid);
  rc = lockReleasePid (LOCK_FN, pid);
  ck_assert (rc == 0);
  rc = stat (LOCK_FN, &statbuf);
  ck_assert (rc < 0);
}
END_TEST

START_TEST(lock_already)
{
  int           rc;
  struct stat   statbuf;
  FILE          *fh;
  pid_t         pid;
  pid_t         fpid;

  pid = getpid ();
  unlink (LOCK_FN);
  rc = lockAcquirePid (LOCK_FN, pid);
  ck_assert (rc >= 0);
  rc = lockAcquirePid (LOCK_FN, pid);
  ck_assert (rc < 0);
  rc = lockReleasePid (LOCK_FN, pid);
  ck_assert (rc == 0);
}
END_TEST

START_TEST(lock_other_dead)
{
  int           rc;
  struct stat   statbuf;
  FILE          *fh;
  pid_t         pid;
  pid_t         fpid;

  pid = getpid ();
  unlink (LOCK_FN);
  rc = lockAcquirePid (LOCK_FN, 5);
  ck_assert (rc >= 0);
  rc = lockAcquirePid (LOCK_FN, pid);
  ck_assert (rc >= 0);
  rc = lockReleasePid (LOCK_FN, pid);
  ck_assert (rc == 0);
}
END_TEST

START_TEST(lock_unlock_fail)
{
  int           rc;
  struct stat   statbuf;
  FILE          *fh;
  pid_t         pid;
  pid_t         fpid;

  pid = getpid ();
  unlink (LOCK_FN);
  rc = lockAcquirePid (LOCK_FN, pid);
  ck_assert (rc >= 0);
  rc = lockReleasePid (LOCK_FN, 5);
  ck_assert (rc < 0);
  rc = lockReleasePid (LOCK_FN, pid);
  ck_assert (rc == 0);
}
END_TEST

Suite *
lock_suite (void)
{
  Suite     *s;
  TCase     *tc_lock;

  s = suite_create ("Lock Suite");
  tc_lock = tcase_create ("Lock");
  tcase_add_test (tc_lock, lock_acquire_release);
  tcase_add_test (tc_lock, lock_already);
  tcase_add_test (tc_lock, lock_other_dead);
  tcase_add_test (tc_lock, lock_unlock_fail);
  suite_add_tcase (s, tc_lock);
  return s;
}

