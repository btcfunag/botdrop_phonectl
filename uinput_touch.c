#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/uinput.h>
#include <linux/input.h>

#define SCREEN_W 1080
#define SCREEN_H 2340

static int fd = -1;

static void emit(int type, int code, int val) {
    struct input_event ev = {0};
    ev.type = type;
    ev.code = code;
    ev.value = val;
    write(fd, &ev, sizeof(ev));
}

static void syn(void) {
    emit(EV_SYN, SYN_REPORT, 0);
}

static int create_device(int need_keys) {
    struct uinput_setup setup;

    fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) { perror("open uinput"); return -1; }

    /* Enable event types */
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);

    /* Enable BTN_TOUCH */
    ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);

    /* If we need keyboard keys */
    if (need_keys) {
        ioctl(fd, UI_SET_KEYBIT, KEY_BACK);
        ioctl(fd, UI_SET_KEYBIT, KEY_HOME);
        ioctl(fd, UI_SET_KEYBIT, KEY_POWER);
        ioctl(fd, UI_SET_KEYBIT, KEY_VOLUMEUP);
        ioctl(fd, UI_SET_KEYBIT, KEY_VOLUMEDOWN);
        ioctl(fd, UI_SET_KEYBIT, KEY_ENTER);
        ioctl(fd, UI_SET_KEYBIT, KEY_MENU);
    }

    /* Enable ABS axes */
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);

    /* Multi-touch support */
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_SLOT);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);

    /* Set INPUT_PROP_DIRECT - this makes it a direct touchscreen! */
    ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);

    /* Setup ABS axis parameters */
    struct uinput_abs_setup abs_x = {0};
    abs_x.code = ABS_X;
    abs_x.absinfo.maximum = SCREEN_W - 1;
    abs_x.absinfo.resolution = 1;
    ioctl(fd, UI_ABS_SETUP, &abs_x);

    struct uinput_abs_setup abs_y = {0};
    abs_y.code = ABS_Y;
    abs_y.absinfo.maximum = SCREEN_H - 1;
    abs_y.absinfo.resolution = 1;
    ioctl(fd, UI_ABS_SETUP, &abs_y);

    /* Multi-touch axes */
    struct uinput_abs_setup mt_slot = {0};
    mt_slot.code = ABS_MT_SLOT;
    mt_slot.absinfo.maximum = 9;
    ioctl(fd, UI_ABS_SETUP, &mt_slot);

    struct uinput_abs_setup mt_x = {0};
    mt_x.code = ABS_MT_POSITION_X;
    mt_x.absinfo.maximum = SCREEN_W - 1;
    mt_x.absinfo.resolution = 1;
    ioctl(fd, UI_ABS_SETUP, &mt_x);

    struct uinput_abs_setup mt_y = {0};
    mt_y.code = ABS_MT_POSITION_Y;
    mt_y.absinfo.maximum = SCREEN_H - 1;
    mt_y.absinfo.resolution = 1;
    ioctl(fd, UI_ABS_SETUP, &mt_y);

    struct uinput_abs_setup mt_id = {0};
    mt_id.code = ABS_MT_TRACKING_ID;
    mt_id.absinfo.minimum = 0;
    mt_id.absinfo.maximum = 65535;
    ioctl(fd, UI_ABS_SETUP, &mt_id);

    /* Setup device identity */
    memset(&setup, 0, sizeof(setup));
    strcpy(setup.name, "virtual_touchscreen");
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x1234;
    setup.id.product = 0x5679;
    setup.id.version = 1;
    ioctl(fd, UI_DEV_SETUP, &setup);

    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("UI_DEV_CREATE");
        close(fd);
        return -1;
    }
    return 0;
}

