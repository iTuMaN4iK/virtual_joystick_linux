#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <linux/uhid.h>
#include <cstdint>

enum class Joystick_Dpad_Button {
    centered = 0,
    up = 1,
    up_right = 2,
    right = 3,
    down_right = 4,
    down = 5,
    down_left = 6,
    left = 7,
    up_left = 8
};

static unsigned char rdesc[] = {
        0x05, 0x01,    /* USAGE_PAGE (Generic Desktop) */
        0x09, 0x04,    /* USAGE (joystick) */
        0xa1, 0x01,    /* COLLECTION (Application) */
        0x09, 0x01,        /* USAGE (Pointer) */
        0xa1, 0x00,        /* COLLECTION (Physical) */

        0x05, 0x09,            /* USAGE_PAGE (Button) */
        0x19, 0x01,            /* USAGE_MINIMUM (Button 1) */
        0x29, 0x08,            /* USAGE_MAXIMUM (Button 8) */
        0x15, 0x00,            /* LOGICAL_MINIMUM (0) */
        0x25, 0x01,            /* LOGICAL_MAXIMUM (1) */
        0x95, 0x08,            /* REPORT_COUNT (8) */
        0x75, 0x01,            /* REPORT_SIZE (1) */
        0x81, 0x02,            /* INPUT (Data,Var,Abs) */

        0x05, 0x01,            /* USAGE_PAGE (Generic Desktop) */
        0x09, 0x30,            /* USAGE (X) */
        0x09, 0x31,            /* USAGE (Y) */
        0x15, 0x81,            /* LOGICAL_MINIMUM (-127) */
        0x25, 0x7f,            /* LOGICAL_MAXIMUM (127) */
        0x95, 0x02,            /* REPORT_COUNT (2) */
        0x75, 0x08,            /* REPORT_SIZE (8) */
        0x81, 0x02,            /* INPUT (Data,Var,Rel) */

        0x05, 0x01,                            /*   USAGE_PAGE (Generic Desktop) */
        0x09, 0x39,                            /*   USAGE (Hat switch) */
        0x09, 0x39,                            /*   USAGE (Hat switch) */
        0x15, 0x01,                            /*   LOGICAL_MINIMUM (1) */
        0x25, 0x08,                            /*   LOGICAL_MAXIMUM (8) */
        0x95, 0x02,                            /*   REPORT_COUNT (2) */
        0x75, 0x04,                            /*   REPORT_SIZE (4) */
        0x81, 0x02,                            /*   INPUT (Data,Var,Abs) */

        0xc0,            /* END_COLLECTION */
        0xc0            /* END_COLLECTION */
};

typedef struct {
    uint8_t button1: 1;                   // Value = 0 to 1
    uint8_t button2: 1;                   // Value = 0 to 1
    uint8_t button3: 1;                   // Value = 0 to 1
    uint8_t button4: 1;                   // Value = 0 to 1
    uint8_t button5: 1;                   // Value = 0 to 1
    uint8_t button6: 1;                   // Value = 0 to 1
    uint8_t button7: 1;                   // Value = 0 to 1
    uint8_t button8: 1;                   // Value = 0 to 1
    int8_t X_GamePad;                          // Value = -127 to 127
    int8_t Y_GamePad;                          // Value = -127 to 127
    uint8_t dPad1: 4;                    // Value = 0 to 8
    uint8_t dPad2: 4;                    // Value = 0 to 8
} inputReport_t;

static int uhid_write(int fd, const struct uhid_event *ev) {
    ssize_t ret;

    ret = write(fd, ev, sizeof(*ev));
    if (ret < 0) {
        fprintf(stderr, "Cannot write to uhid: %m\n");
        return -errno;
    } else if (ret != sizeof(*ev)) {
        fprintf(stderr, "Wrong size written to uhid: %zd != %zu\n",
                ret, sizeof(ev));
        return -EFAULT;
    } else {
        return 0;
    }
}

