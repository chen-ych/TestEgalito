#ifndef EGALITO_CHUNK_LINK_H
#define EGALITO_CHUNK_LINK_H

#include <vector>
#include <string>
#include "chunkref.h"
#include "util/iter.h"
#include "types.h"

/** Represents a reference from a Chunk that may need to be updated.

    Some Links refer to a target Chunk, and the offset may change if either
    the source or destination are moved. Others store a fixed target address,
    which again involves some recomputation if the source Chunk moves.
*/
class Link {
public:
    enum LinkScope {
        SCOPE_UNKNOWN           = 0,
        SCOPE_WITHIN_FUNCTION   = 1 << 0,  // e.g. short jump
        SCOPE_WITHIN_SECTION    = 1 << 1,  // code-to-code links
        SCOPE_WITHIN_MODULE     = 1 << 2,  // inside the same ELF

        SCOPE_INTERNAL_JUMP     = SCOPE_WITHIN_FUNCTION
            | SCOPE_WITHIN_SECTION | SCOPE_WITHIN_MODULE,
        SCOPE_EXTERNAL_JUMP     = SCOPE_WITHIN_SECTION | SCOPE_WITHIN_MODULE,
        SCOPE_INTERNAL_DATA     = SCOPE_WITHIN_MODULE,
        SCOPE_EXTERNAL_DATA     = 0,
        SCOPE_EXTERNAL_CODE     = 0,
    };
public:
    virtual ~Link() {}

    /** Returns target as a Chunk, if possible. May return NULL. */
    virtual ChunkRef getTarget() const = 0;
    virtual address_t getTargetAddress() const = 0;

    virtual bool isAbsolute() const = 0;
    virtual bool isRIPRelative() const = 0;

    virtual LinkScope getScope() const = 0;
    virtual bool isExternalJump() const = 0;
    virtual bool isWithinModule() const = 0;
};

template <typename BaseType>
class LinkDefaultAttributeDecorator : public BaseType {
public:
    virtual bool isAbsolute() const { return false; }
    virtual bool isRIPRelative() const { return false; }
};

template <Link::LinkScope Scope, typename BaseType>
class LinkScopeDecorator : public BaseType {
public:
    virtual Link::LinkScope getScope() const { return Scope; }
    virtual bool isExternalJump() const
        { return !matches(Link::SCOPE_WITHIN_FUNCTION); }
    virtual bool isWithinModule() const
        { return matches(Link::SCOPE_WITHIN_MODULE); }
private:
    bool matches(Link::LinkScope s) const
        { return (Scope & s) == s; }
};

class LinkImpl : public LinkDefaultAttributeDecorator<Link> {
private:
    Link::LinkScope scope;
public:
    LinkImpl(Link::LinkScope scope) : scope(scope) {}

    virtual Link::LinkScope getScope() const { return scope; }
    virtual bool isExternalJump() const
        { return !matches(Link::SCOPE_WITHIN_FUNCTION); }
    virtual bool isWithinModule() const
        { return matches(Link::SCOPE_WITHIN_MODULE); }

    void setScope(Link::LinkScope scope) { this->scope = scope; }
private:
    bool matches(Link::LinkScope s) const
        { return (scope & s) == s; }
};


// --- standard Chunk links ---

class NormalLinkBase : public LinkImpl {
private:
    ChunkRef target;
public:
    NormalLinkBase(ChunkRef target, Link::LinkScope scope)
        : LinkImpl(scope), target(target) {}

    virtual ChunkRef getTarget() const { return target; }
    virtual address_t getTargetAddress() const;
};

/** A relative reference to another Chunk.

    The source and destination address may both be updated for this Link.
*/
class NormalLink : public NormalLinkBase {
public:
    using NormalLinkBase::NormalLinkBase;

    virtual bool isRIPRelative() const { return true; }
};

/** An absolute reference to another Chunk.

    Here the source address is irrelevant to getTargetAddress().
*/
class AbsoluteNormalLink : public NormalLinkBase {
public:
    using NormalLinkBase::NormalLinkBase;

