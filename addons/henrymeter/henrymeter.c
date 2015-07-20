#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>

#define SERIAL_LINEBUF_SIZE 512

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
  tio.c_cc[VTIME] = 0; /* Blocking read.                               */

  if (tcsetattr(fd, TCSANOW, &tio)) {
    /* Failed to apply settings. */
    return -1;
  } else {
    tcflush(fd, TCIFLUSH);
    return fd;
  }
}

/* Blocking read, returns 0 on error. */
static unsigned char serial_getc(int fd) {
  unsigned char c = 0;

  if(fd > 0) {
    if (read(fd, &c, 1)) {
      return c;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

static void serial_wait_line(int fd) {
  unsigned char c = 0;

  do {
    c = serial_getc(fd);
  } while (c != 0 && c != '\n' && c != '\r');
}

static int serial_read_line(int fd, char *buf, int size) {
  unsigned char c = 0;

  if (!buf || !size) {
    return -EINVAL;
  } else {
    /* Wait for a new line. */
    serial_wait_line(fd);

    /* First char needs care (CR+LF problem): skip all \n\r mess. */
    c = serial_getc(fd);
    while ((c == '\n') || (c == '\r')) {
      c = serial_getc(fd);
    }
    *buf = c;
    size --;

    do {
      buf ++;
      *buf = serial_getc(fd);
      size --;
    } while ((*buf != 0) && (*buf != '\n') && (*buf != '\r') && (size > 0));
    *buf = '\0';

    return 0;
  }
}

static void print_help(const char *self) {
  fprintf(stderr, "\
Usage: %s [-d serial] [-c cap] [-p]\n\
\n\
\t-c\t Set reference capacitance in the shorthand picofarad notation.\n\
\t  \t e.g. 104 = 100nF \t Default: 223 (22nF)\n\
\t-d\t Set the USB CDC device\n\
\t  \t e.g. /dev/ttyACM0 \t Default: /dev/ttyACM0\n\
\t-h\t Print this help.\n\
\t-o\t Set inductance offset.\n\
\t  \t e.g. 105 = 1mH \t Default: 0 (no offset)\n\
\t-p\t Set this parameter when Pierce/Colpitts oscillator is used.\n\
\t  \t Capacitance will be halved.\n\
\n\
Example: %s -d /dev/ttyACM1 -c 224 -p -o 104\n\
(220nF Cref, 100uH offset, Pierce/Colpitts oscillator, on ttyACM1)\n\
\n", self, self);
}

static void handle_bad_opts(void) {
  if ((optopt == 'c') || (optopt == 'd')) {
    fprintf(stderr, "ERROR: option -%c requires an argument.\n\n", optopt);
  } else if (isprint(optopt)) {
    fprintf(stderr, "ERROR: unknown option `-%c'.\n\n", optopt);
  } else {
    fprintf(stderr, "ERROR: unknown option character `\\x%x'.\n\n", optopt);
  }
}

int main(int argc, char *argv[]) {
  bool   pierce      = false;
  double capacitance = 22e-9; /* F, default to 22nF. */
  double offset      = 0.00f; /* H, detault to no offset. */
  char   *device     = "/dev/ttyACM0";

  int c;
  opterr = 0;
  while ((c = getopt(argc, argv, "c:d:ho:p")) != -1) {
    switch (c) {
      case 'c': {
        /* Set capacitance. */
        /* Convert 104 to 10 * 10 ^ 4 pF. */
        unsigned param = 0;
        int cnt, exp;

        cnt = sscanf(optarg, "%u", &param);
        if ((cnt != 1) || (param < 10)) {
          print_help(argv[0]);
          return -EINVAL;
        }

        exp = param % 10; /* Last digit is the exponent. */
        param /= 10;
        capacitance = param * pow(10.0f, exp);
        capacitance /= 1e12; /* pF -> F. */

        break;
      }

      case 'd': {
        /* Set device. */
        /* optarg is one of the elements in argv[] and will not be freed afterwards. */
        device = optarg;
        break;
      }

      case 'h': {
        /* Print help. */
        print_help(argv[0]);
        return 0;
        break;
      }

      case 'o': {
        /* Set offset. */
        /* Convert 104 to 10 * 10 ^ 4 nF. */
        unsigned param = 0;
        int cnt, exp;

        cnt = sscanf(optarg, "%u", &param);
        if ((cnt != 1) || (param < 10)) {
          print_help(argv[0]);
          return -EINVAL;
        }

        exp = param % 10; /* Last digit is the exponent. */
        param /= 10;
        offset = param * pow(10.0f, exp);
        offset /= 1e9; /* nH -> H. */

        break;
      }

      case 'p': {
        /* Set Pierce/Colpitts mode. */
        /* Since here capacitors are in series, half the value. */
        pierce = true;
        break;
      }

      case '?': {
        handle_bad_opts();
        print_help(argv[0]);
        return -EINVAL;
      }

      default: {
        fprintf(stderr, "BUG: switch fall-through on `%c'!\n", c);
        abort();
      }
    }
  }

  fprintf(stdout, "Device: %s\nCapacitance: %.3lf nF\nPierce/Colpitts: %s\nOffset: %.3lf uH\n\n",
           device,
           capacitance * 1e9,
           pierce ? "yes" : "no",
           offset * 1e6
         );
  if (pierce) {
    capacitance /= 2;
  }

  /* Setup serial. */
  int  serial_fd = -1;
  serial_fd = serial_open(device);
  if (serial_fd < 0) {
    perror("ERROR: cannot open serial port");
    return errno;
  }

  /* Main loop. */
  char buf[SERIAL_LINEBUF_SIZE];
  double  freq, ind;
  char    dot;
  while (true) {
    if (0 != serial_read_line(serial_fd, buf, SERIAL_LINEBUF_SIZE)) {
      fprintf(stderr, "ERROR: cannot read from serial port!\n");
      return -1;
    }

    if (2 != sscanf(buf, "%lf MHz %c", &freq, &dot)) {
      fprintf(stderr, "ERROR: cannot parse frequency from line `%s'!\n", buf);
      return -1;
    }

    if (freq < 0.0f) {
      fprintf(stderr, "ERROR: invalid frequency `%lf'!\n", freq);
      return -1;
    }

    if ('.' != dot) {
      dot = ' ';
    }

    freq *= 1e6; /* MHz -> Hz. */
    if (0.0f == freq) {
      fprintf(stdout, "%15.3lf uH %c (%9.0lf Hz)\r", 0.0f, dot, freq);
    } else {
      ind = (1.0f / (4 * M_PI * M_PI)) / (freq * freq) / capacitance;

      if (ind > 10) {
        /* L > 10H must be open circuit.*/
        ind = INFINITY;
      }

      ind -= offset;
      fprintf(stdout, "%15.3lf uH %c (%9.0lf Hz)\r", ind * 1e6, dot, freq); /* H -> uH. */
    }

    fflush(stdout); /* Required if '\n' not present. */
  }

  return 0;
}
