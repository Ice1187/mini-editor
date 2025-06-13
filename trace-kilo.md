# Trace Code of `kilo`

## `struct editorConfig E`
- `cx,cy`: Cursor x and y position in characters
- `rowoff`: Offset of row displayed.
- `coloff`: Offset of column displayed.
- `screenrows`: Number of rows that we can show
- `screencols`: Number of cols that we can show
- `numrows`: Number of rows of the editing file

## `struct abuf`
> This is basically a class in C.
- `abAppend(abuf, s, len)`: append string `s` (length `len`) to the buffer `abuf`
- `abFree(abuf)`: free the buffer `abuf`


## `getCursorPosition`<!-- {{{ -->
- write `ESC [6n` to `STDOUT_FILENO` to get the horizontal cursor position
    - TODO: what is `[6n`?
    - TODO: why does writing to `STDOUT_FILENO` and readint from `STDIN_FILENO` suppose to do?
- read the ASNI code response from `STDIN_FILENO`
- parse the cursor values <!--}}} -->

## `getWindowSize`<!-- {{{ -->
- get the window size with `ioctl(TIOCGWINSZ)`
- if `ioctl` failed, use ANSI code `ESC [999C ESC [999B` and `getCursorPosition` to get the window size
    - TODO: I assumed `[999C` and `[999B` is moving the cursor to bottom right ? <!-- }}} -->

## `disableRawMode`<!-- {{{ -->
- if raw mode is enable, call `tcsetattr(TCSAFLUSH, orig_termios)` to disable raw mode <!-- }}} -->

## `handleSigWinCh`
- `updateWindowSize` and `editorRefreshScreen`

## `enableRawMode`<!-- {{{ -->
> [!TIP]
> All the settings are documented in [The GNU C Library Reference Manual: Low-Level Terminal Interface](https://www.gnu.org/software/libc/manual/html_node/Low_002dLevel-Terminal-Interface.html)
> "Raw mode" is documented at [here](https://www.gnu.org/software/libc/manual/html_node/Noncanonical-Input.html#index-cfmakeraw)

- verify `STDIN_FILENO` is a tty
- `atexit`: register `editorAtExit` to disable raw mode before exit
- `tcgetattr(STDIN_FILENO)`: get termios struct of the current terminal
- set termios's control modes and characters, see `man termios`
- `tcsetattr`: set the new termios to the terminal <!-- }}} -->

## `editorRefreshScreen`<!-- {{{ -->
- hide cursor with `ESC [?25l` (TODO: what is this ANSI code)
- go home with `ESC [H` (TODO: what is this ANSI code)

### Draw all the texts on the screen
- `for (E.screenrows)`: loop through the rows of the screen:
    - if the editing file has no row, show welcome string at the middle of the screen
    - else if the number of rows of the editing file is less than the rows of the screen, show `~` on non-exist rows.
    - else show the text on the row with syntax highlight
    - some `ESC [39m`, `ESC [0K` (TODO: what is this ANSI code)

### Draw the status bar
- draw the 1st row of the status bar, which should be `{filename} - {number of lines} lines`
    - draw sth with `[0K`, `[7m` (TODO: what is this ANSI code)
    - `snprintf`: format the status string
        - `{filename} - {num of lines} lines [(modified)]        {row of cursor}/{?}`
    - truncate the status string to the width/column of the screen
    - pad space and write the right part of the status bar `{row of cursor}/{?}`
- `ESC [0m\r\n` to goto next line
- draw the 2nd row of the status bar, which should show the msg depend on the editor status

### Put the cursor to its position
- calculate the cursor position
- draw the cursor by writing `ESC [%d;%dH` and `ESC [?25h` to STDOUT (TODO: what is this ANSI code)
    - `ESC [%d;%dH` seems to be the position of cursor
    - `ESC [?25h` is show cursor <!-- }}} -->

## `editorReadKey`<!-- {{{ -->
- keep reading from STDIN (unnecessary busy loop?), until there's a character
- if the character is not `ESC`, return it
- otherwise (the character is `ESC`)
    - look ahead at most 3 character to see if it's a ANSI code. If it is, do the ANSI code stuff
    - NOTE: not able to trigger this by manually typing `ESC [C`, maybe cannot trigger this way? <!-- }}} -->


## `editorProcessKeypress`
- `editorReadKey`: read a typed key
- if the key is: 
    - `ctrl-c` ignore it
    - `ENTER`: for newline
    - `DELETE` for delete
    - `ARROW UP/DOWN/LEFT/RIGHT` for moving cursor
    - `ctrl-q` for quit
    - `ctrl-s` for save
    - `ctrl-f` for find
    - `ctrl-l` for clear screen (why?
    - `ESC` for switching mode (not implemente yet)
- otherwise, insert the typed character

## `Main`
- [x] `initEditor`
    - set init values of editor config `E`
    - `updateWindowSize`
    - setup handler `handleSigWinCh` for `SIGWINCH` signal
- [ ]~~`editorSelectSyntaxHighlight`~~
- [x] `editorOpen`
    - open the provided file
    - read each line
        - put NULL to each non-empty line
        - `editorInsertRow`: insert the line into the editor
    - close the provided file
- [x] `enableRawMode`
- [x] `editorSetStatusMessage`
    - set the status msg at the end of the screen
- [ ] infinite loop (draw -> wait for user input -> process input -> draw)
    - [x] `editorRefreshScreen`
    - [ ] `editorProcessKeypress`

# ANSI Control Code
- xx/yy
    - xx represents the column number from 00 to 07
    - yy represents the row number from 00 to 15
- 01/11 (0x1b) - ESC
- the control modes of kilo is set to 8 bit (by `raw.c_cflag |= (CS8)`)
- What are these codes:
    - `[6n` (suppose get cursor horizontal position)
    - `[999C`, `[999B` (suppose sth like move to bottom right of the window)
    - `[?25l`, `[H` (suppose "hide cursor", "go home")
    - `[39m`, `[7m`, `[0K`
    - `[?%d;%dH` (suppose sth like put cursor at (%d, %d))
    - `[?25h` (suppose "show cursor")

# Reference
- `man termios`
- `man tcgetattr`, `man tcsetattr`
- `man TIOCSWINSZ`
- [The GNU C Library Reference Manual: Low-Level Terminal Interface](https://www.gnu.org/software/libc/manual/html_node/Low_002dLevel-Terminal-Interface.html)
    - ["Raw Mode" in BSD](https://www.gnu.org/software/libc/manual/html_node/Noncanonical-Input.html#index-cfmakeraw)
    - [Noncanonical Mode Example](https://www.gnu.org/software/libc/manual/html_node/Noncanon-Example.html)
    - [Pseudo-Terminals](https://www.gnu.org/software/libc/manual/html_node/Pseudo_002dTerminals.html)
- [ECMA-48 5th edition](https://ecma-international.org/publications-and-standards/standards/ecma-48/)

<!---
vim: foldmethod=marker
--->
