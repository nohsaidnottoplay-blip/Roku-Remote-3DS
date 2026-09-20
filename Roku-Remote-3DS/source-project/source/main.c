#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/stat.h>
#include "ecp.h"
#include "ui.h"

#define CONFIG_DIR "sdmc:/3ds/RokuRemote3DS"
#define CONFIG_FILE CONFIG_DIR "/roku_ip.txt"
#define WHITE 0xF7F4FF
#define MUTED 0xBDB3D6
#define PURPLE 0xB084F5

typedef struct {
    bool busy, stop, pending, has_reply;
    char ip[16], key[32];
    EcpReply reply;
} NetworkState;

static NetworkState net;
static LightLock net_lock;
static LightEvent net_event;
static char roku_ip[16];
static char status_text[128] = "Open Setup to enter your Roku IP.";
static int connection = 0; /* 0 unchecked, 1 responded, -1 error */
static bool networking = false;

static const Button controls[] = {
    {8,8,96,30,"BACK","Back"}, {112,8,96,30,"HOME","Home"}, {216,8,96,30,"OPTIONS","Info"},
    {113,47,94,33,"UP","Up"}, {16,87,88,40,"LEFT","Left"},
    {113,87,94,40,"OK","Select"}, {216,87,88,40,"RIGHT","Right"},
    {113,134,94,33,"DOWN","Down"},
    {16,137,88,28,"VOL -","VolumeDown"}, {216,137,88,28,"VOL +","VolumeUp"},
    {8,177,67,30,"<<","Rev"}, {82,177,156,30,"PLAY/PAUSE","Play"}, {245,177,67,30,">>","Fwd"},
    {8,214,96,22,"REPLAY","InstantReplay"}, {112,214,96,22,"MUTE","VolumeMute"}, {216,214,96,22,"SETUP",NULL}
};
static const Button settings[] = {
    {16,16,288,38,"ENTER ROKU IP",NULL},
    {16,64,288,38,"TEST CONNECTION",NULL},
    {16,112,288,38,"SETUP HELP",NULL},
    {16,178,288,38,"BACK TO REMOTE",NULL}
};

static void set_status(const char *s) { snprintf(status_text, sizeof(status_text), "%s", s); }

static void network_thread(void *unused) {
    (void)unused;
    for (;;) {
        LightEvent_Wait(&net_event);
        LightLock_Lock(&net_lock);
        bool stop = net.stop, pending = net.pending;
        char ip[16], key[32];
        memcpy(ip, net.ip, sizeof(ip));
        memcpy(key, net.key, sizeof(key));
        net.pending = false;
        LightLock_Unlock(&net_lock);
        if (stop) break;
        if (!pending) continue;
        EcpReply r = ecp_request(ip, *key ? key : NULL, 2200);
        LightLock_Lock(&net_lock);
        net.reply = r;
        net.has_reply = true;
        net.busy = false;
        LightLock_Unlock(&net_lock);
    }
}

static bool is_busy(void) {
    LightLock_Lock(&net_lock);
    bool busy = net.busy;
    LightLock_Unlock(&net_lock);
    return busy;
}

static bool submit(const char *key) {
    if (!networking) { set_status("Network unavailable. Restart with Wi-Fi on."); return false; }
    if (!ecp_valid_ip(roku_ip)) { set_status("Open Setup and enter your Roku IP first."); return false; }
    LightLock_Lock(&net_lock);
    if (net.busy || net.has_reply) { LightLock_Unlock(&net_lock); return false; }
    snprintf(net.ip, sizeof(net.ip), "%s", roku_ip);
    snprintf(net.key, sizeof(net.key), "%s", key ? key : "");
    net.pending = true;
    net.busy = true;
    LightLock_Unlock(&net_lock);
    set_status(key ? "Sending..." : "Checking Roku...");
    LightEvent_Signal(&net_event);
    return true;
}

static void take_reply(void) {
    LightLock_Lock(&net_lock);
    if (net.has_reply) {
        connection = net.reply.result == ECP_OK ? 1 : -1;
        set_status(net.reply.message);
        net.has_reply = false;
    }
    LightLock_Unlock(&net_lock);
}

static void load_config(void) {
    FILE *f = fopen(CONFIG_FILE, "rb");
    if (!f) return;
    char line[64];
    if (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (ecp_valid_ip(line)) memcpy(roku_ip, line, strlen(line) + 1);
    }
    fclose(f);
}