    virtual bool isAbsolute() const { return true; }
};

/** Stores a link to a target Chunk, offset a given number of bytes from
    its start. This is used to target into an instruction that has a LOCK
    prefix on x86_64, for example.
*/
class OffsetLink : public LinkImpl {
private:
    ChunkRef target;
    size_t offset;
public:
    OffsetLink(ChunkRef target, size_t offset, Link::LinkScope scope)
        : LinkImpl(scope), target(target), offset(offset) {}

    virtual ChunkRef getTarget() const { return target; }
    virtual address_t getTargetAddress() const;

    virtual bool isRIPRelative() const { return true; }
};


// --- special Chunk links ---

class PLTTrampoline;
class PLTLink : public LinkScopeDecorator<
    Link::SCOPE_WITHIN_MODULE, LinkDefaultAttributeDecorator<Link>> {
private:
    address_t originalAddress;
    PLTTrampoline *pltTrampoline;
public:
    PLTLink(address_t originalAddress, PLTTrampoline *pltTrampoline)
        : originalAddress(originalAddress), pltTrampoline(pltTrampoline) {}

    PLTTrampoline *getPLTTrampoline() const { return pltTrampoline; }
    virtual ChunkRef getTarget() const;
    virtual address_t getTargetAddress() const;

    virtual bool isRIPRelative() const { return true; }
};

class ExternalSymbol;
class ExternalSymbolLink : public LinkScopeDecorator<
    Link::SCOPE_EXTERNAL_DATA, LinkDefaultAttributeDecorator<Link>> {
private:
    ExternalSymbol *externalSymbol;
    address_t offset;
public:
    ExternalSymbolLink(ExternalSymbol *externalSymbol, address_t offset = 0)
        : externalSymbol(externalSymbol), offset(offset) {}

    ExternalSymbol *getExternalSymbol() const { return externalSymbol; }
    virtual ChunkRef getTarget() const;
    virtual address_t getTargetAddress() const;
    address_t getOffset() const { return offset; }

    virtual bool isRIPRelative() const { return false; }
};

class CopyRelocLink : public LinkScopeDecorator<
    Link::SCOPE_EXTERNAL_DATA, LinkDefaultAttributeDecorator<Link>> {
private:
    ExternalSymbol *externalSymbol;
public:
    CopyRelocLink(ExternalSymbol *externalSymbol)
        : externalSymbol(externalSymbol) {}

    ExternalSymbol *getExternalSymbol() const { return externalSymbol; }
    virtual ChunkRef getTarget() const;
    virtual address_t getTargetAddress() const;

    virtual bool isRIPRelative() const { return false; }
};

class JumpTable;
class JumpTableLink : public LinkScopeDecorator<
    Link::SCOPE_WITHIN_MODULE, LinkDefaultAttributeDecorator<Link>> {
private:
    JumpTable *jumpTable;
public:
    JumpTableLink(JumpTable *jumpTable) : jumpTable(jumpTable) {}

    virtual ChunkRef getTarget() const;
    virtual address_t getTargetAddress() const;

    // in position-independent code, always RIP-relative
    virtual bool isRIPRelative() const { return true; }
};

class Symbol;
class SymbolOnlyLink : public LinkScopeDecorator<
    Link::SCOPE_WITHIN_MODULE, LinkDefaultAttributeDecorator<Link>> {
private:
    Symbol *symbol;
    address_t target;
public:
    SymbolOnlyLink(Symbol *symbol, address_t target)
        : symbol(symbol), target(target) {}

    Symbol *getSymbol() const { return symbol; }
    virtual ChunkRef getTarget() const { return nullptr; }
    virtual address_t getTargetAddress() const { return target; }

    virtual bool isRIPRelative() const { return true; }
};

