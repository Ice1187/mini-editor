#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/errno.h>
#include <sys/ioctl.h>

#define LINE_ENLARGE_SIZE 128
#define READ_INPUT_SIZE    10

#define  MOVE_CURSOR_UP        "\x1b[A"
#define  MOVE_CURSOR_DOWN      "\x1b[B"
#define  MOVE_CURSOR_RIGHT     "\x1b[C"
#define  MOVE_CURSOR_LEFT      "\x1b[D"
#define  MOVE_CURSOR_TOP_LEFT  "\x1b[H"
#define  ERASE_WHOLE_LINE      "\x1b[2K"
#define  ERASE_AFTER_CURSOR    "\x1b[0K"
#define  HIDE_CURSOR           "\x1b[?25l"
#define  SHOW_CURSOR           "\x1b[?25h"

#define ESC 0x1b
#define CSI 0x5b

#define EOT 0x04
#define  CR 0x0d
#define DEL 0x7f

/*
## TODO:
- [x] explore what c_lflag do we need
- [x] explore what c_cflag do we need
- [x] explore what c_oflag do we need
- [x] explore what c_iflag do we need
- [x] read ECMA-48 to understand ANSI control codes
- [x] render a empty screen without reading any file
- [x] move up/down/left/right on the empty screen
- [x] open, read and render file
- [x] move in file
- [x] edit file
  - [x] insert (not newline): insert and realloc
  - [x] delete (not newline): remove
  - [x] delete newline
  - [x] insert newline
- [ ] store content changes of editing file
- [ ] write changes to the file
- [ ] maybe we dont need to record cursor position, just use CUD/T/L/R
*/

struct line_t {
    char *buf;
    unsigned int len;
};

struct content_t {
    struct line_t *lines;
    unsigned int len;
    unsigned int cap;
};

struct termios saved_attributes;

unsigned short ws_row = 0, ws_col = 0;       // ws: window size
unsigned short cur_row = -1, cur_col = - 1;  // cur: cursor
unsigned   int ed_row = 0, ed_col = 0;      // ed: edit

char *file_path = NULL;
struct content_t content = {NULL, 0, 0};


void reset_input_mode(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes);
}

void set_input_mode(void) { /* {{{ */
    struct termios tattr;

    /* Make sure stdin is a terminal. */
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "[!] Not a terminal.\n");
        exit(EXIT_FAILURE);
    }

    if (tcgetattr(STDIN_FILENO, &saved_attributes) != 0) {
        fprintf(stderr, "[!] tcgetattr failed.\n");
        exit(EXIT_FAILURE);
    }

    /* Make sure to reset the terminal mode before exit */
    atexit(reset_input_mode);

    // FIXME: maybe we can achieve similar thing just by calling cfmakeraw()
    /* Terminal Modes
        * unset in Input Mode
          * BRKINT: becase there can be long time span between input for thinking what to type? but likely no difference on non-physical terminal
          * ICRNL: becase we want to let user type \r?
          * ISTRIP: to type non-ASCII char
          * IXON: to recieve START, STOP as ordinary char
        * unset in Output Mode
          * OPOST: because we want to have full control on how output looks like
        * set in Control mode
          * CS8: use 8-bit, but likely no difference on non-physical terminal
          * CREAD: we want to read input from the terminal, but likely no difference on non-physical terminal
        * unset in Local mode
          * ICANON: to put terminal in non-canonical mode
          * ECHO: dont echo the received input
          * ISIG: dont recognize SIGINT, SIGQUIT, SIGQUIT (ctrl-c/z/\)
          * IEXTEN: dont enable DISCARD and LNEXT
    */
    tcgetattr(STDIN_FILENO, &tattr);
    tattr.c_iflag &= ~(BRKINT | ICRNL | ISTRIP | IXON);
    //tattr.c_oflag &= ~(OPOST); // likely no difference on non-physical terminal
    // tattr.c_cflag |= (CS8 | CREAD); // likely no difference on non-physical terminal
    tattr.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
    // read wait (VTIME * 0.1) sec before return. if no char, it return 0
    tattr.c_cc[VMIN] = 0;
    tattr.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr) != 0) {
        fprintf(stderr, "[!] tcsetattr failed.\n");
        exit(EXIT_FAILURE);
    }
} /* }}} */

void set_no_buffering(void) {/* {{{ */
    int err;

    err = setvbuf(stdout, NULL, _IONBF, 0);
    if (err) {
        fprintf(stderr, "Failed to set STDOUT to no buffering: %d\n", err);
        exit(EXIT_FAILURE);
    }
}/* }}} */

void init(char *fp) {/* {{{ */
    ed_row = ed_col = 0;
    cur_row = cur_col = 0;
    file_path = fp;

    set_input_mode();
    set_no_buffering();
}/* }}} */

void cleanup(void) {/* {{{ */
    for ( ; content.len ; free((content.lines[--content.len]).buf) );
    free(content.lines);
}/* }}} */