static int create(int fd) {
    struct uhid_event ev{};

    memset(&ev, 0, sizeof(ev));
    ev.type = UHID_CREATE2;
    strcpy((char *) ev.u.create2.name, "test-uhid-device");
    memcpy(ev.u.create2.rd_data, rdesc, sizeof(rdesc));
    ev.u.create2.rd_size = sizeof(rdesc);
    ev.u.create2.bus = BUS_USB;
    ev.u.create2.vendor = 0x15d9;
    ev.u.create2.product = 0x0a37;

    ev.u.create2.version = 0;
    ev.u.create2.country = 0;

    return uhid_write(fd, &ev);
}

static void destroy(int fd) {
    struct uhid_event ev{};

    memset(&ev, 0, sizeof(ev));
    ev.type = UHID_DESTROY;

    uhid_write(fd, &ev);
}

static void handle_output(struct uhid_event *ev) {
    if (ev->u.output.rtype != UHID_OUTPUT_REPORT)
        return;
    if (ev->u.output.size != 2)
        return;
    if (ev->u.output.data[0] != 0x2)
        return;

    fprintf(stderr, "LED output report received with flags %x\n",
            ev->u.output.data[1]);
}

static int event(int fd) {
    struct uhid_event ev{};
    ssize_t ret;

    memset(&ev, 0, sizeof(ev));
    ret = read(fd, &ev, sizeof(ev));
    if (ret == 0) {
        fprintf(stderr, "Read HUP on uhid-cdev\n");
        return -EFAULT;
    } else if (ret < 0) {
        fprintf(stderr, "Cannot read uhid-cdev: %m\n");
        return -errno;
    } else if (ret != sizeof(ev)) {
        fprintf(stderr, "Invalid size read from uhid-dev: %zd != %zu\n",
                ret, sizeof(ev));
        return -EFAULT;
    }

    switch (ev.type) {
        case UHID_START:
            fprintf(stderr, "UHID_START from uhid-dev\n");
            break;
        case UHID_STOP:
            fprintf(stderr, "UHID_STOP from uhid-dev\n");
            break;
        case UHID_OPEN:
            fprintf(stderr, "UHID_OPEN from uhid-dev\n");
            break;
        case UHID_CLOSE:
            fprintf(stderr, "UHID_CLOSE from uhid-dev\n");
            break;
        case UHID_OUTPUT:
            fprintf(stderr, "UHID_OUTPUT from uhid-dev\n");
            handle_output(&ev);
            break;
        case UHID_OUTPUT_EV:
            fprintf(stderr, "UHID_OUTPUT_EV from uhid-dev\n");
            break;
        default:
            fprintf(stderr, "Invalid event from uhid-dev: %u\n", ev.type);
    }

    return 0;
}

static bool btn1_down = false;
static int switch_down = 0;
static signed char abs_hor = 0;
static signed char abs_ver = 0;

static int send_event(int fd) {
    struct uhid_event ev{};

    memset(&ev, 0, sizeof(ev));
    inputReport_t input;
    memset(&input, 0, sizeof(input));

    if (btn1_down)
        input.button1 |= 0x1;
    input.X_GamePad = abs_hor;
    input.Y_GamePad = abs_ver;
    switch (switch_down) {
        case 2:
            input.dPad1 = static_cast<uint8_t>(Joystick_Dpad_Button::down);
            break;
        case 4:
            input.dPad1 = static_cast<uint8_t>(Joystick_Dpad_Button::left);
            break;
        case 5:
            input.dPad1 = static_cast<uint8_t>(Joystick_Dpad_Button::centered);
            break;
        case 6:
            input.dPad1 = static_cast<uint8_t>(Joystick_Dpad_Button::right);
            break;
        case 8:
            input.dPad1 = static_cast<uint8_t>(Joystick_Dpad_Button::up);
            break;
        default:
            break;
    }
    ev.type = UHID_INPUT2;
    ev.u.input2.size = sizeof(input);
    memcpy(&ev.u.input2.data, &input, sizeof(input));
    return uhid_write(fd, &ev);
}