static void touch_down(int x, int y) {
    emit(EV_ABS, ABS_MT_SLOT, 0);
    emit(EV_ABS, ABS_MT_TRACKING_ID, 1);
    emit(EV_ABS, ABS_MT_POSITION_X, x);
    emit(EV_ABS, ABS_MT_POSITION_Y, y);
    emit(EV_ABS, ABS_X, x);
    emit(EV_ABS, ABS_Y, y);
    emit(EV_KEY, BTN_TOUCH, 1);
    syn();
}

static void touch_move(int x, int y) {
    emit(EV_ABS, ABS_MT_POSITION_X, x);
    emit(EV_ABS, ABS_MT_POSITION_Y, y);
    emit(EV_ABS, ABS_X, x);
    emit(EV_ABS, ABS_Y, y);
    syn();
}

static void touch_up(void) {
    emit(EV_ABS, ABS_MT_TRACKING_ID, -1);
    emit(EV_KEY, BTN_TOUCH, 0);
    syn();
}

static void press_key(int code) {
    emit(EV_KEY, code, 1);
    syn();
    usleep(100000);
    emit(EV_KEY, code, 0);
    syn();
}

static int name_to_keycode(const char *name) {
    if (strcmp(name, "home") == 0) return KEY_HOME;
    if (strcmp(name, "back") == 0) return KEY_BACK;
    if (strcmp(name, "power") == 0) return KEY_POWER;
    if (strcmp(name, "volup") == 0) return KEY_VOLUMEUP;
    if (strcmp(name, "voldown") == 0) return KEY_VOLUMEDOWN;
    if (strcmp(name, "enter") == 0) return KEY_ENTER;
    if (strcmp(name, "menu") == 0) return KEY_MENU;
    return -1;
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s tap <x> <y>\n", prog);
    fprintf(stderr, "  %s longpress <x> <y> [ms=1000]\n", prog);
    fprintf(stderr, "  %s swipe <x1> <y1> <x2> <y2> [ms=300]\n", prog);
    fprintf(stderr, "  %s key <home|back|power|volup|voldown|enter|menu>\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc < 2) { print_usage(argv[0]); return 1; }

    int need_keys = (strcmp(argv[1], "key") == 0);
    if (create_device(need_keys) < 0) return 1;
    /* Wait for Android to register the device */
    sleep(2);

    if (strcmp(argv[1], "tap") == 0 && argc >= 4) {
        int x = atoi(argv[2]), y = atoi(argv[3]);
        touch_down(x, y);
        usleep(100000);
        touch_up();
        usleep(300000);
        printf("OK\n");

    } else if (strcmp(argv[1], "longpress") == 0 && argc >= 4) {
        int x = atoi(argv[2]), y = atoi(argv[3]);
        int ms = argc >= 5 ? atoi(argv[4]) : 1000;
        touch_down(x, y);
        usleep(ms * 1000);
        touch_up();
        usleep(300000);
        printf("OK\n");

    } else if (strcmp(argv[1], "swipe") == 0 && argc >= 6) {
        int x1=atoi(argv[2]), y1=atoi(argv[3]);
        int x2=atoi(argv[4]), y2=atoi(argv[5]);
        int ms = argc>=7 ? atoi(argv[6]) : 300;
        int steps = ms / 10; if (steps<1) steps=1;
        touch_down(x1, y1);
        for (int i=1; i<=steps; i++) {
            usleep(10000);
            float p = (float)i / steps;
            touch_move(x1+(int)((x2-x1)*p), y1+(int)((y2-y1)*p));
        }
        usleep(10000);
        touch_up();
        usleep(300000);
        printf("OK\n");

    } else if (strcmp(argv[1], "key") == 0 && argc >= 3) {
        int code = name_to_keycode(argv[2]);
        if (code < 0) {
            fprintf(stderr, "Unknown key: %s\n", argv[2]);
            print_usage(argv[0]);
            ioctl(fd, UI_DEV_DESTROY);
            close(fd);
            return 1;
        }
        press_key(code);
        usleep(300000);
        printf("OK\n");

    } else {
        print_usage(argv[0]);
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
        return 1;
    }

    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
    return 0;
}
