/*! \mainpage
    Returns true if a given key is held down.

    Usage: asyncdown [left|right|either|both] [shift|ctrl|alt|win]

    left:   Return true if the left key is held down
    right:  Return true if the right key is held down
    either: Return true if either key is held down
    both:   Return true if both left and right keys are held down
*/

#include <string.h>
#include <windows.h>

#define KEYDOWN(key) (GetAsyncKeyState(key) & 0x8000 ? 1 : 0)

enum which_t {
    WHICH_LEFT,
    WHICH_RIGHT,
    WHICH_EITHER,
    WHICHES,
    WHICH_BOTH  /* Special case -- left and right */
};

char *whiches[] = {
    "left",
    "right",
    "either",
    "both"
};

enum type_t {
    TYPE_SHIFT,
    TYPE_CTRL,
    TYPE_ALT,
    TYPE_WIN,
    TYPES
};

struct data_t {
    char *name;
    int vk[WHICHES];
} data[] = {
    {"shift", {VK_LSHIFT, VK_RSHIFT, VK_SHIFT}},
    {"ctrl", {VK_LCONTROL, VK_RCONTROL, VK_CONTROL}},
    {"alt", {VK_LMENU, VK_RMENU, VK_MENU}},
    {"win", {VK_LWIN, VK_RWIN, 0}},
};

int main(int argc, char *argv[]) {
    int x;
    enum type_t type = TYPE_SHIFT, t;
    enum which_t which = WHICH_EITHER, w;

    for(x = 1; x < argc; x ++) {
        for(t = 0; t < TYPES; t ++) {
            if(!strcmp(argv[x], data[t].name)) type = t;
        }

        for(w = 0; w < WHICHES; w ++) {
            if(!strcmp(argv[x], whiches[w])) which = w;
        }
    }

    if(which == WHICH_BOTH) {
        return KEYDOWN(data[type].vk[WHICH_LEFT])
            && KEYDOWN(data[type].vk[WHICH_RIGHT]);
    }

    return KEYDOWN(data[type].vk[which]);
}
