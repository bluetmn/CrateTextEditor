/*** INCLUDES ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** DEFINES ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** DATA ***/

struct termios orig_termios;

/*** TERMINAL ***/

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        die("tcsetattr");
    }
}

void enableRawMode() {
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        die("tcgetattr");
    }
    raw = orig_termios;
    // FLIP OFF
    // ECHO - text is not displayed as it is typed
    // ICANON - process input as chars come in
    // ISIG - disable ctrl-C and ctrl-Z
    // IXON - disable ctrl-Q and ctrl-S 
    // IEXTEN - disable ctrl-V waiting for additional chars
    // ICRNL - turn off translation of carriage returns into newlines
    //OPOST
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK| ISTRIP| IXON);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_oflag &= ~(OPOST);

    //c_cc CONTROL CHARS
    // VMIN - set min number of bytes needed before read can return 
    // VTIME - set max amount of time read will wait
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    // SET TERMINAL INPUT SETTINGS
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }

    atexit(disableRawMode);
}

char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            die("read");
        }
    }
    return c;
}

/*** OUTPUT ***/

void editorRefreshScreen() {
    // \x1b[2j escape sequence to clear full screen
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // [H to move cursor to top left corner
    write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** INPUT ***/

void editorProcessKeypress() {
    char c = editorReadKey();

    if (c == CTRL_KEY('q')) {
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
    }
}

/*** INIT ***/
int main() {
    enableRawMode();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
        // char c = '\0';
        // if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) {
        //     die("read");
        // }
        // // \n IMITATES HITTING ENTER, OTHERWISE IT BUFFERS INPUT BEFORE SENDING TO PROGRAM
        // if (iscntrl(c)) {
        //     printf("%d\r\n", c);
        // }
        // else {
        //     printf("%d (%c)\r\n", c, c);
        // }
        // if (c == CTRL_KEY('q')) {
        //     break;
        // }
    };

    return 0;
}