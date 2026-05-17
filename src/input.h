#ifndef CMONKEY_INPUT_H
#define CMONKEY_INPUT_H

typedef enum {
    INPUT_NONE = 0,
    INPUT_CHAR = 1,
    INPUT_BACKSPACE = 2,
    INPUT_ENTER = 3,
    INPUT_ESCAPE = 4,
    INPUT_CTRL_C = 5
} InputType;

typedef struct {
    InputType type;
    char ch;
} InputEvent;

/** Read one input event from stdin in non-blocking mode. */
InputEvent input_read_event(void);

#endif
