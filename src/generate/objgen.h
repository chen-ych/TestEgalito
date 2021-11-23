#ifndef EGALITO_ELF_OBJGEN_H
#define EGALITO_ELF_OBJGEN_H

#include "transform/sandbox.h"
#include "elf/elfspace.h"
#include "section.h"
#include "sectionlist.h"

class ObjGen {
private:
    ElfSpace *elfSpace;
    MemoryBacking *backing;
    std::string filename;
    SectionList sectionList;
    int sectionSymbolCount;
public:
    ObjGen(ElfSpace *elfSpace, MemoryBacking *backing, std::string filename);
public:
    void generate();
private:
    void makeHeader();
    void makeSymbolInfo();
    void makeRelocInfo(const std::string &textSection);
    void makeText();
    void makeSymbolsAndRelocs(address_t begin, size_t size,
        const std::string &textSection);
    void makeSymbolInText(Function *func, const std::string &textSection);
    void makeRelocInText(Function *func, const std::string &textSection);
    void makeRoData();
    void makeShdrTable();
private:
    void updateSymbolTable();
    void updateOffsets();
    void serialize();
private:
    static bool blacklistedSymbol(const std::string &name);
};

#endif