static long keyboard(int fd) {
    char buf[128];
    ssize_t ret, i;

    ret = read(STDIN_FILENO, buf, sizeof(buf));
    if (ret == 0) {
        fprintf(stderr, "Read HUP on stdin\n");
        return -EFAULT;
    } else if (ret < 0) {
        fprintf(stderr, "Cannot read stdin: %m\n");
        return -errno;
    }

    for (i = 0; i < ret; ++i) {
        switch (buf[i]) {
            case '1':
                btn1_down = !btn1_down;
                ret = send_event(fd);
                if (ret)
                    return ret;
                break;
            case '2':
                switch_down = 2;
                ret = send_event(fd);
                if (ret)
                    return ret;
                break;
            case '4':
                switch_down = 4;
                ret = send_event(fd);
                if (ret)
                    return ret;
                break;
            case '5':
                switch_down = 5;
                ret = send_event(fd);
                if (ret)
                    return ret;
                break;
            case '6':
                switch_down = 6;
                ret = send_event(fd);
                if (ret) {
                    return ret;
                }
                break;
            case '8':
                switch_down = 8;
                ret = send_event(fd);
                if (ret) {
                    return ret;
                }
                break;
            case 'a':
                if (abs_hor > -100)
                    abs_hor -= 20;
                ret = send_event(fd);
                if (ret)
                    return ret;
                break;
            case 'd':
                if (abs_hor < 100)
                    abs_hor += 20;
                ret = send_event(fd);
                if (ret)
                    return ret;
                break;
            case 'w':
                if (abs_ver < 100)
                    abs_ver += 20;
                ret = send_event(fd);
                if (ret)
                    return ret;
                break;
            case 's':
                if (abs_ver > -100)
                    abs_ver -= 20;
                ret = send_event(fd);
                if (ret)
                    return ret;
                break;
            case 'q':
                return -ECANCELED;
            default:
                fprintf(stderr, "Invalid input: %c\n", buf[i]);
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    int fd;
    const char *path = "/dev/uhid";
    struct pollfd pfds[2];
    struct termios state{};

    long ret = tcgetattr(STDIN_FILENO, &state);
    if (ret) {
        fprintf(stderr, "Cannot get tty state\n");
    } else {
        state.c_lflag &= ~ICANON;
        state.c_cc[VMIN] = 1;
        ret = tcsetattr(STDIN_FILENO, TCSANOW, &state);
        if (ret)
            fprintf(stderr, "Cannot set tty state\n");
    }

    if (argc >= 2) {
        if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
            fprintf(stderr, "Usage: %s [%s]\n", argv[0], path);
            return EXIT_SUCCESS;
        } else {
            path = argv[1];
        }
    }

    fprintf(stderr, "Open uhid-cdev %s\n", path);
    fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "Cannot open uhid-cdev %s: %m\n", path);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Create uhid device\n");
    ret = create(fd);
    if (ret) {
        close(fd);
        return EXIT_FAILURE;
    }

    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;
    pfds[1].fd = fd;
    pfds[1].events = POLLIN;

    fprintf(stderr, "Press 'q' to quit...\n");
    while (true) {
        ret = poll(pfds, 2, -1);
        if (ret < 0) {
            fprintf(stderr, "Cannot poll for fds: %m\n");
            break;
        }
        if (pfds[0].revents & POLLHUP) {
            fprintf(stderr, "Received HUP on stdin\n");
            break;
        }
        if (pfds[1].revents & POLLHUP) {
            fprintf(stderr, "Received HUP on uhid-cdev\n");
            break;
        }

        if (pfds[0].revents & POLLIN) {
            ret = keyboard(fd);
            if (ret)
                break;
        }
        if (pfds[1].revents & POLLIN) {
            ret = event(fd);
            if (ret)
                break;
        }
    }

    fprintf(stderr, "Destroy uhid device\n");
    destroy(fd);
    return EXIT_SUCCESS;
}