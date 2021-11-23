/*! \mainpage

    codeform, a code formatter and colourer [intended] for C and C++ \n
    Copyright (C) 2007 DWK

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    The GNU General Public License version 2 is included in the file COPYING.

    \author DWK
        dwks@theprogrammingsite.com \n
        http://dwks.theprogrammingsite.com/ \n

    \version 1.2.0

    At the time of this writing, codeform is available at:
        http://dwks.theprogrammingsite.com/myprogs/codeform.htm
*/

/*! \file codeform.c

    The one and only source file for codeform.

    This source file compiles without warnings with GCC 2.95.2 and 3.3.3: \n
        $ gcc -W -Wall -ansi -pedantic -O2 -g -o codeform codeform.c
*/

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*! Version of codeform. */
#define VERSION "codeform version 1.2.0 by DWK"

/*! An array of strings. Stores the length of each string. */
struct strings_t {
    char **data;
    size_t *len;
    size_t number;
};

/*! The type of a rule. */
enum type_t {
    TYPE_KEYWORD,
    TYPE_MIDWORD,
    TYPE_COMMENT,
    TYPE_STRING,
    TYPE_NESTCOM,
    TYPE_NUMBER,
    TYPE_FPNUMBER,
    TYPE_START,
    TYPE_END,
    TYPES,  /*!< Number of values for \c type_t (this enum). */
    TYPE_ERROR = -1
};

/*! Flag values to indicate the position in the current file (can be OR'd
    together).
*/
enum pos_t {
    POS_START = 1,
    POS_END = 2
};

/*! File names passed to the program on the command line. */
struct argument_t {
    struct strings_t inputfn, outputfn, rulefn, ilrule;
};

/*! A rule, containing sections and a type. */
struct onerule_t {
    struct strings_t data;  /*!< The data for the rule. */

    size_t type;  /*!< Type of the rule (type_func number). */

    size_t prev;  /*!< Rule starting with the same as this one. */
};

/*! A pointer to the previous rule parsed (for '*' sections). */
struct prevrule_t {
    enum type_t type;        /* The current header type. */
    struct onerule_t *one;   /* The current rule being parsed. */
    struct onerule_t *prev;  /* The previous rule. */
    /* True if prev is the only pointer to a dynamically allocated structure,
        and so needs freeing. */
    int freep;
};

/*! A variable. */
struct onevar_t {
    /* The name of the variable and the text to replace it with. */
    char *from, *to;
    size_t flen, tlen;  /* The length of \c from and \c to. */
};

/*! Array of all the rules. */
struct ruledata_t {
    struct onerule_t **data;
    size_t number;
};

/*! List of type_func functions, representing which comments the parser is
    inside. Since comments can be nested, it stores a list of function numbers.
*/
struct funclist_t {
    size_t *which;
    enum type_t *type;
    size_t number;

    /* True if the current comment supports nesting of other comments inside it. */
    int nest;
};

/*! The previous tag, its beginning and end. */
struct rulelist_t {
    const char *from, *to;
    char *ws;
    size_t wslen;
};

/*! Structure passed to the type_func functions, containing all of the non-rule
    related data they might need.
*/
struct typefunc_t {
    FILE *out;  /* Output file stream to write data to. */
    char **p;  /* Pointer to the current position in the string. */
    int iw;  /* True if the previous character was a word character. */
    enum pos_t pos;  /* Flags for the beginning and end of the file. */
    size_t n;  /* Character count from the beginning of the string. */
    size_t number;  /* Element in ruledata_t that the rule is. */
    enum type_t type;  /* The type of rule. */

    struct funclist_t func;  /* List of nested comments. */
    struct rulelist_t *list;  /* The previous tag. */
};

/*! A rule type. */
struct ruletype_t {
    const char *name;  /* The name of this type of rule. */
    size_t sort;  /* The part to sort by. */
    int parts, xparts;  /* Minimum parts, and optional extra parts. */

    /* A pointer to the function that handles this type of rule. */
    int (*func)(struct onerule_t *rule, struct typefunc_t *tf);
};

/*! Array of variables. */
struct rulevars_t {
    struct onevar_t **data;
    size_t number;
};

/*! Base rules structure, containing types, rules, and variables. */
struct rules_t {
    struct ruletype_t type[TYPES];
    struct rulevars_t vars;
    struct rulelist_t list;
    struct ruledata_t data;
    struct onerule_t *cdat[TYPES];
    struct prevrule_t pr;
};

/*----------------------*\
 | Function prototypes. |
\*----------------------*/

void parse_arguments(int argc, char *argv[], struct argument_t *arg);
void add_string(struct strings_t *data, const char *str);
void add_string_len(struct strings_t *data, const char *str, size_t len);
void add_string_copy(struct strings_t *data, const struct strings_t *prev);
void shrink_string(char **str, size_t len);
int check_usage(int argc, char *argv[]);
void print_usage(const char *progname);
void print_version(void);
void out_of_memory(const char *file, int line);
void free_ruledata(const struct ruledata_t *ruledata);
void free_rulecdat(const struct rules_t *rules);
void free_onerule(const struct ruledata_t *ruledata, size_t which);
void free_strings(const struct strings_t *strings);
void free_dup_onerule(struct onerule_t *rule, const struct onerule_t *prev);
void free_rulevars(const struct rulevars_t *rulevars);
void free_onevar(const struct onevar_t *var);
void free_argument(const struct argument_t *argument);
void free_prevrule(struct prevrule_t *pr);

FILE *open_file(const char *fn, const char *mode);
size_t get_string(char **line, size_t len, size_t *alen, FILE *fp);
void load_rules(struct rules_t *rules, const struct strings_t *rulefn);
void add_ilrules(struct rules_t *rules, const struct strings_t *ilrule);
void add_rules_file_dir(struct rules_t *rules, const char *fn,
    const char *dir);
void add_rules_file(struct rules_t *rules, const char *fn);
void add_rule_var(struct rules_t *rules, char *str, const char *file,
    int line);
void add_var(struct rulevars_t *vars, char *str, char *eq);
int find_var_pos(struct rulevars_t *vars, const char *p, size_t *pos);
int add_rule_type(struct rules_t *rules, enum type_t *t, char **str,
    const char *file, int line);
int add_rule_pos(struct rules_t *rules, size_t rsort, struct onerule_t *one);
struct onerule_t *rule_new(void);
void add_rule_new(struct ruledata_t *rd);
void add_rule(struct rules_t *rules, char *str, const char *file, int line);
int add_allocated_rule(struct rules_t *rules, struct onerule_t **cdat,
    size_t sort);
void add_parts(struct strings_t *data, const struct strings_t *prev,
    const char *str);
int check_parts(int rparts, int xparts, const char *str);
void process_escapes(struct strings_t *data);
void remove_escapes(char **str, size_t *len);
void process_vars(const struct rulevars_t *vars, struct strings_t *data);
void replace_var_str(const struct rulevars_t *vars, char **str);
void resize_var_string(char **str, char **pos, size_t len);
void find_var_replace(const struct rulevars_t *vars, char **str, char **p,
    char *end);
int replace_onevar(const struct onevar_t *var, char **str, char **pos,
    char *end);
void replace_novar(size_t cp, char **str, char **pos);
size_t get_type(struct rules_t *rules, const char *str, enum type_t *type);

void parse_files(struct rules_t *rules, struct strings_t *inputfn,
    struct strings_t *outputfn);
void read_file(struct rules_t *rules, const char *fn, FILE *out);
void parse_line(struct rules_t *rules, char *line, FILE *out,
    struct typefunc_t *tf);
int call_one_type(struct rules_t *rules, struct typefunc_t *tf);
int call_type_cdat(struct rules_t *rules, struct typefunc_t *tf);
int call_type_funcs(struct rules_t *rules, struct typefunc_t *tf);
void set_rules_prev(struct rules_t *rules);
void set_follow_prev(struct rules_t *rules, size_t start);
int find_rule_match(const struct rules_t *rules, const char *p, size_t *pos);
int find_rule_new(const struct rules_t *rules, const char *p, size_t *pos);
int find_rule_prev(const struct rules_t *rules, const char *p, size_t *pos);
void copy_file(FILE *from, const char *fn);

