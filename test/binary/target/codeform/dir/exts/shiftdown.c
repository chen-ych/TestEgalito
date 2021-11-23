#include <windows.h>

int main(void) {
    return GetAsyncKeyState(VK_SHIFT) & 0x8000 ? 1 : 0;
}
