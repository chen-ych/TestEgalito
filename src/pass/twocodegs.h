#ifndef EGALITO_PASS_TWOCODE_GS_H
#define EGALITO_PASS_TWOCODE_GS_H

#include <vector>
#include "chunkpass.h"

class GSTable;
class IFuncList;

class TwocodeGSPass : public ChunkPass {
private:
    Conductor *conductor;
    GSTable *gsTable;
    IFuncList *ifuncList;

    std::vector<std::pair<Block *, Instruction *>> directCalls;
    std::vector<std::pair<Block *, Instruction *>> tailRecursions;
    std::vector<std::pair<Block *, Instruction *>> indirectCalls;
    std::vector<std::pair<Block *, Instruction *>> indirectTailRecursions;

    std::vector<std::pair<Block *, Instruction *>> jumpTableJumps;

    std::vector<std::pair<Block *, Instruction *>> RIPrelativeCalls;
    std::vector<std::pair<Block *, Instruction *>> RIPrelativeJumps;

    std::vector<std::pair<Block *, Instruction *>> pointerLoads;
    std::vector<std::pair<Block *, Instruction *>> pointerLinks;

    std::vector<std::pair<Block *, Instruction *>> functionReturns;
public:
    TwocodeGSPass(Conductor *conductor, GSTable *gsTable, IFuncList *ifuncList)
        : conductor(conductor), gsTable(gsTable), ifuncList(ifuncList) {}

    virtual void visit(Program *program);
    virtual void visit(Module *module);
private:
    virtual void visit(Function *function);
    virtual void visit(Block *block);
    virtual void visit(DataRegion *dataRegion);
    virtual void visit(PLTTrampoline *trampoline);
    virtual void visit(JumpTableEntry *jumpTableEntry);
    virtual void visit(VTable *vtable);
    virtual void visit(VTableEntry *vtableEntry);

    void convert();
    void redirectEgalitoFunctionPointers();
    void redirectFunctionPointerLinks(DataVariable *var);
    void rewriteDirectCall(Block *block, Instruction *instr);
    void rewriteTailRecursion(Block *block, Instruction *instr);
    void rewriteIndirectCall(Block *block, Instruction *instr);
    void rewriteIndirectTailRecursion(Block *block, Instruction *instr);
    void rewriteJumpTableJump(Block *block, Instruction *instr);
    void rewriteRIPrelativeCall(Block *block, Instruction *instr);
    void rewriteRIPrelativeJump(Block *block, Instruction *instr);
    void rewritePointerLoad(Block *block, Instruction *instr);
    void rewriteReturn(Block *block, Instruction *instr);
};

#endif