int is_number(const char *p);
int is_fpnumber(const char *p);
void print_number(char **p, FILE *out);
int is_word(int c);
int is_backslashed(const char *start, const char *p);
void remove_char(char *p);
int is_word(int c);
int is_word_prev(const char *line, const char *p);
void remove_comments(char *s);
char *is_var(char *s);
int is_current_dir(const char *s);
void print_chars(char **p, size_t len, FILE *out);
void add_to_funclist(struct funclist_t *func, size_t which, enum type_t type);
void remove_from_funclist(struct funclist_t *func);
void add_ws_rulelist(struct rulelist_t *list, char c);
void print_ws_rulelist(struct rulelist_t *list, FILE *out);
void append_rulelist(const char *from, const char *to, struct rulelist_t *list,
    FILE *out);
void chomp_newline(char *str, size_t *flen);

int type_keyword(struct onerule_t *rule, struct typefunc_t *tf);
int type_midword(struct onerule_t *rule, struct typefunc_t *tf);
int type_comment_end(struct onerule_t *rule, struct typefunc_t *tf);
int type_comment(struct onerule_t *rule, struct typefunc_t *tf);
int type_string(struct onerule_t *rule, struct typefunc_t *tf);
int type_nestcom(struct onerule_t *rule, struct typefunc_t *tf);
int type_number(struct onerule_t *rule, struct typefunc_t *tf);
int type_fpnumber(struct onerule_t *rule, struct typefunc_t *tf);
int type_start(struct onerule_t *rule, struct typefunc_t *tf);
int type_end(struct onerule_t *rule, struct typefunc_t *tf);

/*------------*\
 | Functions. |
\*------------*/

/*! Program entry point. Acts as a driver function, calling other functions.
    \param argc Number of arguments passed to the program.
    \param argv Array of command-line arguments, including program name.
    \return Always returns 0, indicating success.
*/
int main(int argc, char *argv[]) {
    struct argument_t arg = {
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
    };
    struct rules_t rules = {
        {
            {"keyword", 0, 3, 1, type_keyword},
            {"midword", 0, 3, 1, type_midword},
            {"comment", 0, 3, 1, type_comment},
            {"string", 0, 3, 1, type_string},
            {"nestcom", 0, 3, 1, type_nestcom},
            {"number", (size_t)-1, 2, 0, type_number},
            {"fpnumber", (size_t)-1, 2, 0, type_fpnumber},
            {"start", (size_t)-1, 1, 0, type_start},
            {"end", (size_t)-1, 1, 0, type_end},
        },
        {0, 0},
        {0, 0, 0, 0},
        {0, 0},
        {0},
        {TYPE_ERROR, 0, 0, 0}
    };  /* The only instance of the structure \c rules_t. */

    if(check_usage(argc, argv)) return 0;

    parse_arguments(argc, argv, &arg);

    add_ilrules(&rules, &arg.ilrule);
    load_rules(&rules, &arg.rulefn);
    set_rules_prev(&rules);

    parse_files(&rules, &arg.inputfn, &arg.outputfn);

    /* Free all allocated memory. */

    free_ruledata(&rules.data);
    free_rulecdat(&rules);
    free_rulevars(&rules.vars);
    free_prevrule(&rules.pr);
    free_argument(&arg);

    return 0;
}

/*! Parses the command-line parameters passed to codeform on the command line.
    Stores the results in \a arg.
    \param argc Number of arguments passed to the program.
    \param argv Array of command-line arguments, including executable name.
    \param arg The structure to store the arguments in.
*/
void parse_arguments(int argc, char *argv[], struct argument_t *arg) {
    struct strings_t *p;
    int x;

    /* Parse command-line parameters. */
    for(x = 1; x < argc; x ++) {
        if(!strcmp(argv[x], "--")) {  /* End of arguments. */
            x ++;
            break;
        }
        else if(!strcmp(argv[x], "-e")) p = &arg->ilrule;  /* Inline rule. */
        else if(!strcmp(argv[x], "-f")) p = &arg->rulefn;  /* Rules file. */
        else if(!strcmp(argv[x], "-o")) p = &arg->outputfn;  /* Output file. */
        else break;  /* End of arguments. */

        add_string(p, argv[++x]);
    }
    
    /* Make the first non-command-line-argument argument an input file if there
        are no rules, much like sed. */
    if(x < argc && !arg->rulefn.number && !arg->ilrule.number) {
        add_string(&arg->rulefn, argv[x++]);
    }

    /* Parse remaining command-line parameters; rules or input files. */
    for( ; x < argc; x ++) {
        add_string(&arg->inputfn, argv[x]);
    }
}

/*! Adds a string to a strings_t structure. Calls add_string_len().
    \param data The structure to add the string \a str to.
    \param str The string to add to \a data.
*/
void add_string(struct strings_t *data, const char *str) {
    add_string_len(data, str, (size_t)-1);
}

/*! Adds a string of specified length to a strings_t structure.
    \param data The structure to add the string to.
    \param str The string to add to the structure, \a data. Only \a len
        characters are copied.
    \param len Number of characters to copy from the string \a str.
*/
void add_string_len(struct strings_t *data, const char *str, size_t len) {
    char **p;
    size_t slen, *tlen;

    slen = (len == (size_t)-1 ? strlen(str) : len);

    p = realloc(data->data, (data->number + 1) * sizeof(char *));

    if(!p) out_of_memory(__FILE__, __LINE__);

    data->data = p;
    data->data[data->number] = malloc(slen + 1);
    if(!data->data[data->number]) out_of_memory(__FILE__, __LINE__);

    if(len == (size_t)-1) {
        strcpy(data->data[data->number], str);
    }
    else {
        strncpy(data->data[data->number], str, len);
        data->data[data->number][len] = 0;
    }

    tlen = realloc(data->len, (data->number + 1) * sizeof(size_t));

    if(!tlen) out_of_memory(__FILE__, __LINE__);

    data->len = tlen;
    data->len[data->number] = slen;

    data->number ++;
}

/*! Adds a string to \a data, the equivalent string in \a prev. The string
    itself isn't copied, the pointer is just set to the same position in
    (dynamically-allocated) memory.
    \param data The strings_t structure to add a string to.
    \param prev The structure to get the pointer to the string from.
*/
void add_string_copy(struct strings_t *data, const struct strings_t *prev) {
    char **p;
    size_t *tlen;

    p = realloc(data->data, (data->number + 1) * sizeof(char *));

    if(!p) out_of_memory(__FILE__, __LINE__);

    data->data = p;

    data->data[data->number] = prev->data[data->number];

    tlen = realloc(data->len, (data->number + 1) * sizeof(size_t));

    if(!tlen) out_of_memory(__FILE__, __LINE__);

    data->len = tlen;
    data->len[data->number] = prev->len[data->number];

    data->number ++;
}

/*! Reallocates the memory for the string \c str to be just enough. Used when
    some characters have been removed from the string (eg by remove_escapes()).
    \param str The string to shrink to the right size.
    \param len The new length of the string.
*/
void shrink_string(char **str, size_t len) {
    char *p = realloc(*str, len + 1);

    if(!p) out_of_memory(__FILE__, __LINE__);

    *str = p;
}

/*! Returns true if any of the command-line parameters are usage, help, or
    version requests. Called before parsing the rest of the arguments.
    \param argc Number of command-line parameters.
    \param argv Array of command-line parameters.
*/
int check_usage(int argc, char *argv[]) {
    int x;
    
    for(x = 1; x < argc; x ++) {
        if(!strcmp(argv[x], "--help") || !strcmp(argv[x], "-h")
            || !strcmp(argv[x], "--usage")) {

            print_usage(argv[0]);
            return 1;
        }
        else if(!strcmp(argv[x], "--version") || !strcmp(argv[x], "-v")) {
            print_version();
            return 1;
        }
    }

    return 0;
}

/*! Prints codeform's usage to the screen.
    \param progname The executable path of codeform (argv[0] in main()).
*/
void print_usage(const char *progname) {
    fprintf(stderr, "\n" VERSION
        "\nExecutable path: %s\n"
        "\nusage: codeform [-f rule-file] [-e inline-rule] [-o output]"
        "\n                [--help] [-h] [--usage] [--version] [-v]"
        "\n                [--] [rule-file-if-no-other-rules] [files...]\n"
        "\nThe arguments are very similar to sed's (and -o is from GCC).\n"
        "\nOutput can go to multiple files -- just specify more than one -o"
        " argument. With\nno -o arguments (or if none of the output files"
        " could be opened), stdout is\nused instead.\n", progname);
    fprintf(stderr,  /* Two fprintf()s to avoid strings over 509 chars long. */
        "\nIf no rules are specified, the first argument that isn't preceded"
        " by an option\nis taken as a rules file instead of an input file."
        " With no input files, stdin\nis used.\n"
        "\nA simple usage of codeform might look like the following:"
        "\n    $ ./codeform -o codeform.htm rules/c_1_html codeform.c"
        "\nThis formats codeform.c according to the rules in rules/c_1_html,"
        " storing the\noutput in codeform.htm.\n");
}

