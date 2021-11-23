#ifndef EGALITO_ARCHIVE_CHUNK_TYPES_H
#define EGALITO_ARCHIVE_CHUNK_TYPES_H

#include <cstdint>

enum EgalitoChunkType {
    TYPE_UNKNOWN = 0,
    TYPE_Program,
    TYPE_Module,
    TYPE_FunctionList,
    TYPE_PLTList,
    TYPE_JumpTableList,
    TYPE_DataRegionList,
    TYPE_InitFunctionList,
    TYPE_ExternalSymbolList,
    TYPE_LibraryList,
    TYPE_VTableList,
    TYPE_Function,
    TYPE_Block,
    TYPE_Instruction,
    TYPE_PLTTrampoline,
    TYPE_JumpTable,
    TYPE_JumpTableEntry,
    TYPE_DataRegion,
    TYPE_TLSDataRegion,
    TYPE_DataSection,
    TYPE_DataVariable,
    TYPE_GlobalVariable,
    TYPE_MarkerList,
    TYPE_Marker,
    TYPE_VTable,
    TYPE_VTableEntry,
    TYPE_InitFunction,
    TYPE_ExternalSymbol,
    TYPE_Library,
    TYPE_TOTAL
};

uint8_t encodeChunkType(EgalitoChunkType type);
EgalitoChunkType decodeChunkType(uint8_t encoded);
const char *getChunkTypeName(EgalitoChunkType type);

#endif
