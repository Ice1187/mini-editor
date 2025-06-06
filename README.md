# Mini Editor

A mini text editor written from scratch.

## TODO
- [x] compile and play with `kilo` program
- [x] trace the code of `kilo`
    - [x] trace `editorRefreshScreen`
    - [x] trace `editorProcessKeypress`
- [x] understand how terminal works and how to control it
    - [x] read [Low-Level Terminal Interface](https://www.gnu.org/software/libc/manual/html_node/Low_002dLevel-Terminal-Interface.html)
- [x] understand what is RAW mode and how it works
- [x] read ECMA-48 to understand ANSI control codes
- [x] render a empty screen
- [ ] open, read and render file
- [ ] store content changes of editing file
- [ ] write changes to the file

## Minimal PoC Requirement
- put the terminal into *noncanonical input mode* for controlling all the inputs by our programs, instead of let OS handle the editing
    - maybe with [`cfmakeraw`](https://www.gnu.org/software/libc/manual/html_node/Noncanonical-Input.html#index-cfmakeraw)
    - see [the example in the doc](https://www.gnu.org/software/libc/manual/html_node/Noncanon-Example.html)
- read the editing file line by line into the internal buffer
- event loop
    - draw the internal buffer line by line to the screen
    - wait for user input
    - process user input
    - draw the screen (repeat)
- on save: save the internal buffer to the editing file
- on quit: disable RAW mode

### Optional
- no resisze window (capture signal `SIGWINCH` to handle window size change)
- no status bar
- don't need to handle TAB
- no syntax highlight
- no line number
- no search

### Possible Cool Weird Features
- EMCA-48 support vertical right-to-left line progression (section 6.1.1). So maybe a vertical right-to-left oriented editor will be cool. Combined with [wenyan-lang](https://github.com/wenyan-lang/wenyan?tab=readme-ov-file) would be a nice fit to write 文言古詩-style script.

## Terminal Control Mode
### Local Mode
> These flags generally control higher-level aspects of input processing than the input modes flags described in Input Modes, such as echoing, signals, and the choice of canonical or noncanonical input.
- `ICANON`: put terminal in canonical mode
- `ECHO`: echo the received input
- `ISIG`: recognize SIGINT, SIGQUIT, SIGQUIT (ctrl-c/z/\\)
- `IEXTEN`: enable DISCARD and LNEXT
- `LNEXT`: the special meaning of the character the user typed next to LNEXT is disable and read as a plain character. LNEXT is usually ctrl-v. For example, backspace is usually mapped to ERASE control code, represented as `^?`. When you type backspace, the character on the cursor is erased because backspace send the ERASE control code. But when you type ctrl-v (LNEXT) then backspace, it show `^?` on the terminal instead. Beacuse the special meaning of backspace is disabled by LNEXT and read as a plain character.
- `DISCARD`: when the DISCARD is typed, all program output is discarded.
- `ECHONL`: only work in canonical mode, so we don't need it, I think.

### Control Mode
> Seems to only affecting physical serial ports.
- `CS8`: specify eight bits per byte. Usually only useful on physical serial ports.
- `CREAD`: tell the hardware to read input from the terminal, or discard it. Usually only useful on physical serial ports.

### Input Mode
- `ICRNL`: when receive carriage return `\r` from input, send newline `\n` to output instead.
- `INLCR`: when receive newline `\n` from input, send carriage return `\r` to output instead.
- `INPCK`: enable input parity checking
- `ISTRIP`: strip valid input bytes to seven bits, otherwise it will be 8 bits. For example, unset `ISTRIP`, then we can type `` with opt-k on macOS. Set `ISTRIP`, then it become `K`.
- `IXON`: if receives a STOP character, suspend output until a START character is received. In this case, the STOP and START characters are never passed to the application program. If this bit is not set, then START and STOP can be read as ordinary characters. For example, when `ixon` is set, `ctrl-v ctrl-q enter` show `^M` because enter is `^M`. But when unset it with `stty -ixon`, `ctrl-v ctrl-q enter` show `^Q` and enter because START (`^Q`) is read as ordinary character and not intercept by the kernel(?
    - START character can be set by `termios.c_cc[VSTART]`, which is usually ctrl-q; same for STOP, which is usually ctrl-s.

### Output Mode
- `OPOST`: output data is processed in some unspecified way so that it is displayed appropriately on the terminal device. If not set, the characters are sent as-is. For example, `stty -a` shows the terminal options in a well-formatted way, but if you set `stty -opost` and `stty -a` again, the `\n` doesn't automatically return to the beginning of the line, and, thus, mess up the display format.
- `ONLCR`: convert the newline character `\n` on output into carriage return `\r` followed by linefeed `\n`. For example, `stty -onlcr` also cause `\n` to not automatically return to the beginning of the line and mess up the display format.
- `OXTABS`: convert the tab characters `\t` on output into the appropriate number of spaces to emulate a tab stop every eight columns. For example, type `echo "a\tb"` and select the tab with mouse get a big chunk of one single space. But `stty oxtabs` then `echo "a\tb"`, the space become multiple single-char spaces.

## ANSI Control Code Table
- [my notes on ECMA-48](https://app.heptabase.com/fe2dcb7b-fdfc-4173-a112-aaa81e688360/card/6c9de788-1b27-4a16-9173-51af63b9da51)
- the control modes of kilo is set to 8 bit (by `raw.c_cflag |= (CS8)`)
- `man console_codes` on Linux is useful

code  |  hex  |  meaning
----- | ----- | --------
ESC   | 0x1b  | Escape
[     | 0x5b  | CSI in 7-bit
<!-- {{{
n     | 0x6e  |
B     | 0x42  |
C     | 0x43  |
?     | 0x3f  |
l     | 0x6c  |
H     | 0x48  |
h     | 0x68  |
m     | 0x6d  |
K     | 0x4b  |
}}} -->

code  | control fn | meaning
----- | ---------- | ---------
`[6n`   | DSR        | report the cursor position
`[#B`   | CUD        | move cursor down # position
`[#C`   | CUF        | move cursor right # position
`[0K`   | EL         | erase the char under the cursor up to the end of the line
`[#;#H` | CUP        | move cursor to (#, #) position; `[H` default to (1, 1)
`[#m`   | SGR        | set graphic rendition for subsequent text, e.g., fg/bg color, bold, italic, etc.
`[?25l` | RM         | hide the cursor. Not in ECMA-48, see [wiki](https://en.wikipedia.org/wiki/ANSI_escape_code#SCP)
`[?25h` | SM         | show the cursor. Not in ECMA-48, see [wiki](https://en.wikipedia.org/wiki/ANSI_escape_code#SCP)


## Reference
- [Kilo](https://github.com/antirez/kilo/tree/master)
- [The GNU C Library Reference Manual: Low-Level Terminal Interface](https://www.gnu.org/software/libc/manual/html_node/Low_002dLevel-Terminal-Interface.html)
    - ["Raw Mode" in BSD](https://www.gnu.org/software/libc/manual/html_node/Noncanonical-Input.html#index-cfmakeraw)
    - [Noncanonical Mode Example](https://www.gnu.org/software/libc/manual/html_node/Noncanon-Example.html)
    - [Pseudo-Terminals](https://www.gnu.org/software/libc/manual/html_node/Pseudo_002dTerminals.html)
- [ECMA-48 5th edition](https://ecma-international.org/publications-and-standards/standards/ecma-48/)
- `man termios`
- `man tcgetattr`, `man tcsetattr`
- `man stty`
- `man console_codes`
- `man TIOCSWINSZ`
- [Plain Text - Dylan Beattie - NDC Copenhagen 2022](https://www.youtube.com/watch?v=gd5uJ7Nlvvo)
    - Great talk for understand "plain text" in the computer. It answers a lot of question for who never saw a teletypewriter, like why we need `\r` and `\n`, what does `ctrl-c`, the code design of ASCII, etc. A very interesting talk for anyone whose interested in how "plain text" evolved in the computer era.

<!--- vim: foldmethod=marker
