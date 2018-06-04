/*** includes ***/

/*iscntrl*/
#include <ctype.h>
/*errno
  EAGAIN*/
#include <errno.h>
/*printf
  perror*/
#include <stdio.h>
/*atexit
  exit*/
#include <stdlib.h>
/*ioctl*/
#include <sys/ioctl.h>
/*read
  write
  STDIN_FILENO
  STDOUT_FILENO*/
#include <unistd.h>
/*ICANON
  ISIG
  IXON
  IEXTEN
  ICRNL
  OPOST
  BRKINT
  INPCK
  ISTRIP
  CS8
  VMIN
  VTIME*/
#include <termios.h>

/*** defines ***/

/* Sets the first 3 bits to zero, mirrors what the Ctrl
   key does in the terminal(strips bits 5 and 6)*/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

/*Global editor data*/
struct editorConfig {
  int screenrows;
  int screencols;
  struct termios originalTermios;
};

struct editorConfig config;

/*** terminal **/

/*Prints an error message and exits the program*/
void Die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  /*C lib functinos that fail set the global errno var to indicate 
    what the error was
    Perror() looks at the global errno var and prints a descriptive
    error message for it*/
  perror(s);
  /*We exit the program with error status 1 indicating failure*/
  exit(1);
}

/*Disables raw mode*/
void DisableRawMode() {
  /*Resets terminal attributes to starting state*/
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &config.originalTermios) == -1)
    Die("tcsetattr");
}

/*Enables raw mode*/
void EnableRawMode() {
  /*Read current terminal attributes into a struct*/
  if (tcgetattr(STDIN_FILENO, &config.originalTermios) == -1) Die ("tcgetattr");
  atexit(DisableRawMode);
  
  struct termios raw = config.originalTermios;
  /*Modify the struct
    ~ - bitwise-NOT operator
    & - bitwise-AND operator
    ECHO is a bitflag
    Switches fourth bit in the flags field to 0

    ICANON terminal's canonical mode flag
    ISIG Ctrl-C and Ctrl-Z, SIGINT, SIGSTP
    IXON Ctrl-S and Ctrl-Q, software flow control
    IEXTEN Ctrl-V literal char output
    ICRNL Ctrl-M, diabling prevents carriage returns
    from being treated as newlines
    OPOST output processing features,disabling prevents terminal
    from turning newlines into CR followed by newline

    Maybe unnecessary:
    BRKINT a break condition causes SIGINT to be sent to the program
    INPCK enables parity checking, does not apply to modern emulators
    ISTRIP casuses 8th bit of every byte to be stripped
    CS8 sets char size to 8 bits per byte */
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK| ISTRIP| IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  /*VMIN - sets the minimum number of bytes of input needed before
    read() can return
    VTIME - sets the maximum amount of time to wait before read()
    returns, measured in tenths of a second
    !Does not seem to be doing anything
  */
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  /*TCSAFLUSH specifies when to apply the change
    In this case waits for all output to be written to the terminal*/
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) Die("tcsetattr");
}

/*Waits for one keypress and then returns it*/
char EditorReadKey() {
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) Die("read");
  }

  return c;
}

int GetCursorPosition(int *rows, int *cols) {
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  printf("\r\n");
  char c;

  while (read(STDIN_FILENO, %c, 1) == 1) {
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    }
    else {
      printf("%d ('%c')\r\n", c, c);
    }
  }

  EditorReadKey();

  return -1;

}

/*Finds the size of terminal and stores height and width into 
  passed params*/
int GetWindowSize(int *rows, int *cols) {
  struct winsize ws;
  
  /*Returns window size of terminal*/
  if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    /*Moves the cursor to the end of the screen by using C and B cmds
      C - cursor forward
      B - cursor down
      B and C stop the cursor from going past the edge of the screen*/
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return GetCursorPosition(rows, cols);
  }
  else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** output ***/

void EditorDrawRows() {
  int y;

  for (y = 0; y < config.screenrows; y++) {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}

void EditorRefreshScreen() {
  /*4 - we're writing 4 bytes out to the terminal
    \x1b - escape char (27 in decimal)
    \x1b[ - beginning of any escape sequence
    J - erase display command
    2 - escape seuence command argument, clears the entire screen
    
    H - cursor position cmd*/   
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  EditorDrawRows();
  write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** input ***/

/*Waits for a keypress and then handles it*/
void EditorProcessKeypress () {
  char c = EditorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }
}

/*** init ***/

// Initializes the fields in config struct
void InitEditor() {
  if (GetWindowSize(&config.screenrows, &config.screencols) == -1) 
    Die("GetWindow size");
}

int main() {
  EnableRawMode();
  InitEditor();

  while (1) {
    EditorRefreshScreen();
    EditorProcessKeypress();
  }

  return 0;
}
