#include "config.h"

/** This file contains the default log levels for other source files.

    If any constant is set to -1, those messages are removed at compile time.
*/

// Default groups created from subdirectory names
#ifdef RELEASE_BUILD
#define D_analysis      -1
#define D_archive       -1
#define D_break         -1
#define D_chunk         -1
#define D_conductor     -1
#define D_disasm        -1
#define D_dwarf         -1
#define D_elf           -1
#define D_generate      -1
#define D_instr         -1
#define D_load          -1
#define D_log           -1
#define D_main          -1
#define D_operation     -1
#define D_pass          -1
#define D_snippet       -1
#define D_transform     -1
#define D_util          -1

// Custom groups
#define D_dsymbol       -1
#define D_dreloc        -1
#define D_djumptable    -1
#define D_dplt          -1
#define D_dloadtime     -1
#define D_dassign       -1
#define D_dtiming       -1
#define D_dreorder      -1
#else   /* debug build */
#define D_analysis      9
#define D_archive       9
#define D_break         9
#define D_chunk         9
#define D_conductor     9
#define D_disasm        9
#define D_dwarf         9
#define D_elf           9
#define D_generate      9
#define D_instr         9
#define D_load          9
#define D_log           0
#define D_main          9
#define D_operation     9
#define D_pass          9
#define D_snippet       9
#define D_transform     9
#define D_util          9

// Custom groups
#define D_dsymbol       3
#define D_dreloc        0
#define D_djumptable    0
#define D_dplt          0
#define D_dloadtime     0
#define D_dassign       0
#define D_dtiming       9
#define D_dreorder      0
#endif
