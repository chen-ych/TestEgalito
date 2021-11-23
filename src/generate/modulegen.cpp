#include <set>
#include <fstream>
#include <cstring>
#include "modulegen.h"
#include "concretedeferred.h"
#include "transform/sandbox.h"
#include "chunk/aliasmap.h"
#include "chunk/concrete.h"
#include "instr/concrete.h"
#include "util/streamasstring.h"
#include "log/log.h"
#include "config.h"

ModuleGen::ModuleGen(Config config, Module *module, SectionList *sectionList)
    : config(config), module(module), sectionList(sectionList) {

}

void ModuleGen::makeDataSections() {
    // Before all LOAD segments, we need to put padding.
    makePaddingSection(0);

    auto phdrTable = getSection("=phdr_table")->castAs<PhdrTableContent *>();
    auto regionList = module->getDataRegionList();
    for(auto region : CIter::children(regionList)) {
        auto tls = regionList->getTLS();
        if(region == tls) continue;

        std::map<address_t, DataSection *> sectionMap;
        for(auto section : CIter::children(region)) {
            sectionMap[section->getAddress()] = section;
        }
        /*auto tlsOverlappingRegion = module->getDataRegionList()
            ->findNonTLSRegionContaining(regionList->getTLS()->getAddress());*/

        if(tls && region->getRange().overlaps(tls->getRange())) {
            for(auto section : CIter::children(tls)) {
                sectionMap[section->getAddress()] = section;
            }
        }

        if(sectionMap.empty()) continue;

        makePaddingSection((*sectionMap.begin()).second->getAddress() & (0x200000-1));

        address_t previousEndAddress = 0;
        uint64_t previousFlags = 0;
        //auto loadSegment = new SegmentInfo(PT_LOAD, PF_R | PF_W, /*0x200000*/ 0x1000);
        SegmentInfo *loadSegment = nullptr;
        for(auto kv: sectionMap) {
            auto section = kv.second;
            uint64_t flags = SHF_ALLOC;
            if(section->getPermissions() & SHF_WRITE) flags |=  SHF_WRITE;

            if(/*section->getAddress() != previousEndAddress
                ||*/ (!previousFlags || flags != previousFlags)) {

                if(loadSegment) {
                    if(!loadSegment->getContainsList().empty()) {
                        phdrTable->add(loadSegment);
                    }
                    else delete loadSegment;
                }
                int segmentFlags = PF_R;
                if(section->getPermissions() & SHF_WRITE) {
                    segmentFlags |= PF_W;
                }
                loadSegment = new SegmentInfo(PT_LOAD, segmentFlags, /*0x200000*/ 0x1000);
                previousFlags = flags;
            }

            switch(section->getType()) {
            case DataSection::TYPE_DATA: {
                LOG(0, "DATA section " << section->getName());
                LOG(0, "    seems like we need " << (section->getAddress() - previousEndAddress) << " padding bytes");
                auto padding = makeIntraPaddingSection(section->getAddress() & (0x1000-1));
                if(previousEndAddress) loadSegment->addContains(padding);

                // by default, make everything writable
                auto dataSection = new Section(section->getName(),
                    SHT_PROGBITS, flags);
                auto content = new DeferredString(region->getDataBytes()
                    .substr(section->getOriginalOffset(), section->getSize()));
                dataSection->setContent(content);
                dataSection->getHeader()->setAddress(section->getAddress());
                sectionList->addSection(dataSection);
                loadSegment->addContains(dataSection);
                maybeMakeDataRelocs(section, dataSection);
                previousEndAddress = section->getAddress() + section->getSize();
                previousFlags = flags;
                break;
            }
            case DataSection::TYPE_BSS: {
                // The TLS .tbss overlaps with other sections, don't write
                // it out here or too much data (e.g. 0x1000) will appear in the
                // output ELF, misaligning virtual addresses.
                if(section->getParent() == tls) continue;

                LOG(0, "BSS section " << section->getName());
                auto padding = makeIntraPaddingSection(section->getAddress() & (0x1000-1));
                if(previousEndAddress) loadSegment->addContains(padding);

                auto bssSection = new Section(section->getName(),
                    /*SHT_NOBITS*/ SHT_PROGBITS, flags);
                //if(region == regionList->getTLS()) {
                if(false && section->getParent() == regionList->getTLS()) {
                    bssSection->setContent(new DeferredString(
                        std::string(section->getSize(), 0x0)));
                    LOG(1, "   Initializing bss with 0");
                }
                else {
                    auto content = new DeferredString(region->getDataBytes()
                        .substr(section->getOriginalOffset(), section->getSize()));
                    bssSection->setContent(content);
                    LOG(1, "   Initializing bss with " << section->getSize() << " data bytes");
                }
                //bssSection->setContent(new DeferredString(""));
                bssSection->getHeader()->setAddress(section->getAddress());
                sectionList->addSection(bssSection);
                loadSegment->addContains(bssSection);
                maybeMakeDataRelocs(section, bssSection);
                previousEndAddress = section->getAddress() + section->getSize();
                previousFlags = flags;
                break;
            }
            case DataSection::TYPE_UNKNOWN:
            default:
                break;
            }
        }

        if(loadSegment) {
            if(!loadSegment->getContainsList().empty()) {
                phdrTable->add(loadSegment);
            }
            else delete loadSegment;
        }

#if 0
        SegmentInfo *loadSegment = nullptr;
        address_t previousEndAddress = 0;
        for(auto section : CIter::children(region)) {
            if (section->getAddress() != previousEndAddress) {
                if(loadSegment) {
                    if(!loadSegment->getContainsList().empty()) {
                        phdrTable->add(loadSegment);
                    }
                    else delete loadSegment;
                }
                loadSegment = new SegmentInfo(PT_LOAD, PF_R | PF_W, /*0x200000*/ 0x1000);
            }

            switch(section->getType()) {
            case DataSection::TYPE_DATA: {
                LOG(0, "DATA section " << section->getName());
                makePaddingSection(section->getAddress() & (0x200000-1));

                // by default, make everything writable
                auto dataSection = new Section(section->getName(),
                    SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
                auto content = new DeferredString(region->getDataBytes()
                    .substr(section->getOriginalOffset(), section->getSize()));
                dataSection->setContent(content);
                dataSection->getHeader()->setAddress(section->getAddress());
                sectionList->addSection(dataSection);
                loadSegment->addContains(dataSection);
                maybeMakeDataRelocs(section, dataSection);
                previousEndAddress = section->getAddress() + section->getSize();
                break;
            }
            case DataSection::TYPE_BSS: {
                LOG(0, "BSS section " << section->getName());
                makePaddingSection(section->getAddress() & (0x200000-1));

                auto bssSection = new Section(section->getName(),
                    /*SHT_NOBITS*/ SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
                if(region == regionList->getTLS()) {
                    bssSection->setContent(new DeferredString(
                        std::string(section->getSize(), 0x0)));
                    LOG(1, "   Initializing bss with 0");
                }
                else {
                    auto content = new DeferredString(region->getDataBytes()
                        .substr(section->getOriginalOffset(), section->getSize()));
                    bssSection->setContent(content);
                    LOG(1, "   Initializing bss with data bytes");
                }
                //bssSection->setContent(new DeferredString(""));
                bssSection->getHeader()->setAddress(section->getAddress());
                sectionList->addSection(bssSection);
                loadSegment->addContains(bssSection);
                previousEndAddress = section->getAddress() + section->getSize();
                break;
            }
            case DataSection::TYPE_UNKNOWN:
            default:
                break;
            }
        }

        if(loadSegment->getContainsList().empty()) {
            delete loadSegment;
        }
        else {
            phdrTable->add(loadSegment);
        }
#endif
    }
}

void ModuleGen::maybeMakeDataRelocs(DataSection *section, Section *sec) {
    std::set<DataVariable *> jumpTableVars; //jump table entries
    for(auto jt : CIter::children(module->getJumpTableList())) {
        for(auto entry : CIter::children(jt)) {
            jumpTableVars.insert(entry->getDataVariable());
        }
    }
    std::vector<DataVariable *> ldRelocVars, generalRelocVars;
    for(auto var : CIter::children(section)) {
        auto link = var->getDest();
        if(!link) continue;
        if(jumpTableVars.find(var) != jumpTableVars.end()) continue;

        if(dynamic_cast<LDSOLoaderLink *>(var->getDest())) {
            ldRelocVars.push_back(var);
        }
        else if(dynamic_cast<TLSDataOffsetLink *>(var->getDest())) {
            // handled elsewhere
        }
        else if(config.getRelocsForAbsoluteRefs()) {
            generalRelocVars.push_back(var);
        }
    }
    LOG(1, "maybeMakeDataRelocs with "
        << ldRelocVars.size() << ", " << generalRelocVars.size());
    if(ldRelocVars.empty() && generalRelocVars.empty()) return;

    LOG(1, "Adding relocations for data section [" << section->getName() << "]");
    auto relaDyn = (*sectionList)[".rela.dyn"]->castAs<DataRelocSectionContent *>();
    for(auto var : ldRelocVars) {
        auto link = var->getDest();
        relaDyn->addUndefinedRef(var,
            static_cast<LDSOLoaderLink *>(link)->getTargetName());
        LOG(2, "    relocation for LDSO ["
            << static_cast<LDSOLoaderLink *>(link)->getTargetName()
            << "] at 0x" << std::hex << var->getAddress());
    }
    for(auto var : generalRelocVars) {
        auto link = var->getDest();
        if(auto target = link->getTarget()) {
            LOG(2, "    relocation for [" << target->getName()
                << "] at 0x" << std::hex << var->getAddress());
            if(auto function = dynamic_cast<Function *>(target)) {
                if(function->isIFunc()) {
                    relaDyn->addDataIFuncRef(var, function->getAddress());
                }
                else {
                    relaDyn->addDataFunctionRef(var, function);
                }
            }
            else if(auto plt = dynamic_cast<PLTTrampoline *>(target)) {
                if(plt->isIFunc()) {
                    relaDyn->addDataIFuncRef(var, plt->getAddress());
                }
                else {
                    relaDyn->addDataArbitraryRef(var, link->getTargetAddress());
                }
            }
            else if(auto intAndExt = dynamic_cast<InternalAndExternalDataLink *>(link)) {
                relaDyn->addDataExternalRef(var, intAndExt->getExternalSymbol(), sec,
                    module, 0);
            }
            else /*if(link->isAbsolute())*/ {
                relaDyn->addDataArbitraryRef(var, link->getTargetAddress());
            }
            /*else {
                // relative internal references do not need relocations 
                LOG(3, "        skipping relocation for internal reference ["
                    << target->getName()
                    << "] at 0x" << std::hex << var->getAddress());
            }*/
        }
        else {
            if(auto pltLink = dynamic_cast<PLTLink *>(link)) {
                LOG(2, "    relocation for PLT ["
                    << pltLink->getPLTTrampoline()->getExternalSymbol()->getName()
                    << "] at 0x" << std::hex << var->getAddress());
                // GLOB_DAT relocation
                relaDyn->addUndefinedRef(var,
                    pltLink->getPLTTrampoline()->getExternalSymbol()->getName());
            }
            else if(auto extSymLink = dynamic_cast<ExternalSymbolLink *>(link)) {
                LOG(2, "    relocation for external symbol ["
                    << extSymLink->getExternalSymbol()->getName()
                    << "] at 0x" << std::hex << var->getAddress());
                relaDyn->addDataExternalRef(var, extSymLink->getExternalSymbol(), sec,
                    module, extSymLink->getOffset());
            }
            else if(auto copyRelocLink = dynamic_cast<CopyRelocLink *>(link)) {
                LOG(2, "    relocation for copy reloc ["
                    << copyRelocLink->getExternalSymbol()->getName()
                    << "] at 0x" << std::hex << var->getAddress());
                relaDyn->addCopyExternalRef(var, copyRelocLink->getExternalSymbol(), sec);
            }
            else LOG(1, "Unhandled link type for relocation");
        }
    }

    // very important! dynsyms are created on demand per reloc, which are not
    // necessarily sorted by name, so indices may have been invalidated.
    auto dynsym = (*sectionList)[".dynsym"]->castAs<SymbolTableContent *>();
    dynsym->recalculateIndices();
}

void ModuleGen::makeText() {
    // Before all LOAD segments, we need to put padding.
    makePaddingSection(0);

    // Split separate pages into their own LOAD sections.
    // First, find the set of all pages that are used.
    std::set<address_t> pagesUsed;
    for(auto func : CIter::functions(module)) {
        address_t start = func->getAddress() & ~0xfff;
        address_t end = ((func->getAddress() + func->getSize()) + 0xfff) & ~0xfff;
        for(address_t page = start; page < end; page += 0x1000) {
            LOG(19, "code uses page " << std::hex << page);
            pagesUsed.insert(page);
        }
    }

    // Next, find sequences of contiguous pages and merge them.
    std::vector<Range> codeRegions;
    std::set<address_t>::iterator i = pagesUsed.begin();
    while(i != pagesUsed.end()) {
        size_t size = 0;
        std::set<address_t>::iterator j = i;
        while(j != pagesUsed.end() && (*j) == (*i) + size) {
            j++;
            size += 0x1000;
        }

        address_t address = *i;
        codeRegions.push_back(Range(address, size));
        i = j;
    }

    // Finally, map all code regions as individual ELF segments.
    auto phdrTable = getSection("=phdr_table")->castAs<PhdrTableContent *>();
    for(auto range : codeRegions) {
        auto address = range.getStart();
        auto size = range.getSize();
        LOG(1, "map " << std::hex << address << " size " << size);

        std::string name;
        if(codeRegions.size() == 1 && !config.getUniqueSectionNames()) {
            name = ".text";
        }
        else {
            name = StreamAsString() << ".text.0x" << std::hex << address;
        }

        auto textSection = new Section(name.c_str(), SHT_PROGBITS,
            SHF_ALLOC | SHF_EXECINSTR);
        DeferredString *textValue = nullptr;
        if(auto backing = config.getCodeBacking()) {
            // Don't modify backing after this point to avoid invalidating c_str
            auto copy = new std::string(backing->getBuffer());
            textValue = new DeferredString(
                reinterpret_cast<const char *>(copy->c_str()),
                size);
        }
        else {
            textValue = new DeferredString(
                reinterpret_cast<const char *>(address), size);
        }
        if(config.isKernel()) {
            textSection->getHeader()->setAddress(LINUX_KERNEL_CODE_BASE);
        }
        else {
            textSection->getHeader()->setAddress(address);
        }
        textSection->setContent(textValue);

        sectionList->addSection(textSection);

        makeRelocSectionFor(name);
        makeSymbolsAndRelocs(address, size, name);

        auto loadSegment = new SegmentInfo(PT_LOAD, PF_R | PF_X, 0x1000);
        loadSegment->addContains(textSection);
        phdrTable->add(loadSegment);
    }
}

void ModuleGen::makeTextAccumulative() {
    auto backing = config.getCodeBacking();
    auto address = backing->getBase();
    auto size = backing->getSize();
    makeRelocSectionFor(".text");
    makeSymbolsAndRelocs(address, size, ".text");
}

void ModuleGen::makeRelocSectionFor(const std::string &otherName) {
#if 0
    auto reloc = new RelocSectionContent2(&sectionList,
        new SectionRef(&sectionList, otherName));
    auto relocSection = new Section(".rela" + otherName, SHT_RELA, SHF_INFO_LINK);
    relocSection->setContent(reloc);

    sectionList->addSection(relocSection);
#endif
}

void ModuleGen::makeSymbolsAndRelocs(address_t begin, size_t size,
    const std::string &textSection) {

    // Add symbols to the symbol list, but only for those functions
    // which fall into the given range [begin, begin+size).
    for(auto func : CIter::functions(module)) {
        if(blacklistedSymbol(func->getName())) {
            continue;  // skip making a symbol for this function
        }

        if(func->getAddress() < begin
            || func->getAddress() + func->getSize() >= begin + size) {
            continue;  // not in this text section
        }

        if(config.isKernel()) {
            // fix addresses for kernel generation
            auto backing = config.getCodeBacking();
            func->getPosition()->set(func->getAddress() - backing->getBase()
                + LINUX_KERNEL_CODE_BASE);
        }

        LOG(1, "making symbol for " << func->getName());
        makeSymbolInText(func, textSection);
#if 0
        makeRelocInText(func, textSection);
#endif

        if(config.isKernel()) {
            // undo address fix
            auto backing = config.getCodeBacking();
            func->getPosition()->set(backing->getBase() + func->getAddress());
        }
    }
#if 0
    // Make symbols for PLT entries
    for(auto plt : CIter::plts(module)) {
        auto symtab = getSection(".symtab")->castAs<SymbolTableContent *>();

        auto external = plt->getExternalSymbol();
        auto nameCopy = new std::string(plt->getName().c_str());
        auto sym = new Symbol(plt->getAddress(), plt->getSize(),
            nameCopy->c_str(), external->getType(), external->getBind(),
            0, 0);

        // add name to string table
        auto value = symtab->addPLTSymbol(plt, sym);
        value->addFunction([this, textSection] (ElfXX_Sym *symbol) {
            symbol->st_shndx = shdrIndexOf(textSection);
        });
    }
#endif

#if 0
    // Handle any other types of symbols that need generating.
    for(auto sym : *elfSpace->getSymbolList()) {
        if(sym->isFunction()) continue;  // already handled
        if(blacklistedSymbol(sym->getName())) continue;  // blacklisted

        // undefined symbol
        if(sym->getSectionIndex() == SHN_UNDEF) {
            auto symtab = getSection(".symtab")->castAs<SymbolTableContent *>();
            symtab->addUndefinedSymbol(sym);
        }
    }
#endif
}

void ModuleGen::makeSymbolInText(Function *func, const std::string &textSection) {
    auto symtab = getSection(".symtab")->castAs<SymbolTableContent *>();

    // add name to string table
    auto value = symtab->addSymbol(func, func->getSymbol());
    value->addFunction([this, textSection] (ElfXX_Sym *symbol) {
        symbol->st_shndx = shdrIndexOf(textSection);
    });

    if(func->getSymbol()) for(auto alias : func->getSymbol()->getAliases()) {
        // skip functions with the same name (due to versioning)
        if(alias->getName() == func->getName()) continue;
        // skip SECTION symbols that alias with _init, _fini, _start
        if(alias->getType() != func->getSymbol()->getType()) continue;

        /*auto aliasNameStr = FunctionAliasMap::getNameWithoutVersion(alias->getName());
        if(aliasNameStr != alias->getName()) {
            char *aliasName = new char[aliasNameStr.length() + 1];
            std::strcpy(aliasName, aliasNameStr.c_str());
            LOG(1, "replacing symbol alias [" << alias->getName() << "] -> ["
                << aliasNameStr << "]");
            alias = new Symbol(alias->getAddress(), alias->getSize(), aliasName,
                alias->getType(), alias->getBind(), alias->getIndex(),
                alias->getSectionIndex());
        }*/
        LOG(1, "writing symbol for alias [" << alias->getName() << "]");

        // add name to string table
        auto value = symtab->addSymbol(func, alias);
        value->addFunction([this, textSection] (ElfXX_Sym *symbol) {
            symbol->st_shndx = shdrIndexOf(textSection);
        });
    }

#if 0  // ExternalSymbol can no longer be converted to a Symbol
    for(auto block : CIter::children(func)) {
        for(auto instr : CIter::children(block)) {
            if(auto link = instr->getSemantic()->getLink()) {
                if(auto sol = dynamic_cast<PLTLink *>(link)) {
                    auto sym = sol->getPLTTrampoline()->getTargetSymbol();
                    symtab->addUndefinedSymbol(sym);
                    LOG(1, "undefined symbol with name " << sym->getName());
                }
            }
        }
    }
#endif
}

void ModuleGen::makeRelocInText(Function *func, const std::string &textSection) {
    auto reloc = getSection(".rela" + textSection)->castAs<RelocSectionContent2 *>();

    for(auto block : CIter::children(func)) {
        for(auto instr : CIter::children(block)) {
            if(auto link = instr->getSemantic()->getLink()) {
                //LOG(1, "adding relocation at " << instr->getName());

                if(link->getTarget()) {
                    Chunk *functionTarget = link->getTarget();
                    while(functionTarget && !dynamic_cast<Function *>(functionTarget)) {
                        functionTarget = functionTarget->getParent();
                    }

                    if(functionTarget) {
                        auto func = static_cast<Function *>(functionTarget);
                        auto symtab = getSection(".symtab")
                            ->castAs<SymbolTableContent *>();
                        auto sym = symtab->find(SymbolInTable(
                            SymbolTableContent::getTypeFor(func),
                            func->getSymbol()));
                        if(sym) {
                            //reloc->addFunctionRef();
                            //continue;
                        }
                    }
                }

                auto target = link->getTargetAddress();
                DataSection *targetSection = module->getDataRegionList()
                    ->findDataSectionContaining(target);

                if(targetSection) {
                    reloc->addDataRef(instr->getAddress(), target, targetSection);
                }
            }
        }
    }
}

void ModuleGen::makeTLS() {
    auto phdrTable = getSection("=phdr_table")->castAs<PhdrTableContent *>();
    auto regionList = module->getDataRegionList();
    auto tlsRegion = regionList->getTLS();
    if(!tlsRegion) return;
    SegmentInfo *segment = new SegmentInfo(PT_TLS, PF_R | PF_W, 0x8);
    makePaddingSection(tlsRegion->getAddress() & (0x1000-1));
    LOG(0, "making TLS in modulegen for " << module->getName());
    for(auto section : CIter::children(tlsRegion)) {
        switch(section->getType()) {
        case DataSection::TYPE_DATA: {
            LOG(0, "TLS DATA section " << section->getName());

            // by default, make everything writable
            auto dataSection = new Section(section->getName(),
                SHT_PROGBITS, SHF_ALLOC | SHF_WRITE | SHF_TLS);
            auto content = new DeferredString(tlsRegion->getDataBytes()
                .substr(section->getOriginalOffset(), section->getSize()));
            dataSection->setContent(content);
            dataSection->getHeader()->setAddress(section->getAddress());
            sectionList->addSection(dataSection);
            segment->addContains(dataSection);
            maybeMakeDataRelocs(section, dataSection);
            break;
        }
        case DataSection::TYPE_BSS: {
            LOG(0, "TLS BSS section " << section->getName());

            auto bssSection = new Section(section->getName(),
                /*SHT_NOBITS*/ SHT_PROGBITS, SHF_ALLOC | SHF_WRITE | SHF_TLS);
            /*auto content = new DeferredString(region->getDataBytes()
                .substr(section->getOriginalOffset(), section->getSize()));
            bssSection->setContent(content);*/
            /*bssSection->setContent(new DeferredString(
                std::string(section->getSize(), 0x0)));*/
            bssSection->setContent(new DeferredString(""));
            LOG(0, "TLS BSS section is size " << section->getSize());
            //bssSection->setContent(new TBSSContent(section->getSize()));
            bssSection->getHeader()->setAddress(section->getAddress());
            sectionList->addSection(bssSection);
            segment->addContains(bssSection);
            segment->setAdditionalMemSize(section->getSize());
            break;
        }
        case DataSection::TYPE_UNKNOWN:
        default:
            break;
        }
    }

    //makePaddingSection(0x1000);

    phdrTable->add(segment);
    makeTLSRelocs(tlsRegion);
}

void ModuleGen::makeTLSRelocs(TLSDataRegion *region) {
    auto relaDyn = (*sectionList)[".rela.dyn"]->castAs<DataRelocSectionContent *>();

    for(auto section : CIter::children(region)) {
        std::vector<DataVariable *> relocVars;
        LOG(1, "Searching for relocations in [" << section->getName() << "]");
        for(auto var : CIter::children(section)) {
            LOG(1, "    considering relocation at 0x" << var->getAddress());
            if(!var->getDest()) continue;
            relocVars.push_back(var);
        }

    #if 0
        auto overlappingRegion = module->getDataRegionList()
            ->findNonTLSRegionContaining(region->getAddress());
        for(auto s : CIter::children(overlappingRegion)) {
            if(!section->getRange().overlaps(s->getRange())) continue;
            for(auto v : CIter::children(s)) {
                if(section->getRange().contains(v->getAddress())) {
                    LOG(1, "    considering relocation at 0x" << v->getAddress());
                    if(!v->getDest()) continue;
                    relocVars.push_back(v);
                }
            }
        }
    #endif

        for(auto var : relocVars) {
            LOG(1, "Generating TLS relocation at 0x" << var->getAddress()
                << " pointing to 0x" << var->getDest()->getTargetAddress());
            relaDyn->addDataRef(var->getAddress(),
                var->getDest()->getTargetAddress(),
                section);
        }
    }

    auto regionList = module->getDataRegionList();
    for(auto r : CIter::children(regionList)) {
        for(auto s : CIter::children(r)) {
            if(s->getType() != DataSection::TYPE_DATA) continue;
            for(auto var : CIter::children(s)) {
                if(auto v = dynamic_cast<TLSDataOffsetLink *>(var->getDest())) {
                    LOG(1, "Generating TLS TPOFF64 relocation at 0x"
                        << var->getAddress() << " referring to index "
                        << v->getRawTarget());
                    relaDyn->addTLSOffsetRef(var->getAddress(), v);
                }
            }
        }
    }
    //sectionList->addSection(relocSection);
    //relocSections.push_back(relocSection);
}

void ModuleGen::makePaddingSection(size_t desiredAlignment) {
    // We could assign unique names to the padding sections, but since we
    // never look them up by name in SectionList, it doesn't actually matter.
    auto paddingSection = new Section("=padding");
    auto paddingContent = new PagePaddingContent(
        sectionList->back(), desiredAlignment);
    paddingSection->setContent(paddingContent);
    sectionList->addSection(paddingSection);
}

Section *ModuleGen::makeIntraPaddingSection(size_t desiredAlignment) {
    // We could assign unique names to the padding sections, but since we
    // never look them up by name in SectionList, it doesn't actually matter.
    auto paddingSection = new Section("=intra-padding");
    auto paddingContent = new PagePaddingContent(
        sectionList->back(), desiredAlignment, false);
    paddingSection->setContent(paddingContent);
    sectionList->addSection(paddingSection);
    return paddingSection;
}

size_t ModuleGen::shdrIndexOf(Section *section) {
#if 0
    auto shdrTableSection = getSection("=shdr_table");
    auto shdrTable = shdrTableSection->castAs<ShdrTableContent *>();
    return shdrTable->indexOf(shdrTable->find(section));
#else
    return sectionList->indexOf(section);
#endif
}

size_t ModuleGen::shdrIndexOf(const std::string &name) {
#if 0
    auto shdrTableSection = getSection("=shdr_table");
    auto shdrTable = shdrTableSection->castAs<ShdrTableContent *>();
    return shdrTable->indexOf(shdrTable->find(sectionList[name]));
#else
    return sectionList->indexOf(name);
#endif
}

bool ModuleGen::blacklistedSymbol(const std::string &name) {
    return false;
}
