#include <cassert>
#include <cstring>
#include "injectbridge.h"
#include "conductor/bridge.h"
#include "elf/reloc.h"
#include "chunk/dataregion.h"
#include "chunk/link.h"
#include "operation/mutator.h"
#include "snippet/hook.h"
#include "log/log.h"

void InjectBridgePass::visit(Module *module) {
    auto bridge = LoaderBridge::getInstance();

    for(auto reloc : *relocList) {
        auto sym = reloc->getSymbol();
        if(!sym) continue;

        if(bridge->containsName(sym->getName())) {
            makeLinkToLoaderVariable(module, reloc);
        }
    }
}

void InjectBridgePass::makeLinkToLoaderVariable(Module *module, Reloc *reloc) {
    LOG(1, "[InjectBridge] assigning EgalitoLoaderLink for "
        << reloc->getSymbol()->getName());

    auto link = new EgalitoLoaderLink(reloc->getSymbol()->getName());

    auto address = reloc->getAddress() + module->getBaseAddress();
    DataVariable::create(module, address, link, reloc->getSymbol());
}
