#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>

#define LINE_ENLARGE_SIZE 128

#define MOVE_CURSOR_TOP_LEFT "\x1b[H"
#define ERASE_WHOLE_LINE "\x1b[2K"
#define ERASE_AFTER_CURSOR "\x1b[0K"
#define HIDE_CURSOR "\x1b[?25l"
#define SHOW_CURSOR "\x1b[?25h"

// TODO:
// - [x] explore what c_lflag do we need
// - [x] explore what c_cflag do we need
// - [x] explore what c_oflag do we need
// - [x] explore what c_iflag do we need
// - [x] read ECMA-48 to understand ANSI control codes
// - [x] render a empty screen without reading any file
// - [x] move up/down/left/right on the empty screen
// - [x] open, read and render file
// - [ ] move and edit file
// - [ ] store content changes of editing file
// - [ ] write changes to the file


struct line_t {
    char *buf;
    unsigned int len;
};


struct termios saved_attributes;
unsigned short ws_row, ws_col;
struct line_t *lines;
unsigned int num_of_lines;


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
    tattr.c_cc[VMIN] = 1;
    tattr.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr) != 0) {
        fprintf(stderr, "[!] tcsetattr failed.\n");
        exit(EXIT_FAILURE);
    }
} /* }}} */

void set_no_buffering(void) {/* {{{ */
    int err;

    err = setvbuf(stdout, NULL, _IONBF, 0);
    if (err) {
        fprintf(stderr, "setvbuf in set_no_buffering failed: %d\n", err);
        exit(EXIT_FAILURE);
    }
}/* }}} */

void init(void) {
    set_input_mode();
    set_no_buffering();
}

void cleanup() {
    for ( ; num_of_lines ; free((lines++)->buf), num_of_lines--);
    free(lines);
}

void update_window_size(void) {
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
}

void read_file_to_edit(const char *path) {
    FILE *fp = NULL;
    char *line = NULL;
    size_t line_cap = 0;
    ssize_t line_len = 0;
    unsigned int line_num = 0;

    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to read the file: %s\n", path);
        exit(EXIT_FAILURE);
    }

    // FIXME: can be easier
    lines = calloc(LINE_ENLARGE_SIZE, sizeof(struct line_t));
    num_of_lines = LINE_ENLARGE_SIZE;
    for (line_num = 0; (line_len = getline(&line, &line_cap, fp)) > 0; line_num++) {
        if (line_num >= num_of_lines) {
            lines = realloc(lines, (num_of_lines + LINE_ENLARGE_SIZE) * sizeof(struct line_t));
            num_of_lines = num_of_lines + LINE_ENLARGE_SIZE;
        }

        lines[line_num].buf = line;
        lines[line_num].len = line_len;

        line = NULL;
        line_cap = 0;
    }

    lines = realloc(lines, line_num * sizeof(struct line_t));
    num_of_lines = line_num;

    fclose(fp);
}

void render_init_screen() {
    update_window_size();

    printf(HIDE_CURSOR);

    // clean the whole screen
    printf(MOVE_CURSOR_TOP_LEFT ERASE_WHOLE_LINE);
    for (unsigned short r = 1; r < ws_row; r++)
        printf("\n" ERASE_WHOLE_LINE);

    // render content
    printf(MOVE_CURSOR_TOP_LEFT);
    for (unsigned short r = 0; r < ws_row; r++) {
        if (r < num_of_lines) {
            printf("%s", lines[r].buf);
        } else {
            printf("~");
            printf((r == ws_row-1) ? "" : "\n");  // FIXME: smell
        }
    }

    // put and show cursor
    printf(MOVE_CURSOR_TOP_LEFT SHOW_CURSOR);
}

int main(int argc, char* argv[])
{
    char c;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return EXIT_FAILURE;
    }

    init();
    read_file_to_edit(argv[1]);
    render_init_screen();

    while (1)
    {
        read(STDIN_FILENO, &c, 1);
        if (c == '\004') {        /* C-d */
            break;
        }
        else {
            putchar(c);
            /* printf("[%02hhx]\n", c); */
        }
    }

    cleanup();

    return EXIT_SUCCESS;
}

// vim: foldmethod=marker
