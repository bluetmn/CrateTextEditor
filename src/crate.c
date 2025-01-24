/*** INCLUDES ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** DEFINES ***/

#define CRATE_VERSION "0.0.1"

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
    char buf[32];
    unsigned int i = 0;

    // n command to query status info from terminal - 6 for cursor pos
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) {
            break;
        }
        if (buf[i] == 'R') {
            break;
        }
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') {
        return -1;
    }
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) {
        return -1;
    }

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    // IOCTL is not guaranteed on all systems - provide fallback
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 1 || ws.ws_col == 0) {
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

/*** append buffer ***/

struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0};

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) {
        return;
    }
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** OUTPUT ***/

void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows ; y++) {
        if (y == E.screenrows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome), 
            "Crate editor == version %s", CRATE_VERSION);

            // ensure message does not wrap - ugly
            if (welcomelen > E.screencols) {
                welcomelen = E.screencols;
            }
            int padding = (E.screencols - welcomelen) / 2;
            if (padding) {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--) {
                abAppend(ab, " ", 1);
            }
            abAppend(ab, welcome, welcomelen);
        }
        else {
            abAppend(ab, "~", 1);
        }
        // <esc>[K to clear right of cursor
        abAppend(ab, "\x1b[K", 3);

        if (y != E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    // <esc>[l to hide cursor
    abAppend(&ab, "\x1b[?25l", 6);
    // <esc>[2j escape sequence to clear full screen
    // abAppend(&ab, "\x1b[2J", 4);
    // [H to move cursor to top left corner
    abAppend(&ab, "\x1b[H", 3);

    // row setup
    editorDrawRows(&ab);

    abAppend(&ab, "\x1b[H",3);
    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
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