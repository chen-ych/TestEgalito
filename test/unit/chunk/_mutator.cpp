#include "framework/include.h"
#include "StreamAsString.h"
#include "conductor/conductor.h"
#include "chunk/dump.h"
#include "operation/mutator.h"
#include "operation/find2.h"
#include "instr/concrete.h"
#include "instr/writer.h"
#include "disasm/disassemble.h"
#include "pass/chunkpass.h"
#include "pass/splitbasicblock.h"
#include "log/temp.h"
#include "log/registry.h"

static Instruction *makeWithImmediate(unsigned char imm) {
#ifdef ARCH_X86_64
    // add imm, %eax
    std::vector<unsigned char> bytes = {0x83, 0xc0, imm};
#elif defined(ARCH_AARCH64) || defined(ARCH_ARM)
    // add X0, X0, #imm
    unsigned char imm1 = (imm << 2) & 0xFF;
    unsigned char imm2 = (imm >> 6) & 0xFF;
    std::vector<unsigned char> bytes = {0x00, imm1, imm2, 0x91};
#elif defined(ARCH_RISCV)
    // addi r1, r1, imm
    unsigned char imm1 = (imm << 4) & 0xff;
    unsigned char imm2 = (imm >> 4) & 0xff;
    std::vector<unsigned char> bytes = {0x93, 0x80, imm1, imm2};
#endif

    return Disassemble::instruction(bytes, true, 0);
}

static Block *makeBlock() {
    PositionFactory *positionFactory = PositionFactory::getInstance();
    Block *block = new Block();
    block->setPosition(
        positionFactory->makePosition(nullptr, block, 0));
    return block;
}

static void ensureValues(Block *block, const std::vector<unsigned char> &values) {
    size_t index = 0;
    for(auto ins : CIter::children(block)) {
        CAPTURE(ins->getName());  // debug info, print instr name

        InstrWriterGetData writer;
        ins->getSemantic()->accept(&writer);
#ifdef ARCH_X86_64
        CHECK(writer.get()[2] == values[index]);
#elif defined(ARCH_AARCH64)
        unsigned char imm = ((writer.get()[1] >> 2) | (writer.get()[2] << 6))
            & 0xFF;
        CHECK(imm == values[index]);
#endif
        index ++;
    }
}

TEST_CASE("calling append() on ChunkMutator", "[chunk][normal]") {
    PositionFactory *positionFactory = PositionFactory::getInstance();

    Block *block = makeBlock();

    Chunk *prevChunk = nullptr;
    for(unsigned char c = 1; c <= 3; c ++) {
        auto instr = makeWithImmediate(c);
        instr->setPosition(
            positionFactory->makePosition(prevChunk, instr, block->getSize()));
        ChunkMutator(block).append(instr);

        prevChunk = instr;
    }

    ensureValues(block, {1, 2, 3});
    delete block;
}

TEST_CASE("calling insert functions with ChunkMutator", "[chunk][normal]") {
    PositionFactory *positionFactory = PositionFactory::getInstance();

    Block *block = makeBlock();

    SECTION("empty list") {
        SECTION("call prepend()") {
            auto instr = makeWithImmediate(22);
            instr->setPosition(positionFactory->makePosition(block, instr, 0));
            ChunkMutator(block).prepend(instr);
            ensureValues(block, {22});
        }
    }

    SECTION("non-empty list") {
        Chunk *prevChunk = nullptr;
        for(unsigned char c = 1; c <= 3; c ++) {
            auto instr = makeWithImmediate(c);
            instr->setPosition(
                positionFactory->makePosition(prevChunk, instr, block->getSize()));
            ChunkMutator(block).append(instr);

            prevChunk = instr;
        }

        ensureValues(block, {1, 2, 3});

        SECTION("call prepend()") {
            auto instr = makeWithImmediate(22);
            instr->setPosition(positionFactory->makePosition(block, instr, 0));
            ChunkMutator(block).prepend(instr);
            ensureValues(block, {22, 1, 2, 3});
        }

        SECTION("call insertAfter()") {
            auto point = block->getChildren()->genericGetAt(0);  // gives the '1'
            auto instr = makeWithImmediate(44);
            instr->setPosition(positionFactory->makePosition(point, instr, 0));
            ChunkMutator(block).insertAfter(point, instr);
            ensureValues(block, {1, 44, 2, 3});
        }

        SECTION("call insertBefore()") {
            auto point1 = block->getChildren()->genericGetAt(0);  // gives the '1'
            auto point2 = block->getChildren()->genericGetAt(1);  // gives the '2'
            auto instr = makeWithImmediate(44);
            instr->setPosition(positionFactory->makePosition(point1, instr, 0));
            ChunkMutator(block).insertBefore(point2, instr);
            ensureValues(block, {1, 44, 2, 3});
        }

        SECTION("call insertAfter() with NULL point") {
            auto instr = makeWithImmediate(44);
            instr->setPosition(positionFactory->makePosition(block, instr, 0));
            ChunkMutator(block).insertAfter(nullptr, instr);
            ensureValues(block, {44, 1, 2, 3});
        }

        SECTION("call insertBefore() with NULL point") {
            auto point3 = block->getChildren()->genericGetLast();  // gives the '3'
            auto instr = makeWithImmediate(44);
            instr->setPosition(positionFactory->makePosition(point3, instr, 0));
            ChunkMutator(block).insertBefore(nullptr, instr);
            ensureValues(block, {1, 2, 3, 44});
        }
    }

    delete block;
}

#if 0
TEST_CASE("calling splitBlockBefore() in ChunkMutator", "[chunk][fast]") {
    TemporaryLogLevel tll("pass", 20);

    ElfMap elf(TESTDIR "hi0");
    Conductor conductor;
    conductor.parseExecutable(&elf);

    Function *main = ChunkFind2(&conductor).findFunction("main");
    REQUIRE(main != nullptr);

    SplitBasicBlock splitBasicBlock;
    conductor.getProgram()->accept(&splitBasicBlock);

    ChunkDumper dumper;
    main->accept(&dumper);
}
#endif