static bool save_config(void) {
    mkdir("sdmc:/3ds", 0777);
    mkdir(CONFIG_DIR, 0777);
    FILE *f = fopen(CONFIG_FILE ".tmp", "wb");
    if (!f) return false;
    bool ok = fprintf(f, "%s\n", roku_ip) > 0;
    if (fclose(f) != 0) ok = false;
    if (ok && rename(CONFIG_FILE ".tmp", CONFIG_FILE) == 0) return true;
    remove(CONFIG_FILE ".tmp");
    return false;
}

static void enter_ip(void) {
    if (is_busy()) { set_status("Wait for the current request to finish."); return; }
    SwkbdState keyboard;
    char input[16] = {0};
    swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, 15);
    swkbdSetHintText(&keyboard, "Roku IP (Settings > Network > About)");
    swkbdSetInitialText(&keyboard, roku_ip);
    swkbdSetButton(&keyboard, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&keyboard, SWKBD_BUTTON_RIGHT, "Save", true);
    if (swkbdInputText(&keyboard, input, sizeof(input)) != SWKBD_BUTTON_RIGHT) return;
    if (!ecp_valid_ip(input)) {
        set_status("Invalid IP. Use four numbers, like 192.168.1.25.");
        return;
    }
    snprintf(roku_ip, sizeof(roku_ip), "%s", input);
    connection = 0;
    if (save_config()) set_status("IP saved. Choose Test connection.");
    else set_status("IP set for this session. Could not save to SD.");
}

static void draw_top(Canvas c, int page, bool busy) {
    ui_clear(c, 0x171225);
    ui_rect(c, 0, 0, 400, 5, 0x9260D9);
    ui_text(c, 20, 22, 3, WHITE, "ROKU REMOTE");
    ui_text(c, 22, 52, 1, MUTED, "NINTENDO 3DS  /  PERSONAL HOMEBREW");
    ui_round(c, 20, 74, 360, 75, 10, 0x2A213E);
    uint32_t indicator = busy ? 0xE5BC70 : connection > 0 ? 0x71DDB4 : connection < 0 ? 0xF39D9D : 0xAA9BBF;
    ui_round(c, 34, 89, 10, 10, 5, indicator);
    ui_text(c, 53, 86, 2, WHITE, *roku_ip ? roku_ip : "NO ROKU SET");
    ui_wrap(c, 34, 111, 330, 1, MUTED, status_text);
    if (page == 0) {
        ui_text(c, 22, 167, 1, PURPLE, "D-PAD: MOVE     A: OK       B: BACK");
        ui_text(c, 22, 183, 1, WHITE, "X: HOME        Y: PLAY/PAUSE");
        ui_text(c, 22, 199, 1, WHITE, "L/R: VOLUME    SELECT: SETUP");
        ui_text(c, 22, 223, 1, MUTED, "START: EXIT       VOLUME NEEDS ROKU SUPPORT");
    } else if (page == 1) {
        ui_text(c, 22, 165, 2, WHITE, "CONNECT YOUR ROKU");
        ui_text(c, 22, 191, 1, MUTED, "FIND ITS IP ON YOUR TV:");
        ui_text(c, 22, 207, 1, WHITE, "SETTINGS > NETWORK > ABOUT");
        ui_text(c, 22, 225, 1, MUTED, "D-PAD + A OR TAP     B: BACK     START: EXIT");
    } else {
        ui_text(c, 22, 166, 2, WHITE, "BEFORE YOU CONNECT");
        ui_wrap(c, 22, 194, 354, 1, MUTED,
            "TURN THE ROKU ON. CONNECT BOTH DEVICES TO YOUR HOME NETWORK. "
            "AVOID GUEST WI-FI THAT BLOCKS DEVICE COMMUNICATION.");
    }
}

static void draw_bottom(Canvas c, int page, int selected, int pressed) {
    ui_clear(c, 0x20192F);
    if (page == 0) {
        for (unsigned i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i)
            ui_button(c, controls[i], (int)i == pressed || i == 5);
    } else if (page == 1) {
        for (int i = 0; i < 4; ++i) ui_button(c, settings[i], i == selected);
        ui_text(c, 50, 228, 1, MUTED, "YOUR IP IS SAVED ON THE SD CARD");
    } else {
        ui_text(c, 16, 14, 2, WHITE, "ROKU SETTINGS");
        ui_wrap(c, 16, 43, 288, 1, MUTED,
            "SYSTEM > ADVANCED SYSTEM SETTINGS > CONTROL BY MOBILE APPS\n\n"
            "SET TO ENABLED. OLDER SOFTWARE MAY SHOW NETWORK ACCESS > DEFAULT.\n\n"
            "IF CONTROL IS STILL DENIED, YOUR ROKU MAY RESTRICT THIRD-PARTY REMOTES.\n\n"
            "THIS APP DOES NOT NEED YOUR ROKU PASSWORD OR A COMPUTER SERVER.");
        ui_button(c, settings[3], 1);
    }
}