/*! Prints the version of codeform, including time and date of compilation
    (__TIME__ and __DATE__).
*/
void print_version(void) {
    fprintf(stderr, "\n" VERSION " (" __TIME__ " " __DATE__ ")"
        "\nCopyright (C) 2007 DWK\n"
        "\nThis is free software; see the source for copying conditions. "
            " There is NO"
        "\nwarranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR"
            " PURPOSE,"
        "\nto the extent permitted by law.\n"
        "\nSee the file history that comes with the distribution for a"
        " complete history\nlist of versions of codeform.\n");
}

/*! Prints an error and exits due to an out-of-memory condition.
    \param file The file in which the condition occured (__FILE__).
    \param line The line at which the condition occured (__LINE__).
*/
void out_of_memory(const char *file, int line) {
    fprintf(stderr, "codeform: (%s:%i): Out of memory\n", file, line);
    exit(1);
}

/*! Frees the memory allocated in a ruledata_t structure. Frees data only once,
    even if two rules reference the same memory.
    \param ruledata The ruledata_t structure to free the contents of.
*/
void free_ruledata(const struct ruledata_t *ruledata) {
    size_t x;

    for(x = 0; x < ruledata->number; x ++) {
        free_onerule(ruledata, x);

        free(ruledata->data[x]);
    }

    free(ruledata->data);
}

/*! Frees the memory allocated for the cdat[] array of rules in \a rules.
    \param rules The structure containing the array of rules to free.
*/
void free_rulecdat(const struct rules_t *rules) {
    enum type_t x;
    size_t y;

    for(x = 0; x < TYPES; x ++) {
        if(rules->cdat[x]) {
            for(y = 0; y < rules->cdat[x]->data.number; y ++) {
                free(rules->cdat[x]->data.data[y]);
            }

            free(rules->cdat[x]->data.data);
            free(rules->cdat[x]->data.len);
            free(rules->cdat[x]);
        }
    }
}

/*! Frees the allocated memory for a strings_t structure contained in a
    one_rule structure. Sets any pointers in \a ruledata to the same memory to
    NULL.
    \param ruledata The structure containing structures with allocated memory.
    \param which The strings_t structure to free the contents of.
*/
void free_onerule(const struct ruledata_t *ruledata, size_t which) {
    size_t x, y;

    for(x = 0; x < ruledata->data[which]->data.number; x ++) {
        if(ruledata->data[which]->data.data[x]) {
            for(y = which + 1; y < ruledata->number; y ++) {
                if(y != which && x < ruledata->data[y]->data.number  /* !!!?*/
                    && ruledata->data[which]->data.data[x]
                    == ruledata->data[y]->data.data[x]) {

                    ruledata->data[y]->data.data[x] = 0;
                }
            }

            free(ruledata->data[which]->data.data[x]);
        }
    }

    free(ruledata->data[which]->data.data);
    free(ruledata->data[which]->data.len);
}

/*! Frees the memory allocated for a strings_t structure. All members are freed
    except for the structure itself and non-pointer members.
    \param strings The strings_t structure to free allocated memory from.
*/
void free_strings(const struct strings_t *strings) {
    size_t x;

    for(x = 0; x < strings->number; x ++) {
        free(strings->data[x]);
    }

    free(strings->data);
    free(strings->len);
}

/*! Frees the rule \a rule and its parts that are not present in \a prev.
    \param rule The structure to free, including all non-duplicate parts.
    \param prev The structure containing parts that could possibly be
        duplicates of \a rule's parts.
*/
void free_dup_onerule(struct onerule_t *rule, const struct onerule_t *prev) {
    size_t x;

    for(x = 0; x < rule->data.number; x ++) {
        if(!prev || x >= prev->data.number
            || rule->data.data[x] != prev->data.data[x]) {

            free(rule->data.data[x]);
        }
    }

    free(rule->data.data);
    free(rule->data.len);
    free(rule);
}

/*! Frees the memory allocated in a rulevars_t structure.
    \param rulevars The rulevars_t structure to free the contents of.
*/
void free_rulevars(const struct rulevars_t *rulevars) {
    size_t x;

    for(x = 0; x < rulevars->number; x ++) {
        free_onevar(rulevars->data[x]);

        free(rulevars->data[x]);
    }

    free(rulevars->data);
}

/*! Frees the memory allocated for a onevar_t structure.
    \param var The structure to free the contents of.
*/
void free_onevar(const struct onevar_t *var) {
    free(var->from);
    free(var->to);
}

/*! Frees the memory allocated while parsing command-line arguments:
    the input, output, and rules file names and any inline rules.
    \param argument The structure containing the comment-line data to free.
*/
void free_argument(const struct argument_t *argument) {
    free_strings(&argument->inputfn);
    free_strings(&argument->outputfn);
    free_strings(&argument->rulefn);
    free_strings(&argument->ilrule);
}

/*! Frees the rule pointed to in the \c prevrule_t structure by the member
    \c prev if it is the only pointer to the allocated memory.
    \param pr The \c prevrule_t structure containing a pointer to the previous
        rule parsed.
*/
void free_prevrule(struct prevrule_t *pr) {
    if(pr->freep) {
        free_dup_onerule(pr->prev, pr->one);
    }
}

/*! Opens the file \a fn in mode \a mode, returning the file pointer on success
    or NULL on error. Prints an error message on error before returning.
    \param fn The name of the file to open.
    \param mode The mode to open the file in.
    \return The newly opened file, or NULL on error.
*/
FILE *open_file(const char *fn, const char *mode) {
    FILE *fp;

    if(!strcmp(fn, "-")) {
        return strchr(mode, 'r') ? stdin : stdout;
    }

    if(!(fp = fopen(fn, mode))) {
        fprintf(stderr, "codeform: Can't open file \"%s\" for mode \"%s\"\n",
            fn, mode);
    }

    return fp;
}

/*! A dynamic-memory allocating clone of fgets(). Reads characters from fp,
    storing them in \a line starting with (*line)[len].
    \param line The resizeable string to store characters in.
    \param len The position in \a line to start storing characters at.
    \param alen The amount of memory currently allocated (multiple of BUFSIZ).
    \param fp The file to read the characters from.
    \return The new length of the string (\a len modified). It will be the same
    as \a len if no characters were read before EOF was encountered.
*/
size_t get_string(char **line, size_t len, size_t *alen, FILE *fp) {
    char *p;
    int c = 0;

    do {
        if(c == '\n') c = 0;
        else c = getc(fp);

        if(c == EOF) {
            c = 0;
        }

        if(len >= *alen) {
            if(*alen) *alen *= 2;
            else *alen = BUFSIZ;

            p = realloc(*line, *alen);

            if(!p) out_of_memory(__FILE__, __LINE__);

            *line = p;
        }

        (*line)[len++] = c;
    } while(c);

    return len - 1;
}

/*! Loads all the rules files in \a rulefn by calling add_rules_file() for each
    one. Stores the rules read into \a rules.
    \param rules The structure to store all the rules read in into.
    \param rulefn The names of rules files to parse.
*/
void load_rules(struct rules_t *rules, const struct strings_t *rulefn) {
    size_t x;

    for(x = 0; x < rulefn->number; x ++) {
        add_rules_file(rules, rulefn->data[x]);
    }
}

/*! Adds the inline rules in \c ilrule to \c rules by calling add_rule_var()
    for each rule.
    \param rules The structure to add the rules to.
    \param ilrule The inline rules to add.
*/
void add_ilrules(struct rules_t *rules, const struct strings_t *ilrule) {
    size_t x;

    for(x = 0; x < ilrule->number; x ++) {
        add_rule_var(rules, ilrule->data[x], NULL, x + 1);
    }
}

