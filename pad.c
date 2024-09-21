/*** includes ***/

#include <asm-generic/ioctls.h>
#include <errno.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** data ***/
struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

struct editorConfig E;

/*** defines ***/

#define PAD_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)


/*** terminal ***/

void die(const char *s) {
  write(STDIN_FILENO, "\x1b[2J", 4);
  write(STDIN_FILENO, "\x1b[H", 3);

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
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("tcsetattr");
  }
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

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if(ioctl(STDIN_FILENO,TIOCGWINSZ, &ws) == 1 || ws.ws_col == 0) {
    return -1;
  }
  *cols = ws.ws_col;
  *rows = ws.ws_row; 
  return 0;
}
/*** append buffer ***/

struct abuf {
  char *buf;
  int len;
};

#define  ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->buf, ab->len + len);

  if(new == NULL) {
    return;
  }
  memcpy(&new[ab->len], s, len);
  ab->buf = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->buf);
}

/*** output ***/

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows ; y++) {
    if (y == E.screenrows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome),
        "Pad editor -- version %s", PAD_VERSION);
      if (welcomelen > E.screencols) welcomelen = E.screencols;
      int padding = (E.screencols - welcomelen) / 2;
      if (padding) {
        abAppend(ab, "~", 1);
        padding--;
      }
      while (padding--) abAppend(ab, " ", 1);
      abAppend(ab, welcome, welcomelen);
    } else {
      abAppend(ab, "~", 1);
    }
    abAppend(ab, "\x1b[K", 3);  // clear line

    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);// hide cursor 
  abAppend(&ab, "\x1b[H", 3);   // move cursor to the top

  editorDrawRows(&ab); // draw `~` chars

  abAppend(&ab, "\x1b[H", 3);   // move cursor to the top
  abAppend(&ab, "\x1b[?25h", 6);// show  cursor 

  write(STDIN_FILENO, ab.buf, ab.len);
  abFree(&ab);

}

/*** input ***/

void editorProcessKeypress() {
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'): {
      write(STDIN_FILENO, "\x1b[2J", 4);
      write(STDIN_FILENO, "\x1b[H", 3);
      exit(0);
      break;
    }
  }
}

/*** init ***/
void initEditor() {
  if(getWindowSize(&E.screenrows, &E.screencols) == -1) {
    die("getWindowSize");
  }
}

int main() {
  enableRawMode();
  initEditor();
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