class EgalitoLoaderLink : public LinkScopeDecorator<
    Link::SCOPE_EXTERNAL_CODE, LinkDefaultAttributeDecorator<Link>> {
private:
    std::string targetName;
public:
    EgalitoLoaderLink(const std::string &name) : targetName(name) {}

    const std::string &getTargetName() const { return targetName; }
    virtual ChunkRef getTarget() const { return nullptr; }
    virtual address_t getTargetAddress() const;

    virtual bool isRIPRelative() const { return true; }
};

// Only used for executable generation
class LDSOLoaderLink : public LinkScopeDecorator<
    Link::SCOPE_EXTERNAL_CODE, LinkDefaultAttributeDecorator<Link>> {
private:
    std::string targetName;
public:
    LDSOLoaderLink(const std::string &name) : targetName(name) {}

    const std::string &getTargetName() const { return targetName; }
    virtual ChunkRef getTarget() const { return nullptr; }
    virtual address_t getTargetAddress() const { return 0; }

    virtual bool isRIPRelative() const { return true; }
};

class StackLink : public LinkScopeDecorator<
    Link::SCOPE_UNKNOWN, LinkDefaultAttributeDecorator<Link>> {
private:
    address_t targetAddress;
public:
    StackLink(address_t target) : targetAddress(target) {}

    virtual ChunkRef getTarget() const { return nullptr; }
    virtual address_t getTargetAddress() const { return targetAddress; }
};

class Marker;
class MarkerLinkBase : public LinkScopeDecorator<
    Link::SCOPE_UNKNOWN, LinkDefaultAttributeDecorator<Link>> {
private:
    Marker *marker;
public:
    MarkerLinkBase(Marker *marker) : marker(marker) {}

    Marker *getMarker() const { return marker; }
    virtual ChunkRef getTarget() const { return nullptr; }
    virtual address_t getTargetAddress() const;
};
class MarkerLink : public MarkerLinkBase {
public:
    using MarkerLinkBase::MarkerLinkBase;

    virtual bool isRIPRelative() const { return true; }
};
class AbsoluteMarkerLink : public MarkerLinkBase {
public:
    using MarkerLinkBase::MarkerLinkBase;

    virtual bool isAbsolute() const { return true; }
};

class GSTableEntry;
class GSTableLink : public LinkScopeDecorator<
    Link::SCOPE_UNKNOWN, LinkDefaultAttributeDecorator<Link>> {
private:
    GSTableEntry *entry;
public:
    GSTableLink(GSTableEntry *entry) : entry(entry) {}

    GSTableEntry *getEntry() const { return entry; }
    virtual ChunkRef getTarget() const;
    virtual address_t getTargetAddress() const;
};

class DistanceLink : public LinkScopeDecorator<
    Link::SCOPE_UNKNOWN, LinkDefaultAttributeDecorator<Link>> {
private:
    ChunkRef base;
    ChunkRef target;
public:
    DistanceLink(ChunkRef base, ChunkRef target) : base(base), target(target) {}
    virtual ChunkRef getTarget() const;
    virtual address_t getTargetAddress() const; // distance
};

// --- data links ---

class DataSection;
class DataOffsetLinkBase : public LinkImpl {
private:
    DataSection *section;
    address_t target;
    size_t addend;
public:
    DataOffsetLinkBase(DataSection *section, address_t target,
        Link::LinkScope scope) : LinkImpl(scope), section(section),
        target(target), addend(0) {}

    void setAddend(size_t addend) { this->addend = addend; }
    size_t getAddend() const { return this->addend; }
    virtual ChunkRef getTarget() const;
    virtual address_t getTargetAddress() const;
};
class DataOffsetLink : public DataOffsetLinkBase {
public:
    DataOffsetLink(DataSection *section, address_t target,
        Link::LinkScope scope = Link::LinkScope::SCOPE_UNKNOWN)
        : DataOffsetLinkBase(section, target, scope) {}

    virtual bool isRIPRelative() const { return true; }
};
class AbsoluteDataLink : public DataOffsetLinkBase {
public:
    AbsoluteDataLink(DataSection *section, address_t target,
        Link::LinkScope scope = Link::LinkScope::SCOPE_UNKNOWN)
        : DataOffsetLinkBase(section, target, scope) {}

