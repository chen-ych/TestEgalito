#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <windows.h>

size_t read_file(char **file, FILE *fp);
void windows_error(DWORD error);

int main(void) {
    HGLOBAL gmem;
    char *to, *data = 0;
    size_t len;
    DWORD error = ERROR_SUCCESS;

    len = read_file(&data, stdin);

    if(!(gmem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, len + 1))
        || !(to = GlobalLock(gmem))) {

        error = GetLastError();
    }
    else {
        memcpy(to, data, len);

        GlobalUnlock(gmem);

        if(OpenClipboard(NULL)) {
            if(!EmptyClipboard() || !SetClipboardData(CF_TEXT, gmem)) {
                error = GetLastError();
            }

            if(!CloseClipboard()) error = GetLastError();
        }
        else error = GetLastError();
    }

    free(data);

    if(error != ERROR_SUCCESS) windows_error(error);

    return error;
}

/*! Reads all the characters in \a fp until EOF, storing them in the
    dynamically allocated string \a data. Returns the number of characters
    read. Prints an error message and returns if it runs out of memory.
    \param file The pointer to store the data in.
    \param fp The file to read the characters from.
    \return The number of characters read from the file.
*/
size_t read_file(char **file, FILE *fp) {
    char *p;
    int c = 0;
    size_t len = 0, alen = 0;

    do {
        c = getc(fp);

        if(c == EOF) c = 0;
        else putchar(c);

        if(len >= alen) {
            alen += BUFSIZ;
            p = realloc(*file, alen);

            if(!p) {
                fprintf(stderr, "inclip: read_file(): Out of memory\n");

                if(len) {
                    (*file)[len-1] = 0;
                }

                return len;
            }

            *file = p;
        }

        (*file)[len++] = c;
    } while(c);

    return len - 1;
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

    fprintf(stderr, "cliptee: %s\n", (const char *)message);

    MessageBox(NULL, (LPCTSTR)message, "cliptee: Error",
        MB_OK | MB_ICONINFORMATION);

    LocalFree(message);
}
