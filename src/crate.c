/*** INCLUDES ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** DEFINES ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** DATA ***/

struct editorConfig {
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

/*** TERMINAL ***/

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        die("tcgetattr");
    }
    struct termios raw = E.orig_termios;

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

int getCursorPosition(int *rows, int *cols) {
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }

    printf("\r\n");
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        }
        else {
            printf("%d ('%c')\r\n", c, c);
        }
    }

    editorReadKey();

    return -1;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    // IOCTL is not guaranteed on all systems - provide fallback
    if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }
        else {
            return getCursorPosition(rows, cols);
        }
        return -1;
    }
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** OUTPUT ***/

void editorDrawRows() {
    int y;
    for (y = 0; y < E.screenrows ; y++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void editorRefreshScreen() {
    // \x1b[2j escape sequence to clear full screen
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // [H to move cursor to top left corner
    write(STDOUT_FILENO, "\x1b[H", 3);

    // row setup
    editorDrawRows();
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

void initEditor() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) {
        die("getWindowsSize");
    }
}

int main() {
    enableRawMode();
    initEditor();

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