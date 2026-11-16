#ifndef INPUT_EVENTS_H
#define INPUT_EVENTS_H
/* 端子扫描事件独立于面板按键5/6；数值由原BIN确认。 */
typedef enum {
  TERMINAL_EVENT_NONE = 0,
  TERMINAL_EVENT_START = 7,
  TERMINAL_EVENT_STOP = 8
} TerminalEvent;
#endif
