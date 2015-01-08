#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/serio.h>
//#include "serio-ids.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static int readchar(int fd, unsigned char *c, int timeout)
{
    struct timeval tv;
    fd_set set;

    tv.tv_sec = 0;
    tv.tv_usec = timeout * 1000;

    FD_ZERO(&set);
    FD_SET(fd, &set);

    if (!select(fd + 1, &set, NULL, NULL, &tv))
        return -1;

    if (read(fd, c, 1) != 1)
        return -1;

    return 0;
}

static void setline(int fd, int flags, int speed)
{
    struct termios t;

    tcgetattr(fd, &t);

    t.c_cflag = flags | CREAD | HUPCL | CLOCAL;
    t.c_iflag = IGNBRK | IGNPAR;
    t.c_oflag = 0;
    t.c_lflag = 0;
    t.c_cc[VMIN ] = 1;
    t.c_cc[VTIME] = 0;

    cfsetispeed(&t, speed);
    cfsetospeed(&t, speed);

    tcsetattr(fd, TCSANOW, &t);
}


struct input_types {
    const char *name;
    const char *name2;
    const char *desc;
    int speed;
    int flags;
    unsigned long type;
    unsigned long id;
    unsigned long extra;
    int flush;
    int (*init)(int fd, unsigned long *id, unsigned long *extra);
};

static struct input_types input_types[] = {
{ "--i2c",     "-i",    "I2C",
        B115200, CS8,
    100,      0,  0,  0,  NULL },
{ NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, NULL }
};

static void show_help(void)
{
//    struct input_types *type;

    puts("");
//    puts("Usage: nanpyattach [--daemon] [--baud <baud>] [--always] [--noinit] <mode> <device>");
    puts("Usage: nanpyattach [--daemon] [--baud <baud>] [--always] [--noinit] <device>");
//    puts("");
//    puts("Modes:");
//
//    for (type = input_types; type->name; type++)
//        printf("  %-16s %-8s  %s\n",
//            type->name, type->name2, type->desc);

    puts("");
}

/* palmed wisdom from http://stackoverflow.com/questions/1674162/ */
#define RETRY_ERROR(x) (x == EAGAIN || x == EWOULDBLOCK || x == EINTR)

int main(int argc, char **argv)
{
    unsigned long devt;
    int ldisc;
    struct input_types *type = NULL;
    const char *device = NULL;
    int daemon_mode = 0;
//    int need_device = 0;
    unsigned long id, extra;
    int fd;
    int i;
    unsigned char c;
    int retval;
    int baud = -1;
    int ignore_init_res = 0;
    int no_init = 0;

    for (i = 1; i < argc; i++) {
        if (!strcasecmp(argv[i], "--help")) {
            show_help();
            return EXIT_SUCCESS;
        } else if (!strcasecmp(argv[i], "--daemon")) {
            daemon_mode = 1;
        } else if (!strcasecmp(argv[i], "--always")) {
            ignore_init_res = 1;
        } else if (!strcasecmp(argv[i], "--noinit")) {
            no_init = 1;
        } else if (!strcasecmp(argv[i], "--baud")) {
            if (argc <= i + 1) {
                show_help();
                fprintf(stderr,
                    "nanpyattach: require baud rate\n");
                return EXIT_FAILURE;
            }

            baud = atoi(argv[++i]);
//        } else if (need_device) {
        } else {
            device = argv[i];
//            need_device = 0;
        }
//        else {
//            if (type && type->name) {
//                fprintf(stderr,
//                    "nanpyattach: '%s' - "
//                    "only one mode allowed\n", argv[i]);
//                return EXIT_FAILURE;
//            }
//            for (type = input_types; type->name; type++) {
//                if (!strcasecmp(argv[i], type->name) ||
//                    !strcasecmp(argv[i], type->name2)) {
//                    break;
//                }
//            }
//            if (!type->name) {
//                fprintf(stderr,
//                    "nanpyattach: invalid mode '%s'\n",
//                    argv[i]);
//                return EXIT_FAILURE;
//            }
//            need_device = 1;
//        }
    }

//    if (!type || !type->name) {
//        fprintf(stderr, "nanpyattach: must specify mode\n");
//        return EXIT_FAILURE;
//        }

//    if (need_device) {
    if (!device) {
        fprintf(stderr, "nanpyattach: must specify device\n");
        return EXIT_FAILURE;
    }

    type = input_types;

    fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "nanpyattach: '%s' - %s\n",
            device, strerror(errno));
        return 1;
    }

    switch(baud) {
    case -1: break;
    case 2400: type->speed = B2400; break;
    case 4800: type->speed = B4800; break;
    case 9600: type->speed = B9600; break;
    case 19200: type->speed = B19200; break;
    case 38400: type->speed = B38400; break;
    case 57600: type->speed = B57600; break;
    case 115200: type->speed = B115200; break;
    default:
        fprintf(stderr, "nanpyattach: invalid baud rate '%d'\n",
                baud);
        return EXIT_FAILURE;
    }

    setline(fd, type->flags, type->speed);

    if (type->flush)
        while (!readchar(fd, &c, 100))
            /* empty */;

    id = type->id;
    extra = type->extra;

    if (type->init && !no_init) {
        if (type->init(fd, &id, &extra)) {
            if (ignore_init_res) {
                fprintf(stderr, "nanpyattach: ignored device initialization failure\n");
            } else {
                fprintf(stderr, "nanpyattach: device initialization failed\n");
                return EXIT_FAILURE;
            }
        }
    }

    ldisc = N_MOUSE;
    if (ioctl(fd, TIOCSETD, &ldisc) < 0) {
        fprintf(stderr, "nanpyattach: can't set line discipline\n");
        return EXIT_FAILURE;
    }

    devt = type->type | (id << 8) | (extra << 16);

    if (ioctl(fd, SPIOCSTYPE, &devt) < 0) {
        fprintf(stderr, "nanpyattach: can't set device type\n");
        return EXIT_FAILURE;
    }

    retval = EXIT_SUCCESS;
    if (daemon_mode && daemon(0, 0) < 0) {
        perror("nanpyattach");
        retval = EXIT_FAILURE;
    }

    errno = 0;
    do {
        i = read(fd, NULL, 0);
    } while (RETRY_ERROR(errno));

    ldisc = 0;
    if (errno == 0) {
        // If we've never managed to read, avoid resetting the line
        // discipline - another nanpyattach is probably running
        ioctl(fd, TIOCSETD, &ldisc);
    }
    close(fd);

    return retval;
}
