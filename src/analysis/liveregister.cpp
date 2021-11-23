#include "liveregister.h"
#include "analysis/usedef.h"
#include "analysis/walker.h"
#include "analysis/controlflow.h"
#include "analysis/savedregister.h"
#include "chunk/concrete.h"
#include "instr/register.h"
#include "instr/isolated.h"

#include "log/log.h"

LiveInfo LiveRegister::getInfo(Function *function) {
    auto it = list.find(function);
    if(it == list.end()) {
        detect(function);
    }
    return list[function];
}

LiveInfo LiveRegister::getInfo(UDRegMemWorkingSet *working) {
    Function *function = working->getFunction();
    auto it = list.find(function);
    if(it == list.end()) {
        detect(working);
    }
    return list[function];
}

void LiveRegister::detect(Function *function) {
    ControlFlowGraph cfg(function);
    UDConfiguration config(&cfg);
    UDRegMemWorkingSet working(function, &cfg);
    UseDef usedef(&config, &working);

    SccOrder order(&cfg);
    order.genFull(0);
    usedef.analyze(order.get());
    detect(&working);
}

void LiveRegister::detect(UDRegMemWorkingSet *working) {
#ifdef ARCH_AARCH64
    Function *function = working->getFunction();
    LiveInfo &info = list[function];

    LOG(10, "LiveRegister " << function->getName());
    for(const auto& s : working->getStateList()) {
        for(const auto& def : s.getRegDefList()) {
            info.kill(def.first);
        }
    }

    // if this function contains a function call or a jump to another
    // function, we assume all caller-saved or temporary registers are killed
    auto module = dynamic_cast<Module *>(function->getParent()->getParent());
    for(const auto& s : working->getStateList()) {
        if(StateGroup::isCall(&s) || StateGroup::isExternalJump(&s, module)) {
            for(size_t i = 0; i < 19; i++) {
                info.kill(i);
            }
            break;
        }
    }

    // resurrect actually saved registers
    SavedRegister saved;
    for(auto r : saved.getList(function)) {
        info.live(r);
    }

    LOG0(10, "live registers:");
    for(size_t i = 0; i < 32; i++) {
        if(info.get(i)) {
            LOG0(10, " " << i);
        }
    }
    LOG(10, "");
#else
    LOG(0, "NYI");
#endif
}

void LiveInfo::kill(int reg) {
    if(reg < 32) {
        regs[reg] = 0;
    }
}

void LiveInfo::live(int reg) {
    if(reg < 32) {
        regs[reg] = 1;
    }
}
