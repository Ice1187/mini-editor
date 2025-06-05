#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

// TODO:
// - [x] explore what c_lflag do we need
// - [x] explore what c_cflag do we need
// - [x] explore what c_oflag do we need
// - [x] explore what c_iflag do we need
// - [x] read ECMA-48 to understand ANSI control codes
// - [ ] render a empty screen without reading any file

struct termios saved_attributes;

void reset_input_mode (void) {
    tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
}

void set_input_mode (void) { /* {{{ */
    struct termios tattr;
    char *name;

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

    tcsetattr (STDIN_FILENO, TCSAFLUSH, &tattr);
} /* }}} */

int main (void)
{
    char c;

    set_input_mode ();

    while (1)
    {
        read (STDIN_FILENO, &c, 1);
        if (c == '\004') {        /* C-d */
            break;
        }
        else {
            putchar (c);
            printf("[%02hhx]\n", c);
        }
    }

    return EXIT_SUCCESS;
}

// vim: foldmethod=marker
