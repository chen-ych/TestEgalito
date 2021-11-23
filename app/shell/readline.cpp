#include "readline.h"
#include "config.h"

#define HAVE_READLINE

#ifndef HAVE_READLINE
#include <iostream>

Readline::Readline() {}
Readline::~Readline() {}

std::string Readline::get(const std::string &prompt) {
    std::cout << prompt;
    std::cout.flush();

    std::string line;
    std::getline(std::cin, line);

    return std::move(line);
}
#else
#include <readline/readline.h>
#include <readline/history.h>

#define HISTORY_FILE ".etshell_history"

Readline::Readline() {
    read_history(HISTORY_FILE);
    rl_bind_key('\t', rl_abort);  // disable auto-complete
}

Readline::~Readline() {
    write_history(HISTORY_FILE);
}

#define C_WHITE "37"
#define C_GREEN "32"
#ifndef PROMPT_COLOR
#define PROMPT_COLOR C_WHITE
#endif

#define UNPRINTABLE(text) "\001" text "\002"

std::string Readline::get(const std::string &prompt) {
    std::string prompt2
        = UNPRINTABLE("\033[1;" PROMPT_COLOR "m")
        + prompt
        + UNPRINTABLE("\033[0m");

    char *line = readline(prompt2.c_str());
    if(line && *line) {
        add_history(line);
    }

    std::string lineCpp(line ? line : "quit");
    free(line);
    return std::move(lineCpp);
}
#endif