/*! Sources a rules file, prepending the location of the containing rules file
    if it does not start with "./" or ".\\".
    \param rules The rules read from rules files so far.
    \param fn The name of the rules file to read.
    \param dir The name of the containing rules file to extract the path from.
*/
void add_rules_file_dir(struct rules_t *rules, const char *fn,
    const char *dir) {

    char *last = strrchr(dir, '/'), *p = strrchr(dir, '\\');

    if(p > last) last = p;

    if(fn && !is_current_dir(fn) && last ++) {
        p = malloc(strlen(fn) + last - dir + 1);
        if(!p) out_of_memory(__FILE__, __LINE__);

        strncpy(p, dir, last - dir);
        strcpy(p + (last - dir), fn);

        add_rules_file(rules, p);

        free(p);
    }
    else add_rules_file(rules, fn);
}

/*! Reads the file \a fn, passing each rule to add_rule_var() for parsing.
    \param rules The rules that have already been parsed, to add new rules to.
    \param fn The name of the file containing the new rules.
*/
void add_rules_file(struct rules_t *rules, const char *fn) {
    FILE *fp = open_file(fn, "r");
    char *str = 0;
    int line = 0, isb;
    size_t tlen, len, flen = 0, alen = 0;

    if(!fp) return;

    do {
        len = 0;

        isb = 1;
        while(isb && (tlen = get_string(&str, len, &alen, fp)) != len) {
            flen = tlen;

            chomp_newline(str, &flen);

            isb = is_backslashed(str+len, str+flen);

            len = flen;
            line ++;

            if(len && str[len-1] == '\\') str[--len] = 0;
        }

        add_rule_var(rules, str, fn, line);
    } while(tlen != len);

    if(fp != stdin) fclose(fp);

    free(str);
}

/*! Adds a rule/variable specified by \c str into \c rules.
    \param rules The structure to add the variable/rule to.
    \param str The string representing the rule/variable.
    \param file The file that the string \c str came from.
    \param line The line of the file \c file from which \c str came.
*/
void add_rule_var(struct rules_t *rules, char *str, const char *file,
    int line) {

    char *p;

    remove_comments(str);

    if(*str == '.') {
        if(file && !strcmp(file, str+1)) {
            fprintf(stderr, "codeform: Error in rule (%s:%i):"
                " Recursive sourcing: \"%s\"\n",
                file ? file : "-e", line, str);
        }
        else {
            if(file && !strchr(str+1, '/') && !strchr(str+1, '\\')) {
                add_rules_file_dir(rules, str+1, file);
            }
            else add_rules_file(rules, str+1);
        }
    }
    else if((p = is_var(str))) add_var(&rules->vars, str, p);
    else if(*str) add_rule(rules, str, file, line);
}

/*! Adds a variable to the list of variables. The variable starts with \a str,
    and the position of the '=' is at \a eq.
    \param vars The structure of variables to store the new variable in.
    \param str The string representing the variable.
    \param eq The position in string of the '=' sign separating the name of the
        variable from the variable's value.
*/
void add_var(struct rulevars_t *vars, char *str, char *eq) {
    struct onevar_t **p;
    size_t eqlen = strlen(eq), pos;

    *eq = 0;

    if(find_var_pos(vars, str, &pos)) return;
    if(pos == (size_t)-1) pos = 0;

    p = realloc(vars->data, (vars->number + 1) * sizeof(struct onevar_t *));

    if(!p) out_of_memory(__FILE__, __LINE__);

    vars->data = p;

    memmove(vars->data + pos + 1, vars->data + pos,
        (vars->number - pos) * sizeof(struct onevar_t *));

    vars->data[pos] = malloc(sizeof(struct onevar_t));
    if(!vars->data[pos]) out_of_memory(__FILE__, __LINE__);

    vars->data[pos]->from = malloc(eq - str + 1);
    if(!vars->data[pos]->from) out_of_memory(__FILE__, __LINE__);
    /*strncpy(vars->data[pos]->from, str, eq - str);*/
    strcpy(vars->data[pos]->from, str);  /* will not overflow */
    /*vars->data[pos]->from[eq - str] = 0;*/
    vars->data[pos]->flen = eq - str;

    vars->data[pos]->to = malloc(eqlen);
    if(!vars->data[pos]->to) out_of_memory(__FILE__, __LINE__);
    strcpy(vars->data[pos]->to, eq+1);
    vars->data[pos]->tlen = eqlen - 1;

    vars->number ++;
}

/*! Binary searches through the existing variables in \a rt for the position
    that the variable \a p should be in, putting the result in \a pos.
    \param vars The existing variables to search through.
    \param p The text representing the new variable to search for.
    \param pos The variable to store the position found in. Set to (size_t)-1
        if no match was found.
    \return 1 if an exact match (to the shortest length) was found, 0
        otherwise.
*/
int find_var_pos(struct rulevars_t *vars, const char *p, size_t *pos) {
    size_t mid = (size_t)-1, first = 0, last = vars->number-1;
    int v = 0;

    *pos = (size_t)-1;

    if(!vars->number) return 0;

    while(first <= last && last != (size_t)-1) {
        mid = (first + last) / 2;

        v = strcmp(p, vars->data[mid]->from);

        if(first == last && v) break;

        if(v < 0) last = mid-1;
        else if(v > 0) first = mid+1;
        else {
            *pos = mid;
            first = mid+1;
        }
    }

    if(*pos != (size_t)-1) return 1;

    if(v < 0) *pos = mid;
    else *pos = mid+1;

    return 0;
}

/*! Called by add_rule() to determine the type of the rule \a str.
    \param rules The array of rules that have been parsed so far.
    \param t The pointer to store the type of the current rule.
    \param str The rule itself.
    \param file The file which the rule \a str came from.
    \param line The line of the file from which the rule came.
    \return 0 if no errors occured and str is an ordinary rule; nonzero if the
        rule is a header, an invalid header, or not under a header. If \a str
        is an inline header, positions \a str past the header and returns 0 so
        that \a str may be treated as an ordinary rule.
*/
int add_rule_type(struct rules_t *rules, enum type_t *t, char **str,
    const char *file, int line) {

    size_t v = get_type(rules, *str, t);

    if(!v) {
        if(rules->pr.type == TYPE_ERROR) {
            fprintf(stderr, "codeform: Error in rule (%s:%i):"
                " Rule not under a header: \"%s\"\n",
                file ? file : "-e", line, *str);
            return 1;
        }
        else {
            *t = rules->pr.type;
        }
    }
    else if(v == (size_t)-1) {
        rules->pr.type = TYPE_ERROR;
        fprintf(stderr, "codeform: Error in rule (%s:%i):"
            " Invalid header: \"%s\"\n",
            file ? file : "-e", line, *str);
        return 1;
    }
    else {
        rules->pr.type = *t;

        if(v != 1) {
            *str += v;
        }
        else return 1;
    }

    return 0;
}

/*! Adds the rule \a one to the correct place, keeping the list of rules
    sorted.
    \param rules The array of rules collected so far.
    \param rsort The index to sort this rule by.
    \param one The rule to add to \a rd.
    \return Same as find_rule_new(): 1 if an exact match was found, 0
        otherwise. 2 if \a one is already an existing rule.
*/
int add_rule_pos(struct rules_t *rules, size_t rsort, struct onerule_t *one) {
    size_t sort;
    int r;

    r = find_rule_new(rules, one->data.data[rsort], &sort);

    if(r == 2) return 2;  /* \a one is already an existing rule. */

    add_rule_new(&rules->data);

    if(r) {
        if(sort+1 == rules->data.number) {
            rules->data.data[rules->data.number] = one;
        }
        else {
            memmove(rules->data.data+sort+2, rules->data.data+sort+1,
                (rules->data.number - sort - 1) * sizeof(struct onerule_t *));
            rules->data.data[sort+1] = one;
        }
    }
    else {
        if(sort == (size_t)-1) {
            rules->data.data[rules->data.number] = one;
        }
        else {
            memmove(rules->data.data+sort+1, rules->data.data+sort,
                (rules->data.number - sort) * sizeof(struct onerule_t *));
            rules->data.data[sort] = one;
        }
    }

    return r;
}

/*! Allocates a new rule and sets all of its fields to an appropriate value.
    \return The newly allocated and initialized rule.
*/
struct onerule_t *rule_new(void) {
    struct onerule_t *one = malloc(sizeof(struct onerule_t));
    if(!one) out_of_memory(__FILE__, __LINE__);