void update_window_size(void) {/* {{{ */
    int err;
    struct winsize ws;

    err = ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
    if (err) {
        fprintf(stderr, "ioctl in get_window_size failed: %d\n", err);
        return;
    }

    ws_row = ws.ws_row;
    ws_col = ws.ws_col;

    /* printf("window size: %hd, %hd\n", ws_row, ws_col); */
}/* }}} */

void read_file_content_if_exist(void) {/* {{{ */
    FILE *fp = NULL;
    char *line = NULL;
    size_t line_cap = 0;
    ssize_t line_len = 0;
    unsigned int line_num = 0;

    content.len = 0;
    content.cap = LINE_ENLARGE_SIZE;
    content.lines = calloc(content.cap, sizeof(struct line_t));

    fp = fopen(file_path, "r");
    if (fp) {
        for (line_num = 0; (line_len = getline(&line, &line_cap, fp)) > 0; line_num++) {
            if (line_num >= content.cap) {
                content.cap += LINE_ENLARGE_SIZE;
                content.lines = realloc(content.lines, content.cap * sizeof(struct line_t));
            }

            // TODO: make sure every line end with newline
            content.lines[line_num].buf = line;
            content.lines[line_num].len = line_len + 1;  // include newline
            content.len += 1;

            line = NULL;   // FIXME: smell
            line_cap = 0;
        }

        fclose(fp);
    } else {
        content.lines[0].buf = calloc(2, sizeof(char));
        content.lines[0].buf[0] = '\n';
        content.lines[0].buf[1] = '\x00';
        content.lines[0].len = 2;
        content.len++;
    }

}/* }}} */

void update_cursor_pos_on_screen(void) {
    // cursor position in control code is 1-index
    printf("\x1b[%d;%dH", cur_row+1, cur_col+1);
}

void render_screen(void) {
    printf(HIDE_CURSOR);

    // clean the whole screen
    printf(MOVE_CURSOR_TOP_LEFT ERASE_WHOLE_LINE);
    for (unsigned short r = 1; r < ws_row; r++)
        printf("\n" ERASE_WHOLE_LINE);

    printf(MOVE_CURSOR_TOP_LEFT);
    for (unsigned short r = 0; r < ws_row; r++) {
        if (r < content.len) {
            printf("%s", content.lines[r].buf);
        } else {
            printf("~");
            printf((r == ws_row-1) ? "" : "\n");  // FIXME: smell
        }
    }

    update_cursor_pos_on_screen();

    printf(SHOW_CURSOR);
}

void render_init_screen(void) {
    // set cursor position
    cur_row = 0;
    cur_col = 0;

    update_window_size();

    render_screen();
}

ssize_t process_CSI_code(const char *c, ssize_t i, const ssize_t sz) {
    if (i == sz - 1) return i;
    if (c[i] != CSI) return i;

    i++;
    switch (c[i]) {
        case 'A':
            /* printf(MOVE_CURSOR_UP); */
            ed_row = (ed_row <= 0) ? 0 : (ed_row - 1);
            ed_col = (ed_col >= content.lines[ed_row].len - 2) ? (content.lines[ed_row].len - 2) : ed_col;
            cur_row = (cur_row <= 0) ? 0 : (cur_row - 1);
            cur_col = (cur_col >= ed_col) ? ed_col : cur_col;
            break;
        case 'B':
            /* printf(MOVE_CURSOR_DOWN); */
            ed_row = (ed_row >= content.len - 1) ? (content.len - 1) : (ed_row + 1);
            ed_col = (ed_col >= content.lines[ed_row].len - 2) ? (content.lines[ed_row].len - 2) : ed_col;
            cur_row = (cur_row >= ws_row - 1) ? (ws_row - 1) : (
                (cur_row >= ed_row) ? ed_row : (cur_row + 1)
            );
            cur_col = (cur_col >= ed_col) ? ed_col : cur_col;
            break;
        case 'C':
            /* printf(MOVE_CURSOR_RIGHT); */
            ed_col = (ed_col >= content.lines[ed_row].len - 2) ? (content.lines[ed_row].len - 2) : (ed_col + 1);
            cur_col = (cur_col >= ws_col - 1) ? (ws_col - 1) : (
                (cur_col >= ed_col) ? ed_col : (cur_col + 1)
            );
            break;
        case 'D':
            /* printf(MOVE_CURSOR_LEFT); */
            ed_col = (ed_col <= 0) ? 0 : (ed_col - 1);
            cur_col = (cur_col <= 0) ? 0 : (cur_col - 1);
            break;
        default:
            printf("[%02hhx]\n", c[i]);
            break;
    }

    return i;
}

