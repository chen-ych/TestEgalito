#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <windows.h>

void windows_error(DWORD error);

int main(void) {
    DWORD error = ERROR_SUCCESS;

    if(OpenClipboard(NULL)) {
        if(!EmptyClipboard()) {
            error = GetLastError();
        }

        if(!CloseClipboard()) error = GetLastError();
    }
    else error = GetLastError();

    if(error != ERROR_SUCCESS) windows_error(error);

    return error;
}

/*! Prints and pops up an error message from a Windows function. */
void windows_error(DWORD error) {
    LPVOID message;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  /* Default language */
        (LPTSTR)&message,
        0,
        NULL
    );

    fprintf(stderr, "clipclr: %s\n", (const char *)message);

    MessageBox(NULL, (LPCTSTR)message, "clipclr: Error",
        MB_OK | MB_ICONINFORMATION);

    LocalFree(message);
}