    one->prev = (size_t)-1;
    one->data.data = 0;
    one->data.len = 0;
    one->data.number = 0;

    return one;
}

/*! Increments the size of the array of structures in \a rd to accommodate a
    new rule.
    \param rd The current array of rules.
    \return The newly allocated rule.
*/
void add_rule_new(struct ruledata_t *rd) {
    struct onerule_t **p;

    p = realloc(rd->data, (rd->number + 1) * sizeof(struct onerule_t *));

    if(!p) out_of_memory(__FILE__, __LINE__);

    rd->data = p;
}

/*! Adds the rule \a str to the structure \a rules containing the array of
    rules. In doing so, it separates the parts of the rule, extracts variables
    and processes escapes; and does a search on the existing rules to find the
    position of the current one. If the rule is a header it is handled, too.
    \param rules The structure containing the rules parsed so far, to add the
        latest rule \a str to.
    \param str The string representing the rule to add to \a rules.
    \param file The file that the rule \a str came from.
    \param line The line of the file that the rule \a str came from.
*/
void add_rule(struct rules_t *rules, char *str, const char *file, int line) {
    enum type_t t;
    int r;

    if(add_rule_type(rules, &t, &str, file, line)) return;

    if(!check_parts(rules->type[t].parts, rules->type[t].xparts, str)) {
        fprintf(stderr, "codeform: Error in rule (%s:%i):"
            " Incorrect number of parts (must be %i-%i): \"%s\"\n",
            file ? file : "-e", line, rules->type[t].parts,
            rules->type[t].parts + rules->type[t].xparts, str);
        return;
    }

    rules->pr.one = rule_new();

    rules->pr.one->type = t;

    add_parts(&rules->pr.one->data,
        rules->data.number ? &rules->pr.prev->data : 0, str);
    process_vars(&rules->vars, &rules->pr.one->data);
    process_escapes(&rules->pr.one->data);

    rules->pr.type = t;

    r = add_allocated_rule(rules, &rules->cdat[t], rules->type[t].sort);

    free_prevrule(&rules->pr);

    rules->pr.prev = rules->pr.one;
    rules->pr.one = 0;

    rules->pr.freep = r;
}

/*! Add a pre-allocated rule to the list of rules, setting \a prev to \a one.
    \param rules The array of existing rules.
    \param cdat The pointer to the cdat structure for \a one's type.
    \param sort The element to sort \a one by.
    \return 1 if everything is taken care of and add_rule() may return. !!!
*/
int add_allocated_rule(struct rules_t *rules, struct onerule_t **cdat,
    size_t sort) {

    if(sort == (size_t)-1) {
        if(!*cdat) *cdat = rules->pr.one;
        else return 1;
    }
    else {
        if(add_rule_pos(rules, sort, rules->pr.one) == 2) {
            return 1;
        }

        rules->data.number ++;
    }

    return 0;
}

/*! Separates the parts of the rule \a str and adds them to \a data, which is a
    structure containing an array of strings. If a part is "*", it is assigned
    the value of the same section of the previous rule (determined from
    \a prev).
    \param data The structure to add the parts of the rule \a str to.
    \param prev The structure containing the parts of the previous rule (or
        NULL if this is the first rule).
    \param str The string representing the current rule, with all its parts.
*/
void add_parts(struct strings_t *data, const struct strings_t *prev,
    const char *str) {

    const char *p = str;

    do {
        p = strchr(p, ':');

        if(!p) {  /* No more ':'s */
            if(prev && *str == '*' && !str[1] && data->number < prev->number) {
                add_string_copy(data, prev);
            }
            else add_string(data, str);
        }
        else {
            if(!is_backslashed(str, p)) {
                if(prev && *str == '*' && str[1] == ':') {
                    add_string_copy(data, prev);
                }
                else add_string_len(data, str, p-str);

                str = p+1;
            }

            p ++;
        }
    } while(p);
}

/*! Checks to see if the number of parts in the rule \a str has at least
    \a rparts (required parts) with an optional up to \a xparts extra parts.
    \param rparts Minimum number of required parts for the rule \a str to have.
    \param xparts Optional number of extra parts in addition to \a rparts.
    \param str The string to check the parts of.
    \return True if the number of parts is in the range rparts to
        rparts+xparts.
*/
int check_parts(int rparts, int xparts, const char *str) {
    const char *s = str, *p;
    int parts = 0;

    do {
        p = strchr(s, ':');
        if(!p && *s) {
            parts ++;
        }
        else {
            if(!is_backslashed(str, p)) {
                parts ++;
                str = s = p+1;
            }
            else s ++;
        }
    } while(p);

    return parts >= rparts && parts <= rparts+xparts;
}

/*! Calls remove_escapes() for every string in the strings structure \a data.
    \param data The structure containing strings to remove escapes from.
*/
void process_escapes(struct strings_t *data) {
    size_t x;

    for(x = 0; x < data->number; x ++) {
        remove_escapes(&data->data[x], &data->len[x]);
    }
}

/*! Removes characters in \a str, replacing them with what they represent. "\n"
    turns into a newline. (To do this, a character must be removed, for "\n" is
    two characters and a newline one; remove_char() is called to this end.)
    \param str The string to remove escaped characters from.
    \param len The length of the string, decremented for each escape sequence
        replaced.
*/
void remove_escapes(char **str, size_t *len) {
    char *p;
    size_t olen = *len;

    for(p = *str; (p = strchr(p, '\\')); p ++) {
        remove_char(p);

        switch(*p) {
        case 'n':
            *p = '\n';
            break;
        case 't':
            *p = '\t';
            break;
        default: break;
        }

        (*len) --;
    }

    if(*len != olen) shrink_string(str, *len);
}

/*! Calls replace_var_str() for every string in \a data.
    \param vars A structure containing an array of variables.
    \param data The data to apply the variables to.
*/
void process_vars(const struct rulevars_t *vars, struct strings_t *data) {
    size_t x;

    for(x = 0; x < data->number; x ++) {
        replace_var_str(vars, &data->data[x]);
        data->len[x] = strlen(data->data[x]);  /* !!! can be determined */
    }
}

/*! For every '$' sign in \a str, calls replace_onevar() with every variable,
    looking for a match.
    \param vars The structure containing the array of variables.
    \param str The string to replace occurences of $variable_name with the
        value of the variable variable_name.
*/
void replace_var_str(const struct rulevars_t *vars, char **str) {
    char *p, *end;

    for(p = *str; (p = strchr(p, '$')); p ++) {
        if(!is_backslashed(*str, p) && p[1] == '('
            && (end = strchr(p+2, ')'))) {

            find_var_replace(vars, str, &p, end);
        }
    }
}

/*! Resizes the string str to len characters in size. If the memory block was
    moved, points pos to the same position relative to the new memory block as
    it was relative to str.
    \param str The string to resize to \a len characters long.
    \param pos The position to move if str changes.
    \param len The new size of the string.
*/
void resize_var_string(char **str, char **pos, size_t len) {
    char *p = realloc(*str, len);

    if(!p) {
        out_of_memory(__FILE__, __LINE__);
    }

    *pos = *pos-*str + p;
    *str = p;
}

/*! Binary searches through the existing variables in \a rt for the variable
    matching the string \a str, calling the function to replace the variable
    with the text for that variable.
    \param vars The existing variables to search through.
    \param str The string containing the variable to search for.
    \param p The start of the variable to look for (position of the '$').
    \param end The end of the variable to look for (position of the ')').
*/
void find_var_replace(const struct rulevars_t *vars, char **str, char **p,
    char *end) {

    size_t mid = (size_t)-1, first = 0, last = vars->number-1;
    size_t pos = (size_t)-1;
    int v = 0;

    pos = (size_t)-1;

    if(!vars->number) return;

    while(first <= last && last != (size_t)-1) {
        mid = (first + last) / 2;

        v = replace_onevar(vars->data[mid], str, p, end);

        if(v < 0) last = mid-1;
        else if(v > 0) first = mid+1;
        else {
            pos = mid;
            first = mid+1;

            *p = *str-1;
            break;
        }
    }

    if(pos == (size_t)-1) {
        replace_novar(end-*p-2, str, p);
        (*p) --;
    }
    else {
        if(v < 0) pos = mid;
        else pos = mid+1;
    }
}