void delete_newline(void) {
    unsigned int prev_line_len = content.lines[ed_row-1].len;

    // realloc the buf of the previous line
    content.lines[ed_row-1].buf = realloc(
        content.lines[ed_row-1].buf,
        content.lines[ed_row-1].len + content.lines[ed_row].len - 2
    );

    // concat the line to the previous line
    memmove(
        content.lines[ed_row-1].buf + content.lines[ed_row-1].len - 2,
        content.lines[ed_row].buf,
        content.lines[ed_row].len
    );
    content.lines[ed_row-1].len = content.lines[ed_row-1].len + content.lines[ed_row].len - 2;
    content.lines[ed_row-1].buf[content.lines[ed_row-1].len-1] = '\x00';

    // remove the deleted line in content.lines
    free(content.lines[ed_row].buf);
    memmove(
        content.lines + ed_row,
        content.lines + (ed_row + 1),
        (content.len - (ed_row + 1)) * sizeof(struct line_t)
    );
    content.len--;

    ed_row--;
    ed_col = prev_line_len - 2;
    cur_row--;
    cur_col = (ed_col >= ws_col - 1) ? (ws_col - 1) : ed_col;
}

void delete_char(void) {
    // delete on the start of line
    if (ed_col == 0) {
        if (ed_row == 0) return;
        delete_newline();
        return;
    }

    memmove(
        content.lines[ed_row].buf + ed_col - 1,
        content.lines[ed_row].buf + ed_col,
        content.lines[ed_row].len - ed_col
    );

    content.lines[ed_row].len--;
    content.lines[ed_row].buf[content.lines[ed_row].len-1] = '\x00';
    ed_col--;
    cur_col = (cur_col <= 0) ? 0 : (cur_col - 1);
}

void insert_newline(void) {
    // realloc content.lines and move the next line down
    if (content.len >= content.cap) {
        content.cap += LINE_ENLARGE_SIZE;
        content.lines = realloc(content.lines, content.cap * sizeof(struct line_t));
    }

    memmove(
        content.lines + (ed_row + 2),
        content.lines + (ed_row + 1),
        (content.len - (ed_row + 1)) * sizeof(struct line_t)
    );
    content.len++;

    // copy content to the next line
    content.lines[ed_row+1].buf = calloc(content.lines[ed_row].len - ed_col, sizeof(char));
    memcpy(
        content.lines[ed_row+1].buf,
        content.lines[ed_row].buf + ed_col,
        content.lines[ed_row].len - ed_col
    );
    content.lines[ed_row+1].len = content.lines[ed_row].len - ed_col;

    // add the newline and NULL back to the current line
    memmove(
        content.lines[ed_row].buf + ed_col,
        content.lines[ed_row].buf + content.lines[ed_row].len - 2,
        2
    );
    content.lines[ed_row].len = ed_col + 2;

    // adjust edit and cursor position
    ed_row++;
    ed_col = 0;
    cur_row = (ed_row >= ws_row - 1) ? (ws_row - 1) : ed_row;
    cur_col = 0;
}

void insert_char(char c) {
    content.lines[ed_row].buf = realloc(content.lines[ed_row].buf, content.lines[ed_row].len + 1);

    memmove(
        content.lines[ed_row].buf + ed_col + 1,
        content.lines[ed_row].buf + ed_col,
        content.lines[ed_row].len - ed_col
    );
    content.lines[ed_row].buf[ed_col] = c;

    content.lines[ed_row].len++;
    content.lines[ed_row].buf[content.lines[ed_row].len-1] = '\x00';

    ed_col++;
    cur_col = (cur_col >= ws_col - 1) ? (ws_col - 1) : (cur_col + 1);
}


bool process_keystroke(void) {
    char c[READ_INPUT_SIZE] = {0};
    ssize_t sz = 0;

    while (!sz) sz = read(STDIN_FILENO, c, READ_INPUT_SIZE);
    if (sz == -1) {
        fprintf(stderr, "Failed to read the input byte\n");
        exit(EXIT_FAILURE);
    }

    for (ssize_t i = 0; i < sz; i++) {
        switch (c[i]) {
            case EOT:
                return 1;
            case ESC:
                if (i == sz - 1) break; // normal ESC
                if (c[i+1] == CSI) {
                    i++;
                    i = process_CSI_code(c, i, sz);
                } else {
                    // TODO: handle non-CSI control code
                    fprintf(stderr, "Non-CSI control code: %hhx, abort\n", c[i+1]);
                    exit(EXIT_FAILURE);
                }
                break;
            case DEL:
                delete_char();
                break;
            case CR:
                insert_newline();
                break;
            default:
                /* fprintf(stderr, "[%02hhx]\n", c[i]); */
                /* exit(1); */
                insert_char(c[i]);
                break;
        }
    }

    return 0;
}

int main(int argc, char* argv[])
{
    bool stop;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return EXIT_FAILURE;
    }

    init(argv[1]);
    read_file_content_if_exist();
    render_init_screen();

    stop = 0;
    while (!stop) {
        stop = process_keystroke();
        render_screen();
    }

    cleanup();

    return EXIT_SUCCESS;
}

// vim: foldmethod=marker
