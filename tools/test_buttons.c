#include <assert.h>
#include <stdio.h>
#include "buttons.h"

int main(void)
{
    Buttons s;
    unsigned i;
    uint32_t start = 0xFFFFFFE0u;
    for (i = 0; i < 4; ++i) {
        unsigned bit = 1u << i;
        Buttons_Init(&s, 0, 0);
        assert(!Buttons_Update(&s, bit, 10));
        assert(!Buttons_Update(&s, 0, 15));
        assert(!Buttons_Update(&s, bit, 20));
        assert(!Buttons_Update(&s, bit, 49));
        assert(Buttons_Update(&s, bit, 50) == BUTTON_PRESS(i));
        assert(!Buttons_Update(&s, 0, 100));
        assert(!Buttons_Update(&s, 0, 129));
        assert(Buttons_Update(&s, 0, 130) == BUTTON_SHORT(i));
        assert(!Buttons_Update(&s, 0, 200));

        Buttons_Init(&s, 0, 0);
        assert(!Buttons_Update(&s, bit, 10));
        assert(Buttons_Update(&s, bit, 40) == BUTTON_PRESS(i));
        assert(!Buttons_Update(&s, bit, 1239));
        assert(Buttons_Update(&s, bit, 1240) == BUTTON_LONG(i));
        assert(!Buttons_Update(&s, bit, 3000));
        assert(!Buttons_Update(&s, 0, 3010));
        assert(!Buttons_Update(&s, 0, 3040)); /* no short action after long */

        Buttons_Init(&s, bit, 0); /* green held while powering up */
        assert(!Buttons_Update(&s, bit, 5000));
        assert(!Buttons_Update(&s, 0, 5010));
        assert(!Buttons_Update(&s, 0, 5040));
        assert(!Buttons_Update(&s, bit, 5100));
        assert(Buttons_Update(&s, bit, 5130) == BUTTON_PRESS(i));
        assert(!Buttons_Update(&s, 0, 5200));
        assert(Buttons_Update(&s, 0, 5230) == BUTTON_SHORT(i));
    }
    Buttons_Init(&s, 0, 0);
    assert(!Buttons_Update(&s, 15, 100));
    assert(Buttons_Update(&s, 15, 130) == 15);
    assert(!Buttons_Update(&s, 0, 200));
    assert(Buttons_Update(&s, 0, 230) == 0xF0);
    Buttons_Init(&s, 0, start);
    assert(!Buttons_Update(&s, 1, start + 1));
    assert(Buttons_Update(&s, 1, start + 31) == BUTTON_PRESS(0));
    assert(!Buttons_Update(&s, 1, start + 1200));
    assert(Buttons_Update(&s, 1, start + 1231) == BUTTON_LONG(0));
    puts("PASS buttons: bounce, short/long, held at boot, simultaneous keys, tick wrap");
    return 0;
}
