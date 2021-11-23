#include <iostream>
#include "internalcalls.h"
#include "operation/find.h"
#include "instr/semantic.h"
#include "instr/concrete.h"
#include "instr/writer.h"
#include "log/log.h"

void InternalCalls::visit(Module *module) {
    functionList = module->getFunctionList();
    recurse(functionList);
}

void InternalCalls::visit(Instruction *instruction) {
    auto semantic = instruction->getSemantic();
    auto link = semantic->getLink();
    if(!link) return;  // no link in this instruction
    if(link->getTarget()) return;  // link already resolved

    auto targetAddress = link->getTargetAddress();

    // We are only resolving ControlFlowInstruction targets
    if(!dynamic_cast<ControlFlowInstruction *>(semantic)) return;

    LOG0(10, "Looking up target 0x" << std::hex << targetAddress << " -> ");

    Chunk *found = nullptr;
    bool isExternal = false;
    // Common case for call instructions: point at another function
    if(!found) {
        found = functionList->getChildren()->getSpatial()->find(targetAddress);
        if(found && found != instruction->getParent()->getParent()) {
            isExternal = true;
        }
    }
    // Common case for jumps: internal jump elsewhere within function
    if(!found) {
        auto enclosing = instruction->getParent()->getParent();
        auto func = dynamic_cast<Function *>(enclosing);
#ifdef ARCH_X86_64
        // we get jumps into the middle of an instruction to skip "LOCK" prefix
        found = ChunkFind().findInnermostInsideInstruction(func, targetAddress);
#else
        found = ChunkFind().findInnermostAt(func, targetAddress);
#endif
    }
    // Uncommon case for jumps: external jump to another function
    // This can be for tail recursion or for overlapping functions (_nocancel)
    if(!found) {
        auto enclosing = instruction->getParent()->getParent()->getParent();
        auto otherFunctionList = dynamic_cast<FunctionList *>(enclosing);

#if 0
        // Right now, spatial search in a Module/FunctionList doesn't work.
        // It doesn't handle overlapping functions correctly.
        found = ChunkFind().findInnermostAt(module, targetAddress);
#elif 1
        // Here's a hack, look at a few nearby functions in the list to
        // resolve targets. Should really just use a data structure that
        // supports overlaps.
        std::vector<Function *> funcs;
        funcs = CIter::spatial(otherFunctionList)
            ->findAllContaining(targetAddress);
        for(auto f : funcs) {
            found = ChunkFind().findInnermostAt(f, targetAddress);
            if(found) break;
        }
#else
        // Brute-force version, guaranteed to work.
        for(auto f : CIter::functions(module)) {
            found = ChunkFind().findInnermostAt(f, targetAddress);
            if(found) break;
        }
#endif
        if(found && found != instruction->getParent()->getParent()) {
            isExternal = true;
        }
    }

    if(found) {
        LOG(10, "FOUND [" << found->getName() << "]");
#ifdef ARCH_X86_64
        auto offset = targetAddress - found->getAddress();
        if(offset == 0) {
            semantic->setLink(new NormalLink(found, isExternal
                ? Link::SCOPE_EXTERNAL_JUMP : Link::SCOPE_INTERNAL_JUMP));
        }
        else {
            auto i = dynamic_cast<Instruction *>(found);
            if(i && offset == 1) {
                InstrWriterGetData writer;
                i->getSemantic()->accept(&writer);
                if(static_cast<unsigned char>(writer.get()[0]) == 0xf0) {
                    // jumping by skipping the "LOCK" prefix
                    semantic->setLink(new OffsetLink(found, 1, isExternal
                        ? Link::SCOPE_EXTERNAL_JUMP : Link::SCOPE_INTERNAL_JUMP));
                }
                else {
                    LOG(1, "WARNING: unknown prefix when jumping into "
                        "the middle of an instruction!");
                    return;  // skip delete link
                }
            }
            else {
                LOG(1, "WARNING: jumping into the middle of an instruction"
                    " in unknown manner!");
                return;  // skip delete link
            }
        }
#else
        semantic->setLink(new NormalLink(found, isExternal
            ? Link::SCOPE_EXTERNAL_JUMP : Link::SCOPE_INTERNAL_JUMP));
#endif
        delete link;
    }
    else {
        LOG(10, "NOT FOUND!");
    }
}
