#pragma once

/* Message keys: cmd=0, listIndex=1, taskIndex=2, text=3, hasCompleted=4 (package.json) */

#define CMD_W_ASK_LISTS 1
#define CMD_W_ASK_OPEN 2
#define CMD_W_ADD_LIST 3
#define CMD_W_ADD_TASK 4
#define CMD_W_COMPLETE 5
#define CMD_W_DELETE_TASK 6
#define CMD_W_CLEAR_COMPLETED 7
#define CMD_W_DELETE_LIST 8
#define CMD_W_ASK_COMPLETED 9
#define CMD_W_DELETE_COMPLETED_TASK 10
#define CMD_W_UNCOMPLETE_COMPLETED 11
#define CMD_W_SET_TASK_DUE 12
#define CMD_W_CLEAR_TASK_DUE 13
#define CMD_W_PIN_TASK 14

#define R_LISTS 20
#define R_OPEN_TASKS 21
#define R_COMPLETED_TASKS 22
#define R_TOAST 23

#define MAX_MENU_LINES 20
#define TEXT_BUF 512
#define DICTATION_BUF 384