/*! Replaces the position \a pos in the string \a str (if it matches a '$' and
    the variable's name) with the text the variable is replaced with.
    \param var A structure containing the variable's name and text to replace
        with.
    \param str The string containing the variable.
    \param pos The start of the string to do the replacing in (dynamically
        reallocated to the new size).
    \param end The position in \a str of the '$' in a variable name.
    \return Nonzero if a text substitution was made.
*/
int replace_onevar(const struct onevar_t *var, char **str, char **pos,
    char *end) {

    size_t olen, plen;
    int v;

    if(!(v = strncmp(*pos+2, var->from, end - *pos - 2))) {
        if(var->from[end - *pos - 2]) return -1;

        olen = strlen(*str);
        plen = olen + *str - *pos;

        if(var->flen+2 < var->tlen) {
            resize_var_string(str, pos, olen - var->flen + var->tlen - 2);

            memmove(*pos + var->tlen, *pos + var->flen+3, plen - var->flen-2);
        }
        else {
            memmove(*pos + var->tlen, *pos + var->flen+3, plen - var->flen-2);

            resize_var_string(str, pos, olen - var->flen + var->tlen - 2);
        }

        memmove(*pos, var->to, var->tlen);
    }

    return v;
}

/*! Replaces the variable at \a pos in the string \a str with a null value, "".
    \param cp The length of the variable.
    \param str The start of the string to do the replacing in (dynamically
        reallocated to the new size).
    \param pos The position in \a str of the variable.
*/
void replace_novar(size_t cp, char **str, char **pos) {
    size_t olen, plen;

    olen = strlen(*str);
    plen = olen + *str - *pos;

    memmove(*pos, *pos + cp+3, plen - cp-2);

    resize_var_string(str, pos, olen - cp-2);
}

/*! Returns the number of the header that the rule \a str represents.
    \param rules The rules structure that holds the types.
    \param str The string representing the rule that may be a header.
    \param type A pointer to set to the type of the header.
    \return Zero if \a str is not a header; (size_t)-1 if it is a nonexistent
        header; 1 if it is a header; and the position of the end of the header
        (>= 2) if it is an inline header.
*/
size_t get_type(struct rules_t *rules, const char *str, enum type_t *type) {
    enum type_t x;
    const char *p;
    size_t len = (size_t)-1;

    if(*str != '=') return 0;
    if((p = strchr(str, ':'))) len = p-str-1;

    for(x = 0; x < TYPES; x ++) {
        if(!strncmp(rules->type[x].name, str+1, len)) {
            *type = x;
            return (p ? len+2 : 1);
        }
    }

    return (size_t)-1;
}

/*! Opens all the input files and pass them on to read_file(). Copies the other
    output files from the first output file with copy_file().
    \param rules The structure containing all the rules.
    \param inputfn The strings_t structure with all the input file names.
    \param outputfn The files to write to. All the data is written to the first
        one, and that file is then copied to all the others.
*/
void parse_files(struct rules_t *rules, struct strings_t *inputfn,
    struct strings_t *outputfn) {

    FILE *out = 0;
    size_t x, y;

    for(x = 0; x < outputfn->number
        && !(out = open_file(outputfn->data[x], "w+")); x ++);

    if(!out) {
        if(x) fprintf(stderr, "codeform: No output files could be opened,"
            " using stdout\n");
        out = stdout;
    }

    for(y = 0; y < inputfn->number; y ++) {
        read_file(rules, inputfn->data[y], out);
    }

    if(!inputfn->number) read_file(rules, "-", out);

    for(y = 1; y < outputfn->number; y ++) {
        copy_file(out, outputfn->data[y]);
    }

    if(out != stdout) fclose(out);
}

/*! Reads from the file \a fn, passing each line read to parse_line().
    Determines when the end or the start of the file \a fn has been reached,
    passing this information on to parse_line().
    \param rules Used only to pass on to parse_line().
    \param fn The filename to read from.
    \param out Used only to pass on to parse_line().
*/
void read_file(struct rules_t *rules, const char *fn, FILE *out) {
    FILE *in = open_file(fn, "r");
    char *line = malloc(BUFSIZ);
    struct typefunc_t tf;
    size_t len, alen = BUFSIZ;
    int quit = 0;

    if(!in) return;

    if(!line) out_of_memory(__FILE__, __LINE__);
    *line = 0;

    tf.func.which = 0;
    tf.func.type = 0;
    tf.func.number = 0;
    tf.func.nest = 1;
    tf.pos = POS_START;
    tf.list = &rules->list;

    tf.out = out;
    parse_line(rules, line, out, &tf);

    do {
        len = 0;

        if(!get_string(&line, 0, &alen, in)) {
            tf.pos |= POS_END;
            quit = 1;
        }

        tf.out = out;
        parse_line(rules, line, out, &tf);

        tf.pos = 0;
    } while(!quit);

    if(in != stdin) fclose(in);

    free(line);
}

/*! Parses a line from an input file, calling type_funcs for every character.
    If one call is successful, calls each type_func again.
    \param rules The structure containing all the rules for parsing.
    \param line The line from the input file to parse.
    \param out The output file stream to write the results to.
    \param tf The structure that is passed to all the type_func functions,
        along with the rule_t structure appropriate for it.
*/
void parse_line(struct rules_t *rules, char *line, FILE *out,
    struct typefunc_t *tf) {

    char *p = line;
    int iw = 0, redo;

    tf->p = &p;

    do {
        do {
            redo = 0;

            tf->iw = iw;
            tf->n = p - line;

            if(tf->func.number) {
                if(call_one_type(rules, tf) || call_type_cdat(rules, tf)) {
                    redo = 1;
                }
            }

            if(!redo && call_type_funcs(rules, tf)) redo = 1;

            if(!redo) {
                if(*p) {
                    if(!isspace(*p)) {
                        append_rulelist(0, 0, &rules->list, out);
                        putc(*p, out);
                    }
                    else if(rules->list.to) {
                        add_ws_rulelist(&rules->list, *p);
                    }
                    else putc(*p, out);
                }

                iw = is_word(*p);
            }
            else iw = is_word_prev(line, p);
        } while(redo);
    } while(*p++);

    append_rulelist(0, 0, &rules->list, out);
}

/*! Calls the type_func function that may end the latest comment.
    \param rules The structure containing the rules and types.
    \param tf The structure that is passed to the type_func function.
    \return Nonzero if the type_func function ended the comment.
*/
int call_one_type(struct rules_t *rules, struct typefunc_t *tf) {
    int r;

    tf->number = tf->func.which[tf->func.number-1];
    tf->type = tf->func.type[tf->func.number-1];

    r = (*rules->type[tf->type].func)
        (rules->data.data[tf->number], tf);

    if(r < 0) remove_from_funclist(&tf->func);

    return r;
}

/*! Calls the type_func functions that have a sort of \c (size_t)-1, which have
    only one rule, stored in the cdat[] array in \a rules.
    \param rules The structure containing the cdat array and sorts for the
        type_func functions.
    \param tf The structure that is passed to the type_func functions.
    \return True if one of the functions called returned true.
*/
int call_type_cdat(struct rules_t *rules, struct typefunc_t *tf) {
    enum type_t x;

    for(x = 0; x < TYPES; x ++) {
        if(rules->type[x].sort == (size_t)-1 && rules->cdat[x]) {
            tf->number = (size_t)-1;
            tf->type = x;

            if((*rules->type[x].func)(rules->cdat[x], tf)) {
                return 1;
            }
        }
    }

    return 0;
}

/*! Calls the appropriate type_func function for a rule if a match to the
    current position in the line was found.
    \param rules The structure containing the rules to search through.
    \param tf The structure passed to the type_func functions.
    \return True if a function was called.
*/
int call_type_funcs(struct rules_t *rules, struct typefunc_t *tf) {
    size_t match;

    if(call_type_cdat(rules, tf)) return 1;

    if(find_rule_match(rules, *tf->p, &match)) {
        tf->number = match;
        tf->type = rules->data.data[match]->type;

        if((*rules->type[tf->type].func)(rules->data.data[match], tf)) {
            return 1;
        }
    }

    return 0;
}

/*! Sets the prev members of every rule in \a rules, calling set_follow_prev()
    for each rule.
    \param rules The structure containing the rules to set the prev member of.
*/
void set_rules_prev(struct rules_t *rules) {
    size_t x, y;

    for(x = 0; x < TYPES; x ++) {
        for(y = 0; y < rules->data.number; y ++) {
            if(rules->type[x].sort != (size_t)-1) {
                set_follow_prev(rules, y);
            }
        }
    }
}

