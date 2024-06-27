/*
 * Copyright 2021-2024 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_VALIDATE_H
#define INC_VALIDATE_H

enum {
  VAL_NONE          = 0,
  VAL_NOT_EMPTY     = (1 << 0),
  VAL_NO_SPACES     = (1 << 1),
  VAL_NO_SLASHES    = (1 << 2),
  VAL_NUMERIC       = (1 << 3),
  VAL_FLOAT         = (1 << 4),
  VAL_HOUR_MIN      = (1 << 5),
  VAL_MIN_SEC       = (1 << 6),
  VAL_HMS  = (1 << 7),
  VAL_HMS_PRECISE   = (1 << 8),
  VAL_NO_WINCHARS   = (1 << 9),
};

bool validate (char *buff, size_t sz, const char *label, const char *str, int flags);

#endif /* INC_VALIDATE_H */