    virtual bool isAbsolute() const { return true; }
};
class InternalAndExternalDataLink : public AbsoluteDataLink {
private:
    ExternalSymbol *externalSymbol;
public:
    InternalAndExternalDataLink(ExternalSymbol *externalSymbol,
        AbsoluteDataLink *other) : AbsoluteDataLink(*other),
        externalSymbol(externalSymbol) {}
    InternalAndExternalDataLink(ExternalSymbol *externalSymbol,
        DataSection *section, address_t target,
        Link::LinkScope scope = Link::LinkScope::SCOPE_UNKNOWN)
        : AbsoluteDataLink(section, target, scope), externalSymbol(externalSymbol) {}

    ExternalSymbol *getExternalSymbol() const { return externalSymbol; }
};

class TLSDataRegion;
class TLSDataOffsetLink : public LinkScopeDecorator<Link::SCOPE_WITHIN_MODULE,
    LinkDefaultAttributeDecorator<Link>> {
private:
    TLSDataRegion *tls;
    Symbol *symbol;
    address_t target;
public:
    TLSDataOffsetLink(TLSDataRegion *tls, Symbol *symbol, address_t target)
        : tls(tls), symbol(symbol), target(target) {}

    virtual ChunkRef getTarget() const;
    virtual address_t getTargetAddress() const;
    Symbol *getSymbol() const { return symbol; }
    TLSDataRegion *getTLSRegion() const { return tls; }
    void setTLSRegion(TLSDataRegion *tls) { this->tls = tls; }
    void setTarget(address_t target) { this->target = target; }
    address_t getRawTarget() const { return target; }
};

// --- other links ---

/** We know that this is a Link, but we're not sure what it points at yet.
*/
class UnresolvedLink : public LinkScopeDecorator<
    Link::SCOPE_UNKNOWN, LinkDefaultAttributeDecorator<Link>> {
private:
    address_t target;
public:
    UnresolvedLink(address_t target) : target(target) {}

    virtual ChunkRef getTarget() const { return nullptr; }
    virtual address_t getTargetAddress() const { return target; }
};

class UnresolvedRelativeLink : public LinkScopeDecorator<
    Link::SCOPE_UNKNOWN, LinkDefaultAttributeDecorator<Link>> {
private:
    address_t target;
public:
    UnresolvedRelativeLink(address_t target) : target(target) {}

    virtual ChunkRef getTarget() const { return nullptr; }
    virtual address_t getTargetAddress() const { return target; }

    virtual bool isRIPRelative() const { return true; }
};

/** Some x86_64 instructions can contain two links.

    Most Link-processing code for Instructions needs to handle this case
    specially.
*/
class ImmAndDispLink : public LinkDefaultAttributeDecorator<Link> {
private:
    NormalLinkBase *immLink;
    Link *dispLink;
public:
    ImmAndDispLink(NormalLinkBase *immLink, Link *dispLink)
        : immLink(immLink), dispLink(dispLink) {}
    NormalLinkBase *getImmLink() const { return immLink; }
    Link *getDispLink() const { return dispLink; }

    virtual ChunkRef getTarget() const { throw "ImmAndDispLink not handled"; }
    virtual address_t getTargetAddress() const { throw "ImmAndDispLink not handled"; }

    virtual Link::LinkScope getScope() const
        { throw "ImmAndDispLink not handled"; }
    virtual bool isExternalJump() const { throw "ImmAndDispLink not handled"; }
    virtual bool isWithinModule() const { throw "ImmAndDispLink not handled"; }
};


// --- link factory ---

class Module;
class LinkFactory {
public:
    static Link *makeNormalLink(ChunkRef target, bool isRelative = true,
        bool isExternal = false);
    static Link *makeDataLink(Module *module, address_t target,
        bool isRelative = true);
    static Link *makeMarkerLink(Module *module, Symbol *symbol, size_t addend,
        bool isRelative);
    static Link *makeInferredMarkerLink(Module *module, address_t address,
        bool isRelative);
};

#endif
