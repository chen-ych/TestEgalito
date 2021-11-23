#ifndef EGALITO_DWARF_CURSOR_H
#define EGALITO_DWARF_CURSOR_H

#include "types.h"
#include "defines.h"

class DwarfCursor {
private:
    address_t start;
    address_t cursor;
public:
    DwarfCursor(address_t start) : start(start), cursor(start) {} 
    address_t getStart() const { return start; }
    address_t getCursor() const { return cursor; }
    size_t getOffset() const { return cursor - start; }

    int64_t nextSleb128();
    uint64_t nextUleb128();
    uint8_t *nextString();
    void skip(size_t bytes) { this->cursor += bytes; }

    bool operator < (const DwarfCursor &other) const
        { return cursor < other.cursor; }

    template <typename ElementType>
    DwarfCursor &operator >> (ElementType &e) {
        e = next<ElementType>();
        return *this;
    }

    template <typename ElementType>
    ElementType nextEncodedPointer(uint8_t encoding) {
        return static_cast<ElementType>(parseNextEncodedPointer(encoding));
    }

    template <typename ElementType>
    ElementType next() {
        auto e = get<ElementType>();
        cursor += sizeof(ElementType);
        return e;
    }
private:
    int64_t parseNextEncodedPointer(uint8_t encoding);

    template <typename ElementType>
    ElementType get() const {
        return *reinterpret_cast<ElementType *>(cursor);
    }
};

#endif