static int index_for(const char *key) {
    for (unsigned i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i)
        if (controls[i].key && !strcmp(key, controls[i].key)) return (int)i;
    return -1;
}

int main(void) {
    gfxInitDefault();
    gfxSet3D(false);
    LightLock_Init(&net_lock);
    LightEvent_Init(&net_event, RESET_ONESHOT);
    void *soc_buffer = memalign(0x1000, 0x100000);
    bool soc_ready = soc_buffer && R_SUCCEEDED(socInit(soc_buffer, 0x100000));
    Thread worker = NULL;
    if (soc_ready) worker = threadCreate(network_thread, NULL, 64 * 1024, 0x31, -2, false);
    networking = worker != NULL;
    load_config();
    int page = *roku_ip ? 0 : 1, selected = 0, pressed = -1;
    uint64_t flash_until = 0, repeat_at = 0;
    u32 repeat_key = 0;
    if (!networking) set_status("Network init failed. Restart with Wi-Fi on.");
    else if (*roku_ip) submit(NULL);

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown(), held = hidKeysHeld();
        uint64_t now = osGetTime();
        take_reply();
        if (down & KEY_START) break;
        if (now >= flash_until) pressed = -1;
        if (down & KEY_SELECT) { page = page ? 0 : 1; repeat_key = 0; }
        const char *key = NULL;

        if (page == 0) {
            if (down & KEY_A) key = "Select";
            else if (down & KEY_B) key = "Back";
            else if (down & KEY_X) key = "Home";
            else if (down & KEY_Y) key = "Play";
            const u32 bits[] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_L, KEY_R};
            const char *names[] = {"Up", "Down", "Left", "Right", "VolumeDown", "VolumeUp"};
            for (int i = 0; i < 6; ++i) {
                if (down & bits[i]) { key = names[i]; repeat_key = bits[i]; repeat_at = now + 380; break; }
                if (repeat_key == bits[i] && (held & bits[i]) && now >= repeat_at) {
                    key = names[i]; repeat_at = now + 160; break;
                }
            }
            if (!(held & repeat_key)) repeat_key = 0;
            if (down & KEY_TOUCH) {
                touchPosition touch;
                hidTouchRead(&touch);
                for (unsigned i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
                    if (!ui_hit(controls[i], touch.px, touch.py)) continue;
                    if (!controls[i].key) page = 1;
                    else key = controls[i].key;
                    break;
                }
            }
        } else {
            repeat_key = 0;
            int action = -1;
            if (down & KEY_B) page = page == 2 ? 1 : 0;
            else if (page == 1) {
                if (down & KEY_UP) selected = (selected + 3) % 4;
                if (down & KEY_DOWN) selected = (selected + 1) % 4;
                if (down & KEY_A) action = selected;
                if (down & KEY_TOUCH) {
                    touchPosition touch; hidTouchRead(&touch);
                    for (int i = 0; i < 4; ++i)
                        if (ui_hit(settings[i], touch.px, touch.py)) { selected = i; action = i; }
                }
                if (action == 0) enter_ip();
                if (action == 1) submit(NULL);
                if (action == 2) page = 2;
                if (action == 3) page = 0;
            } else if (down & KEY_A) page = 1;
            else if (down & KEY_TOUCH) {
                touchPosition touch; hidTouchRead(&touch);
                if (ui_hit(settings[3], touch.px, touch.py)) page = 1;
            }
        }
        if (key && submit(key)) { pressed = index_for(key); flash_until = now + 140; }

        Canvas top = {gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), 400, 240};
        Canvas bottom = {gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL), 320, 240};
        draw_top(top, page, is_busy());
        draw_bottom(bottom, page, selected, pressed);
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
    if (worker) {
        LightLock_Lock(&net_lock); net.stop = true; LightLock_Unlock(&net_lock);
        LightEvent_Signal(&net_event);
        threadJoin(worker, U64_MAX);
        threadFree(worker);
    }
    if (soc_ready) socExit();
    free(soc_buffer);
    gfxExit();
    return 0;
}
