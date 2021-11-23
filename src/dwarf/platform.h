#ifndef EGALITO_DWARF_PLATFORM_H
#define EGALITO_DWARF_PLATFORM_H

#include <string>

std::string getRegisterName(unsigned int reg);
const char *shortRegisterName(unsigned int reg);

#endif
