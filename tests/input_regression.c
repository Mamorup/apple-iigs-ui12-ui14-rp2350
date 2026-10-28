/* Native regression tests: actual input.c/model.c with electrical HAL stubs. */
#include <assert.h>
#include <stdio.h>
#include "../firmware/model.c"
#include "../firmware/input.c"
static glu_t test_glu;
static unsigned command_pin, option_pin, reset_count;
void glu_write(uint8_t r, uint8_t d) { glu_mcu_write(&test_glu, r, d); }
void set_apple_button(unsigned pin, int down) {
    assert(pin == 20 || pin == 21);
    if (pin == 20) command_pin = down; else option_pin = down;
}
void system_reset_pulse(void) { ++reset_count; }
uint8_t adb_phy_talk(uint8_t cmd, uint8_t *data, uint8_t cap) {
    (void)cmd; (void)data; (void)cap; return 0;
}
static void fresh(void) { glu_reset(&test_glu); input_init(); }
int main(void) {
    assert(model_selftest() == 0);
    fresh(); input_keycode(0x35);
    assert(input_take_notification() == 0); /* plain Escape */
    fresh(); input_keycode(0x37); input_keycode(0x35);
    assert(command_pin == 1 && option_pin == 0);
    assert(input_take_notification() == 0); /* Command alone is insufficient */
    fresh(); input_keycode(0x36); input_keycode(0x37); input_keycode(0x35);
    assert((test_glu.mods & 0x82) == 0x82);
    assert(input_take_notification() == 0x20);
    assert(input_take_notification() == 0); /* consume once */
    input_keycode(0xb5); input_keycode(0xb7); input_keycode(0xb6);
    assert(command_pin == 0 && held_count == 0 && (test_glu.mods & 0x82) == 0);
    /* C026 notification raises enabled DATA IRQ; status/read ACK clears it. */
    glu_host_end(&test_glu, 2, true, 0x10);
    glu_mcu_write(&test_glu, 7, 0x20);
    assert(glu_irq(&test_glu)); assert(glu_host_peek(&test_glu, 3) == 0x20);
    glu_host_end(&test_glu, 2, false, 0); glu_host_end(&test_glu, 3, false, 0);
    assert(!glu_irq(&test_glu));
    fresh(); input_keycode(0x00); /* A -> C000, not unsolicited status */
    input_service(1, glu_status(&test_glu), 0, 0x32, 0);
    assert(glu_host_peek(&test_glu, 0) == 0xe1);
    assert(input_take_notification() == 0 && key_published == 1);
    glu_host_end(&test_glu, 1, false, 0);
    assert(glu_host_peek(&test_glu, 0) == 0x61); /* C010 clears C000 bit7 */
    /* A held modifier survives host reset, transient notifications do not. */
    fresh(); input_keycode(0x36); input_keycode(0x37); input_keycode(0x35);
    input_host_reset(); assert(command_pin == 1 && (test_glu.mods & 0x82) == 0x82);
    assert(input_take_notification() == 0 && key_head == key_tail);
    input_keycode(0xb5); input_keycode(0xb7); input_keycode(0xb6);
    assert(command_pin == 0 && option_pin == 0 && reset_count == 0);
    /* Raw ADB arrow codes must not assert Control/Shift/Option. */
    const uint8_t arrows[] = {0x08, 0x15, 0x0a, 0x0b};
    for (unsigned i = 0; i < 4; ++i) {
        fresh(); input_keycode(0x3b + i);
        assert((test_glu.mods & 0xc3) == 0 && command_pin == 0 && option_pin == 0);
        input_service(1, glu_status(&test_glu), 0, 0x32, 0);
        assert(glu_host_peek(&test_glu, 0) == (arrows[i] | 0x80));
    }
    fresh(); input_keycode(0x7b); input_keycode(0x7c); input_keycode(0x7d);
    assert((test_glu.mods & 0x43) == 0x43 && option_pin == 1);
    input_keycode(0xfb); input_keycode(0xfc); input_keycode(0xfd);
    assert((test_glu.mods & 0x43) == 0 && option_pin == 0);
    puts("PASS: model, chord/status IRQ, ordinary keys, release, host reset");
    return 0;
}
