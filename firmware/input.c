#include "input.h"
#include "hal.h"




#include <string.h>
#include <stdio.h>

enum { KEY_QUEUE_SIZE = 8 };
static uint8_t modifiers, held[16], key_queue[KEY_QUEUE_SIZE];
static uint8_t key_head, key_tail, held_count;
static uint8_t mouse_pending, mouse_y, mouse_x;
static uint32_t next_poll;
static uint8_t poll_mouse;
static uint8_t reset_requested;
static uint8_t notification;
static uint32_t key_events, key_published, keyboard_packets, desk_events;
static uint8_t recent_keys[16], recent_pos, last_ascii;

uint8_t input_any_key_down(void) { return held_count != 0; }

uint8_t input_take_notification(void) {
    uint8_t value = notification; notification = 0; return value;
}
void input_report(void) {
    printf("INPUT events=%lu packets=%lu published=%lu mods=%02x held=%u pending=%u last_ascii=%02x notify=%02x desk=%lu recent=",
        (unsigned long)key_events, (unsigned long)keyboard_packets,
        (unsigned long)key_published, modifiers, held_count,
        (unsigned)((key_head + KEY_QUEUE_SIZE - key_tail) % KEY_QUEUE_SIZE),
        last_ascii, notification, (unsigned long)desk_events);
    unsigned n = key_events < 16 ? key_events : 16;
    for (unsigned i = 0; i < n; ++i) printf(" %02x", recent_keys[(recent_pos + 16 - n + i) & 15]);
    puts("");
}

static void button_output(uint8_t bit, uint8_t pressed) {set_apple_button(bit==1?21:20,pressed!=0);}
#define _BV(x) (1u<<(x))
#define PB0 0
#define PB1 1
static void publish_modifiers(void) {
    glu_write(6, modifiers);
    button_output(_BV(PB0), modifiers & 0x40);
    button_output(_BV(PB1), modifiers & 0x80);
}
static void queue_key(uint8_t value) {
    uint8_t next = (uint8_t)((key_head + 1) % KEY_QUEUE_SIZE);
    if (next != key_tail) { key_queue[key_head] = value; key_head = next; }
}
static uint8_t normal_char(uint8_t code) {
    static const uint8_t table[64] = {
        'a','s','d','f','h','g','z','x','c','v',0,'b','q','w','e','r',
        'y','t','1','2','3','4','6','5','=','9','7','-','8','0',']','o',
        'u','[','i','p','\r','l','j','\'','k',';','\\',',','/','n','m','.',
        '\t',' ','`',0x7f,0,0x1b,0,0,0,0,0,0,0,0,0,0
    };
    return table[code & 0x3f];
}
static uint8_t shifted_char(uint8_t value) {
    if (value >= 'a' && value <= 'z') return (uint8_t)(value - 'a' + 'A');
    switch (value) {
    case '1': return '!'; case '2': return '@'; case '3': return '#';
    case '4': return '$'; case '5': return '%'; case '6': return '^';
    case '7': return '&'; case '8': return '*'; case '9': return '(';
    case '0': return ')'; case '-': return '_'; case '=': return '+';
    case '[': return '{'; case ']': return '}'; case '\\': return '|';
    case ';': return ':'; case '\'': return '"'; case ',': return '<';
    case '.': return '>'; case '/': return '?'; case '`': return '~';
    default: return value;
    }
}
static uint8_t translate(uint8_t code) {
    uint8_t value;
    switch (code) {
    /* Raw ADB transition codes differ from Macintosh Keyboard Driver codes. */
    case 0x3b: return 0x08;
    case 0x3c: return 0x15;
    case 0x3d: return 0x0a;
    case 0x3e: return 0x0b;
    case 0x34: return '\r';
    case 0x4c: return '\r';
    case 0x41: return '.'; case 0x43: return '*'; case 0x45: return '+';
    case 0x4b: return '/'; case 0x4e: return '-'; case 0x51: return '=';
    case 0x52: return '0'; case 0x53: return '1'; case 0x54: return '2';
    case 0x55: return '3'; case 0x56: return '4'; case 0x57: return '5';
    case 0x58: return '6'; case 0x59: return '7'; case 0x5b: return '8';
    case 0x5c: return '9';
    default: if (code >= 64) return 0; value = normal_char(code); break;
    }
    if (!value) return 0;
    if (value >= 'a' && value <= 'z') {
        if (!!(modifiers & 0x01) ^ !!(modifiers & 0x04)) value = shifted_char(value);
        if (modifiers & 0x02) value &= 0x1f;
    } else if (modifiers & 0x01) value = shifted_char(value);
    return value;
}

void input_init(void) {
    modifiers = 0; memset(held, 0, sizeof(held)); held_count = 0;
    key_head = key_tail = 0; mouse_pending = 0; next_poll = 0; poll_mouse = 0;
    reset_requested = 0;
    notification = 0;
    key_events = key_published = keyboard_packets = desk_events = 0;
    recent_pos = last_ascii = 0;
    button_output(_BV(PB0), 0); button_output(_BV(PB1), 0);
}

