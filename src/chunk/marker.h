#ifndef EGALITO_CHUNK_MARKER_H
#define EGALITO_CHUNK_MARKER_H

#include "chunk/chunk.h"
#include "chunk/chunklist.h"
#include "elf/symbol.h"

class Module;
class Instruction;
class RelocList;

class Marker : public ChunkImpl {
private:
    Chunk *base;
    size_t addend;

public:
    Marker() : base(nullptr), addend(0) {}
    Marker(Chunk *base, size_t addend);
    virtual void accept(ChunkVisitor *visitor) {}

    virtual address_t getAddress() const;
    virtual void setAddress(address_t address);

    Chunk *getBase() const { return base; }
    size_t getAddend() const { return addend; }
};

class SectionStartMarker : public Marker {
private:
    DataSection *dataSection;
    long int bias;

public:
    SectionStartMarker(DataSection *dataSection);
    DataSection *getDataSection() const { return dataSection; }
    virtual void accept(ChunkVisitor *visitor) {}

    virtual address_t getAddress() const;
    virtual void setAddress(address_t address);
};

class SectionEndMarker : public Marker {
private:
    DataSection *dataSection;
    long int bias;

public:
    SectionEndMarker(DataSection *dataSection);
    DataSection *getDataSection() const { return dataSection; }
    virtual void accept(ChunkVisitor *visitor) {}

    virtual address_t getAddress() const;
    virtual void setAddress(address_t address);
};

// On RISC-V, sometimes accesses are relative to the global pointer
class GlobalPointerMarker : public Marker {
private:
    address_t address;
public:
    GlobalPointerMarker(address_t address);

    virtual address_t getAddress() const { return address; }
};

class MarkerList : public CollectionChunkImpl<Marker> {
public:
    virtual std::string getName() const { return "markerlist"; }
    virtual void accept(ChunkVisitor *visitor);

    Link *createMarkerLink(Symbol *symbol, size_t addend, Module *module,
        bool isRelative);
    Link *createInferredMarkerLink(address_t address, Module *module,
        bool isRelative);
    Link *createTableJumpTargetMarkerLink(Instruction *jump, size_t offset,
        Module *module, bool isRelative);
    Marker *addGeneralMarker(Chunk *chunk, size_t addend);
    Marker *addStartMarker(DataSection *dataSection);
    Marker *addEndMarker(DataSection *dataSection);
    Marker *addGlobalPointerMarker(address_t address);

private:
    Link *createGeneralMarkerLink(Chunk *chunk,
        size_t addend, Module *module, bool isRelative);
    Link *createStartMarkerLink(
        DataSection *dataSection, Module *module, bool isRelative);
    Link *createEndMarkerLink(
        DataSection *dataSection, Module *module, bool isRelative);
};

#endif
