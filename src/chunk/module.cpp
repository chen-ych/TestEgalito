#include <string>
#include "module.h"
#include "library.h"
#include "initfunction.h"
#include "elf/elfspace.h"
#include "elf/sharedlib.h"
#include "serializer.h"
#include "visitor.h"
#include "util/streamasstring.h"
#include "log/log.h"

void Module::setElfSpace(ElfSpace *elfSpace) {
    this->elfSpace = elfSpace;

    // set name based on ElfSpace
    setName(StreamAsString() << "module-" << elfSpace->getName());
}

void Module::setLibrary(Library *library) {
    this->library = library;

    // set name based on Library
    setName(StreamAsString() << "module-" << library->getName());
}

void Module::serialize(ChunkSerializerOperations &op,
    ArchiveStreamWriter &writer) {

    writer.writeString(getName());
    writer.writeID(op.assign(library));

    auto pltListID = op.serialize(getPLTList());
    writer.write(pltListID);

    auto functionListID = op.serialize(getFunctionList());
    writer.write(functionListID);

    auto jumpTableListID = op.serialize(getJumpTableList());
    writer.write(jumpTableListID);

    auto dataRegionListID = op.serialize(getDataRegionList());
    writer.write(dataRegionListID);

    auto vtableListID = getVTableList() ? op.serialize(getVTableList()) : FlatChunk::NoneID;
    writer.write(vtableListID);

    auto initFunctionListID = getInitFunctionList() ? op.serialize(getInitFunctionList()) : FlatChunk::NoneID;
    writer.write(initFunctionListID);

    auto finiFunctionListID = getFiniFunctionList() ? op.serialize(getFiniFunctionList()) : FlatChunk::NoneID;
    writer.write(finiFunctionListID);

    auto externalSymbolListID = op.serialize(getExternalSymbolList());
    writer.write(externalSymbolListID);

    if(op.isLocalModuleOnly() && library) {
        LOG(0, "serializing library [" << library->getName() << "]");
        op.serialize(library, op.assign(library));
    }
}

bool Module::deserialize(ChunkSerializerOperations &op,
    ArchiveStreamReader &reader) {

    setName(reader.readString());
    library = op.lookupAs<Library>(reader.readID());

    LOG(1, "trying to parse Module [" << name << "]");

    {
        auto id = reader.readID();
        auto pltList = op.lookupAs<PLTList>(id);
        getChildren()->add(pltList);
        setPLTList(pltList);
        pltList->setParent(this);
    }

    {
        auto id = reader.readID();
        auto functionList = op.lookupAs<FunctionList>(id);
        getChildren()->add(functionList);
        setFunctionList(functionList);
    }

    {
        auto id = reader.readID();
        auto jumpTableList = op.lookupAs<JumpTableList>(id);
        if(jumpTableList) {
            getChildren()->add(jumpTableList);
            setJumpTableList(jumpTableList);
        }
    }

    {
        auto id = reader.readID();
        auto dataRegionList = op.lookupAs<DataRegionList>(id);
        if(dataRegionList) {
            getChildren()->add(dataRegionList);
            setDataRegionList(dataRegionList);
        }
    }

    {
        auto id = reader.readID();
        auto vtableList = op.lookupAs<VTableList>(id);
        if(vtableList) {
            getChildren()->add(vtableList);
            setVTableList(vtableList);
        }
    }

    {
        auto id = reader.readID();
        auto initFunctionList = op.lookupAs<InitFunctionList>(id);
        if(initFunctionList) {
            getChildren()->add(initFunctionList);
            setInitFunctionList(initFunctionList);
        }
    }

    {
        auto id = reader.readID();
        auto finiFunctionList = op.lookupAs<InitFunctionList>(id);
        if(finiFunctionList) {
            getChildren()->add(finiFunctionList);
            setFiniFunctionList(finiFunctionList);
        }
    }

    {
        auto id = reader.readID();
        auto externalSymbolList = op.lookupAs<ExternalSymbolList>(id);
        if(externalSymbolList) {
            getChildren()->add(externalSymbolList);
            setExternalSymbolList(externalSymbolList);
        }
    }

    return reader.stillGood();
}

void Module::accept(ChunkVisitor *visitor) {
    visitor->visit(this);
}