/*! Sets the prev members of the structures following the rule \a start to
    \a start if their first characters (the length of the rule \a start) match
    the characters in the rule \a start.
    \param rules The structure containing the rules and sorts.
    \param start The element in the array of rules to start at (and set prevs
        to).
*/
void set_follow_prev(struct rules_t *rules, size_t start) {
    size_t x;

    for(x = start+1; x < rules->data.number; x ++) {
        if(strncmp(rules->data.data[start]->data
                .data[rules->type[rules->data.data[start]->type].sort],
            rules->data.data[x]->data
                .data[rules->type[rules->data.data[x]->type].sort],
            rules->data.data[start]->data
                .len[rules->type[rules->data.data[start]->type].sort])) {

            break;
        }

        rules->data.data[x]->prev = start;
    }
}

/*! Executes a binary search for \a p in the existing rules in \a rt. The
    longest match is assigned to \a pos.
    \param rules The array of existing rules to search through.
    \param p The string to search through the rules for.
    \param pos The variable to set to the longest match found, if any; if none,
        set to (size_t)-1.
    \return 1 if an exact match was found, 0 otherwise.
*/
int find_rule_match(const struct rules_t *rules, const char *p, size_t *pos) {
    if(find_rule_new(rules, p, pos)) return 1;

    if(*pos < rules->data.number) return find_rule_prev(rules, p, pos);

    return 0;
}

/*! Binary searches through the existing rules in \a rt for the position that
    the rule \a p should be in, putting the result in \a pos.
    \param rules The structure containing the array of existing rules.
    \param p The new rule to search for.
    \param pos The variable to store the position found in. Set to (size_t)-1
        if no match was found.
    \return 1 if an exact match (to the shortest length) was found, 0
        otherwise. 2 if an exact match the whole way was found.
*/
int find_rule_new(const struct rules_t *rules, const char *p, size_t *pos) {
    size_t mid = (size_t)-1, first = 0, last = rules->data.number-1, sort;
    int v = 0;

    *pos = (size_t)-1;

    if(!rules->data.number) return 0;

    while(first <= last && last != (size_t)-1) {
        mid = (first + last) / 2;
        sort = rules->type[rules->data.data[mid]->type].sort;

        v = strncmp(p, rules->data.data[mid]->data.data[sort],
            rules->data.data[mid]->data.len[sort]);

        if(first == last && v) break;

        if(v < 0) last = mid-1;
        else if(v > 0) first = mid+1;
        else {
            *pos = mid;

            if(!p[rules->data.data[mid]->data.len[sort]]) return 2;

            first = mid+1;
        }
    }

    if(*pos != (size_t)-1) return 1;

    if(v < 0) *pos = mid;
    else *pos = mid+1;

    return 0;
}

/*! Follows the prev members of rules until a match with \a p is found.
    \param rules The array of rules.
    \param p The string to match with a rule.
    \param pos The closest match found so far.
    \return 1 if a match was found, 0 if not.
*/
int find_rule_prev(const struct rules_t *rules, const char *p, size_t *pos) {
    size_t n = *pos-1, sort;

    if(*pos) {
        while(rules->data.data[n]->prev != (size_t)-1) {
            n = rules->data.data[n]->prev;
            sort = rules->type[rules->data.data[n]->type].sort;

            if(!strncmp(p, rules->data.data[n]->data.data[sort],
                rules->data.data[n]->data.len[sort])) {

                *pos = n;
                return 1;
            }
        }
    }

    return 0;
}

/*! Copies the open file \a from to the filename \a fn. Rewinds the file
    pointer \a from beforehand. \a fn is opened via open_file().
    \param from The open file stream used as a source, to copy from.
    \param fn A string containing the filename for the destination, to copy to.
*/
void copy_file(FILE *from, const char *fn) {
    FILE *to = open_file(fn, "w");
    int c;

    if(!to) return;

    rewind(from);  /* Rewind the file pointer to the beginning. */

    while((c = getc(from)) != EOF) {
        putc(c, to);
    }

    if(to != stdout) fclose(to);
}

/*! Returns true if \a p is a string representation of an integral number. A
    number is one or more digits followed by a character that is not a period.
    Thus floating point numbers are excluded.
    \param p The string that may contain a number.
    \return True if the string \a p represents an integral number.
*/
int is_number(const char *p) {
    int n = 0;

    while(isdigit(*p)) p ++, n = 1;

    return n && *p != '.';
}

/*! Returns true if \a p is a string representation of a floating-point number.
    A floating-point number is one or more optional digits plus a period plus
    one or more optional digits. (At least one digit must be present.)
    \param p The string to examine for a floating-point number.
    \return True if the string \a p represents a floating-point number.
*/
int is_fpnumber(const char *p) {
    int n = 0;  /* True if there was a digit before the period. */

    while(isdigit(*p)) p ++, n = 1;

    return *p == '.' && (n || isdigit(p[1]));
}

/*! Prints any digits, periods and underscores starting from the beginning of
    the string \a p to \a out. It stops printing characters when one that
    doesn't meet the criteria is found. \a p is incremented to that character.
    \param p The string to read characters from and to advance.
    \param out The output stream to write the characters to.
*/
void print_number(char **p, FILE *out) {
    while(isalnum(**p) || **p == '.' || **p == '_') {
        fputc(*(*p)++, out);
    }
}

/*! Returns true if the string \a p is escaped. A character is escaped if it
    has an odd number of backslashes preceding it. \a start indicates the start
    of the string so that the function can calculate this without going before
    the beginning of the string.
    \param start The start of the string.
    \param p The position in \a start that the function counts backslashes
        preceding.
    \return True if there are an odd number of backslashes preceding the
        position \a p in the string \a start.
*/
int is_backslashed(const char *start, const char *p) {
    size_t x = 0;

    while(p > start && *(--p) == '\\') x ++;

    return x % 2;
}

/*! Shifts the string \a p to the left, deleting the first character (*p).
    \param p The string to delete the first character from.
*/
void remove_char(char *p) {
    for( ; *p; p++) *p = *(p+1);
}

/*! Returns true if the character \a c is a letter or an underscore (a "word"
    character).
    \param c The character to examine.
    \return True if the character \a c is a word character: a letter or an
        underscore.
*/
int is_word(int c) {
    return c == '_' || isalnum(c);
}

/*! Returns true if the character before \a p is a word character (by calling
    is_word()); if p is the start of the string (equal to \a line), returns 0.
    \param line The start of the string.
    \param p The position in \a line to check the character before.
    \return True if the character before \a p is a word character; false if p
        is the start of the string.
*/
int is_word_prev(const char *line, const char *p) {
    return p == line ? 0 : is_word(*(p-1));
}

/*! Removes shell-style (# to EOL) comments from the string \a s. Called to
    strip comments from rules.
    \param s The string to remove comments from.
*/
void remove_comments(char *s) {
    char *p;

    for(p = s; (p = strchr(p, '#')); p ++) {
        if(!is_backslashed(s, p)) {
            /*while(p > s && isspace(*(p-1))) p --;*/
            *p = 0;

            break;
        }
    }
}

/*! Returns the position of the separator for a variable, or NULL if the passed
    string is not a valid variable string.
    \param s The string that contains the possible variable. It consists of
        alphanumeric characters followed by an \c = followed by any other
        characters.
*/
char *is_var(char *s) {
    char *p = s;

    while(*p++) {
        if(!is_word(*p) && !isdigit(*p)) {
            if(*p == '=') {
                return is_backslashed(s, p) ? 0 : p;
            }
            else return 0;
        }
    }

    return 0;
}

/*! Returns true if s represents a filename which starts with "./" or ".\\".
    \param s The filename to look at.
    \return True if \a s starts with the current directory, "./" or ".\\".
*/
int is_current_dir(const char *s) {
    return *s == '.' && (s[1] == '/' || s[1] == '\\');
}

/*! Prints \a len chars from \a *p to out. \a p is advanced to the next
    character (*p+len).
    \param p The string to advance and print.
    \param len The number of characters from \a p to print/advance.
    \param out The output file stream to write the characters to.
*/
void print_chars(char **p, size_t len, FILE *out) {
    while(len --) fputc(*(*p)++, out);
}