// CPU/system RESET does not physically release a held keyboard key.
// Keep modifier and held-key state so Command+Reset remains asserted while
// the host executes its reset vector; only transient event queues are cleared.
void input_host_reset(void) {
    key_head = key_tail = 0; mouse_pending = 0; next_poll = 0; poll_mouse = 0;
    reset_requested = 0;
    notification = 0;
    publish_modifiers();
    glu_write(5, held_count ? 0x80 : 0);
}

void input_keycode(uint8_t adb_code) {
    if (adb_code == 0xff) return;
    ++key_events; recent_keys[recent_pos] = adb_code; recent_pos = (recent_pos + 1) & 15;
    if (adb_code == 0x7f) { reset_requested = 1; return; }
    uint8_t up = adb_code >> 7, code = adb_code & 0x7f;
    uint8_t mask = (uint8_t)(1u << (code & 7));
    uint8_t *slot = &held[code >> 3];
    if (up) {
        if (*slot & mask) { *slot &= (uint8_t)~mask; if (held_count) --held_count; }
    } else if (!(*slot & mask)) { *slot |= mask; ++held_count; }

    uint8_t modifier = 0;
    uint8_t keypad = (code == 0x41 || code == 0x43 || code == 0x45 ||
        code == 0x4b || code == 0x4c || code == 0x4e || code == 0x51 ||
        (code >= 0x52 && code <= 0x5c));
    if (keypad) {
        if (up) modifiers &= (uint8_t)~0x10; else modifiers |= 0x10;
        modifiers |= 0x20; publish_modifiers();
    }
    if (code == 0x38 || code == 0x7b) modifier = 0x01;
    else if (code == 0x36 || code == 0x7d) modifier = 0x02;
    else if (code == 0x3a || code == 0x7c) modifier = 0x40;
    else if (code == 0x37 || code == 0x7e) modifier = 0x80;
    else if (code == 0x39) {
        if (!up) modifiers ^= 0x04;
        modifiers |= 0x20; publish_modifiers();
        glu_write(5, held_count ? 0x80 : 0); return;
    }
    if (modifier) {
        if (up) modifiers &= (uint8_t)~modifier; else modifiers |= modifier;
        modifiers |= 0x20; publish_modifiers();
    } else if (!up) {
        /* Firmware Reference: Ctrl-Command-Escape requests Desk Manager
         * through C026 status bit5 and the DATA interrupt, not just C000. */
        if (code == 0x35 && (modifiers & 0x82) == 0x82) {
            notification |= 0x20; ++desk_events;
        }
        uint8_t value = translate(code); if (value) queue_key(value);
    }
    glu_write(5, held_count ? 0x80 : 0);
}

void input_service(uint32_t now_ms, uint8_t status, uint8_t modes,
                   uint8_t address_byte, uint8_t enabled) {
#if DISABLE_ADB_AUTOPOLL
    (void)now_ms;
    (void)status;
    (void)modes;
    (void)address_byte;
    (void)enabled;
    (void)mouse_y;
    (void)mouse_x;
    /* No keyboard or mouse is fitted during board diagnostics.  Suppress
     * TALK traffic and prevent line noise from becoming reset keycode $7F. */
    reset_requested = 0;
    return;
#else
    if (reset_requested) {
        reset_requested = 0;
        if ((modes & 0x80) || (modifiers & 0x02)) {
            system_reset_pulse();
        }
    }
    if (!(status & 0x10) && key_head != key_tail) {
        last_ascii = key_queue[key_tail]; ++key_published;
        glu_write(0, (uint8_t)(key_queue[key_tail] | 0x80));
        key_tail = (uint8_t)((key_tail + 1) % KEY_QUEUE_SIZE);
    }
    if (!(status & 0x80) && mouse_pending) {
        glu_write(3, mouse_y);
        glu_write(2, mouse_x); /* X write marks the Y/X pair full */
        mouse_pending = 0;
    }
    if (enabled && next_poll == 0) { next_poll = now_ms + 20; return; }
    if (!enabled || (int32_t)(now_ms - next_poll) < 0) return;
    next_poll = now_ms + 8;
    uint8_t packet[2], length;
    if (poll_mouse) {
        poll_mouse = 0;
        if (modes & 0x02) return;
        uint8_t addr = address_byte >> 4;
        length = adb_phy_talk((uint8_t)((addr << 4) | 0x0c), packet, 2);
        if (length == 2) { mouse_y = packet[0]; mouse_x = packet[1]; mouse_pending = 1; }
    } else {
        poll_mouse = 1;
        if (modes & 0x01) return;
        uint8_t addr = address_byte & 0x0f;
        length = adb_phy_talk((uint8_t)((addr << 4) | 0x0c), packet, 2);
        if (length) ++keyboard_packets;
        for (uint8_t i = 0; i < length; ++i) input_keycode(packet[i]);
    }
#endif
}
