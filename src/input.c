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
        unsigned char seq1 = 0;
        unsigned char seq2 = 0;
        if (read(STDIN_FILENO, &seq1, 1) > 0 && (seq1 == '[' || seq1 == 'O') && read(STDIN_FILENO, &seq2, 1) > 0) {
            if (seq2 == 'A') {
                return (InputEvent){ .type = INPUT_ARROW_UP, .ch = 0 };
            }
            if (seq2 == 'B') {
                return (InputEvent){ .type = INPUT_ARROW_DOWN, .ch = 0 };
            }
            if (seq2 == 'C') {
                return (InputEvent){ .type = INPUT_ARROW_RIGHT, .ch = 0 };
            }
            if (seq2 == 'D') {
                return (InputEvent){ .type = INPUT_ARROW_LEFT, .ch = 0 };
            }
        }
        return (InputEvent){ .type = INPUT_ESCAPE, .ch = 0 };
    }
    if (isprint(c) || c == ' ') {
        return (InputEvent){ .type = INPUT_CHAR, .ch = (char)c };
    }
    return (InputEvent){ .type = INPUT_NONE, .ch = 0 };
}
