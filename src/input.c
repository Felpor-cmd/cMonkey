#include "input.h"

#include <ctype.h>
#include <unistd.h>

InputEvent input_read_event(void)
{
    unsigned char c = 0;
    const ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) {
        return (InputEvent){ .type = INPUT_NONE, .ch = 0 };
    }

    if (c == 3) {
        return (InputEvent){ .type = INPUT_CTRL_C, .ch = 0 };
    }
    if (c == 127 || c == 8) {
        return (InputEvent){ .type = INPUT_BACKSPACE, .ch = 0 };
    }
    if (c == '\r' || c == '\n') {
        return (InputEvent){ .type = INPUT_ENTER, .ch = 0 };
    }
    if (c == 27) {
        return (InputEvent){ .type = INPUT_ESCAPE, .ch = 0 };
    }
    if (isprint(c) || c == ' ') {
        return (InputEvent){ .type = INPUT_CHAR, .ch = (char)c };
    }
    return (InputEvent){ .type = INPUT_NONE, .ch = 0 };
}
