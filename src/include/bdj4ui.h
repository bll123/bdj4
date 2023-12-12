/*
 * Copyright 2021-2023 Brad Lanam Pleasant Hill CA
 */
#ifndef INC_BDJ4UI_H
#define INC_BDJ4UI_H

/* various dynamic options */
enum {
  PLUI_HIDE_MARQUEE_ON_START,
  PLUI_POSITION_X,
  PLUI_POSITION_Y,
  PLUI_SHOW_EXTRA_QUEUES,
  PLUI_SIZE_X,
  PLUI_SIZE_Y,
  PLUI_SWITCH_QUEUE_WHEN_EMPTY,
  PLUI_WORKSPACE,
  PLUI_EXP_MP3_DIR,
  PLUI_RESTART_POSITION,
  SONGSEL_FILTER_POSITION_X,
  SONGSEL_FILTER_POSITION_Y,
  SONGSEL_FILTER_WORKSPACE,
  SONGSEL_SORT_BY,
  MANAGE_SELFILE_POSITION_X,
  MANAGE_SELFILE_POSITION_Y,
  MANAGE_SELFILE_WORKSPACE,
  MANAGE_SBS_SONGLIST,
  MANAGE_CFPL_POSITION_X,
  MANAGE_CFPL_POSITION_Y,
  MANAGE_CFPL_WORKSPACE,
  MANAGE_POSITION_X,
  MANAGE_POSITION_Y,
  MANAGE_SIZE_X,
  MANAGE_SIZE_Y,
  MANAGE_WORKSPACE,
  REQ_EXT_POSITION_X,
  REQ_EXT_POSITION_Y,
  REQ_EXT_DIR,
  APPLY_ADJ_POSITION_X,
  APPLY_ADJ_POSITION_Y,
  COPY_TAGS_POSITION_X,
  COPY_TAGS_POSITION_Y,
  EXP_IMP_BDJ4_POSITION_X,
  EXP_IMP_BDJ4_POSITION_Y,
  EXP_MP3_DIR,
  MANAGE_EXP_BDJ4_DIR,
  MANAGE_IMP_BDJ4_DIR,
  MANAGE_AUDIOID_PANE_POSITION,
  QE_POSITION_X,
  QE_POSITION_Y,
  /* the reload data is in its own data file, not the options file */
  RELOAD_CURR,
  PLUI_KEY_MAX,
};

enum {
  SPD_DIGITS = 0,
  VOL_ADJ_DIGITS = 1,
  VOL_DIGITS = 0,
};

#define SPD_LOWER 70.0
#define SPD_UPPER 130.0
#define SPD_INCA 1.0
#define SPD_INCB 5.0
#define VOL_ADJ_LOWER -50.0
#define VOL_ADJ_UPPER 50.0
#define VOL_ADJ_INCA 0.1
#define VOL_ADJ_INCB 5.0
#define VOL_LOWER 0.0
#define VOL_UPPER 100.0
#define VOL_INCA 1.0
#define VOL_INCB 10.0

#endif /* INC_BDJ4UI_H */
