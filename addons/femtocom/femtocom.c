#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>

#define SERIAL_LINEBUF_SIZE 512

static struct termios ttysave;
static bool ttysave_valid = false;

static void serial_close(int fd) {
  int result = -1;

  if (fd > 0) {
    do {
      result = close(fd);
    } while (result == -1 && errno == EINTR);
  }
}

static int serial_open(const char *device_path) {
  int fd = -1;

  /* NULL or empty string. */
  if (!device_path || !*device_path) {
    return -EINVAL;
  }

  do {
    fd = open(device_path, O_RDWR | O_NOCTTY);
  } while (fd == -1 && errno == EINTR);

  if (fd < 0) {
    /* Failed to open file. */
    return fd;
  }

  if (ioctl(fd, TIOCEXCL)) {
    /* Failed to set IO exclusive, close it. */
    serial_close(fd);
    return -EIO;
  }

  /* Basic serial IO setup. */
  /* Most common config. USB CDC without real serial does not care anyway. */
  struct termios tio = {
    .c_cflag = B115200 | CS8 | CLOCAL | CREAD,
    .c_iflag = 0,
    .c_oflag = 0,
    .c_lflag = NOFLSH,
    .c_cc = {0},
  };
  tio.c_cc[VMIN]  = 1; /* Wait until 1 char available (blocking read). */
  tio.c_cc[VTIME] = 1; /* Give up after 100ms.                         */

  if (tcsetattr(fd, TCSANOW, &tio)) {
    /* Failed to apply settings. */
    return -1;
  } else {
    tcflush(fd, TCIFLUSH);
    return fd;
  }
}

static int term_setup(void) {
  /* Setup local terminal. */
  struct termios ttystate;

  if (tcgetattr(STDIN_FILENO, &ttystate)) {
    return -1;
  }

  ttysave = ttystate;
  ttysave_valid = true;

  ttystate.c_lflag &= ~(ICANON | ECHO);
  ttystate.c_cc[VMIN] = 0; /* Non-blocking. */

  if (tcsetattr(STDIN_FILENO, TCSANOW, &ttystate)) {
    return -1;
  }

  return 0;
}

static void sig_handler(int signo) {
  /* Restore handlers first, so user can force quit. */
  signal(signo, SIG_DFL);
  /* Restore terminal settings. */
  if (ttysave_valid) {
    tcsetattr(STDIN_FILENO, TCSANOW, &ttysave);
  }

  fputc('\n', stdout);
  exit(1);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(
      stderr,
      "Usage: %s [path to tty]\nExample: %s /dev/ttyACM0\n",
      argv[0],
      argv[0]
    );
    return -EINVAL;
  }

  if (signal(SIGINT, sig_handler) == SIG_ERR) {
    perror("Error setting signal handler");
    return errno;
  }
  if (signal(SIGTERM, sig_handler) == SIG_ERR) {
    perror("Error setting signal handler");
    return errno;
  }

  if (term_setup()) {
    perror("Error setting terminal up");
    return errno;
  }

  int  serial_fd = -1;
  serial_fd = serial_open(argv[1]);
  if (serial_fd < 0) {
    perror("Error opening serial port");
    return errno;
  }

  char buf[SERIAL_LINEBUF_SIZE];
  ssize_t len;
  int c, in;
  while (true) {
    /* Handle user input first.                      */
    /* Drain excessive inputs and keep the last one. */
    in = EOF;
    while (EOF != (c = getchar())) {
      in = c;
    }
    if (EOF != in) {
      write(serial_fd, &in, 1);
    }

    /* Reserve space for '\0'. */
    len = read(serial_fd, buf, SERIAL_LINEBUF_SIZE - 1);
    if (len < 0) {
      perror("Error reading serial port");
      break;
    }
    if (len == 0) {
      continue;
    }

    buf[len] = '\0';
    fputs(buf, stdout);
    fflush(stdout);
  }

  /* Cleaning up. */
  serial_close(serial_fd);
  sig_handler(0);

  return 0;
}