/*! Adds a function to the list of type_func functions.
    \param func The function list to add \a n to.
    \param which The function number to add to \a func.
    \param type The type of the function being added to the list.
*/
void add_to_funclist(struct funclist_t *func, size_t which, enum type_t type) {
    size_t *f = realloc(func->which, (func->number + 1) * sizeof(size_t));
    enum type_t *t;

    if(!f) out_of_memory(__FILE__, __LINE__);
    func->which = f;

    t = realloc(func->type, (func->number + 1) * sizeof(enum type_t));
    if(!t) out_of_memory(__FILE__, __LINE__);
    func->type = t;

    func->which[func->number] = which;
    func->type[func->number] = type;

    func->number ++;
}

/*! Removes the most recent function (on the bottom) from the function list
    \a func. (The function list stores the offset of the rule within the rules
    structure, not a function pointer.)
    \param func The function list to remove a function from.
*/
void remove_from_funclist(struct funclist_t *func) {
    size_t *f;
    enum type_t *t;

    if(func->number > 1) {
        func->number --;

        f = realloc(func->which, func->number * sizeof(size_t));
        if(!f) out_of_memory(__FILE__, __LINE__);
        func->which = f;

        t = realloc(func->type, func->number * sizeof(enum type_t));
        if(!t) out_of_memory(__FILE__, __LINE__);
        func->type = t;
    }
    else {
        free(func->which);
        free(func->type);
        func->which = 0;
        func->type = 0;
        func->number = 0;
    }

    func->nest = 1;
}

/*! Adds a whitespace character to the list of whitespace characters.
    \param list The structure containing the list of whitespace characters.
    \param c The character to add.
*/
void add_ws_rulelist(struct rulelist_t *list, char c) {
    char *p;

    p = realloc(list->ws, list->wslen + 2);
    if(!p) out_of_memory(__FILE__, __LINE__);
    list->ws = p;

    list->ws[list->wslen ++] = c;
    list->ws[list->wslen] = 0;
}

/*! Prints the whitespace in the whitespace list (if any), clearing the list.
    \param list The structure containing the list of whitespace characters.
    \param out The file stream to print the characters to.
*/
void print_ws_rulelist(struct rulelist_t *list, FILE *out) {
    if(list->ws) {
        fputs(list->ws, out);

        free(list->ws);
        list->ws = 0;
        list->wslen = 0;
    }
}

/*! Starts a tag, ignoring tags of the same kind in sequence. That is, if two
    keywords with the same precede-with occur in a row, don't close the tag
    only to start it again.
    \param from The text to print before (now) if the previous tag wasn't the
        same as it.
    \param to The text to print after if \a from doesn't match list->from.
    \param list The structure containing the previous tag.
    \param out The output file stream to write everything to.
*/
void append_rulelist(const char *from, const char *to, struct rulelist_t *list,
    FILE *out) {

    if(!from) {
        if(list->to) {
            fputs(list->to, out);
            list->to = 0;
            list->from = 0;

            print_ws_rulelist(list, out);
        }
    }
    else if(!list->from || strcmp(from, list->from)) {
        if(list->to) fputs(list->to, out);

        print_ws_rulelist(list, out);

        fputs(from, out);
        list->from = from;
        list->to = to;
    }
    else print_ws_rulelist(list, out);
}

/*! Removes the newline characters, if any, off of the end of \a str. \a flen
    is adjusted accordingly. (Newline characters are '\\n' and '\\r', to
    support CRLF newlines under UNIX systems.)
    \param str The string to remove newline characters from the end of.
    \param flen The length of the string \a str, decreased with each character
        removed.
*/
void chomp_newline(char *str, size_t *flen) {
    while(*flen && (str[*flen-1] == '\n' || str[*flen-1] == '\r')) {
        str[--(*flen)] = 0;
    }
}

/*--------------------------------------------*\
 | type_func functions which implement rules. |
\*--------------------------------------------*/

int type_keyword(struct onerule_t *rule, struct typefunc_t *tf) {
    if(!tf->iw && !is_word((*tf->p)[rule->data.len[0]])) {
        return type_midword(rule, tf);
    }

    return 0;
}

int type_midword(struct onerule_t *rule, struct typefunc_t *tf) {
    size_t x;

    if(!strncmp(rule->data.data[0], *tf->p, rule->data.len[0])) {
        if(!tf->func.number) {
            append_rulelist(rule->data.data[1],
                rule->data.data[rule->data.number-1], tf->list, tf->out);
        }

        if(rule->data.number == 3) {
            print_chars(tf->p, rule->data.len[0], tf->out);
        }
        else {
            for(x = 0; x < rule->data.len[0]; x ++) {
                (*tf->p)++;
            }

            fputs(rule->data.data[2], tf->out);
        }

        return 1;
    }

    return 0;
}

int type_comment_end(struct onerule_t *rule, struct typefunc_t *tf) {
    if(rule->data.number == 3) {
        if(tf->type == TYPE_COMMENT || tf->type == TYPE_NESTCOM) {
            if((**tf->p == '\n' || **tf->p == '\r')
                && !is_backslashed(*tf->p - tf->n, *tf->p)) {

                fputs(rule->data.data[2], tf->out);

                return -1;
            }
        }
        else if(!strncmp(rule->data.data[0], *tf->p, rule->data.len[0])) {
            print_chars(tf->p, rule->data.len[0], tf->out);

            if(tf->func.number == 1) fputs(rule->data.data[2], tf->out);

            return -1;
        }
    }
    else if(!strncmp(rule->data.data[1], *tf->p, rule->data.len[1])) {
        print_chars(tf->p, rule->data.len[1], tf->out);

        if(tf->type != TYPE_STRING || tf->func.number == 1) {
            fputs(rule->data.data[3], tf->out);
        }

        return -1;
    }

    if(tf->pos & POS_END) {
        if(tf->type != TYPE_STRING || tf->func.number == 1) {
            fputs(rule->data.data[rule->data.number-1], tf->out);
        }

        return -1;
    }

    return 0;
}

int type_comment(struct onerule_t *rule, struct typefunc_t *tf) {
    if(tf->func.number && tf->func.which[tf->func.number-1] == tf->number
        && tf->func.type[tf->func.number-1] == tf->type) {

        if(type_comment_end(rule, tf)) return -1;
    }

    if(tf->func.nest
        && !strncmp(rule->data.data[0], *tf->p, rule->data.len[0])) {

        append_rulelist(0, 0, tf->list, tf->out);

        if(tf->type != TYPE_STRING || !tf->func.number) {
            fputs(rule->data.data[rule->data.number-2], tf->out);
        }

        print_chars(tf->p, rule->data.len[0], tf->out);

        add_to_funclist(&tf->func, tf->number, tf->type);
        tf->func.nest = (tf->type == TYPE_NESTCOM);

        return 1;
    }

    return 0;
}

int type_string(struct onerule_t *rule, struct typefunc_t *tf) {
    if(tf->func.nest || !is_backslashed(*tf->p - tf->n, *tf->p)) {
        return type_comment(rule, tf);
    }

    return 0;
}

int type_nestcom(struct onerule_t *rule, struct typefunc_t *tf) {
    return type_comment(rule, tf);
}

int type_number(struct onerule_t *rule, struct typefunc_t *tf) {
    if(!tf->iw && is_number(*tf->p)) {
        if(!tf->func.number) {
            append_rulelist(rule->data.data[0], rule->data.data[1], tf->list,
                tf->out);
        }

        print_number(tf->p, tf->out);

        return 1;
    }

    return 0;
}

int type_fpnumber(struct onerule_t *rule, struct typefunc_t *tf) {
    if(!tf->iw && is_fpnumber(*tf->p)) {
        if(!tf->func.number) {
            append_rulelist(rule->data.data[0], rule->data.data[1], tf->list,
                tf->out);
        }

        print_number(tf->p, tf->out);

        return 1;
    }

    return 0;
}

int type_start(struct onerule_t *rule, struct typefunc_t *tf) {
    if(tf->pos & POS_START && !tf->n) {
        tf->pos &= ~POS_START;
        fputs(rule->data.data[0], tf->out);
    }

    return 0;
}

int type_end(struct onerule_t *rule, struct typefunc_t *tf) {
    if(tf->pos & POS_END && !**tf->p) {
        tf->pos &= ~POS_END;
        fputs(rule->data.data[0], tf->out);
    }

    return 0;
}
