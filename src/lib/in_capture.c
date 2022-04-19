/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <raff/raff.h>
#include <raff/tag.h>
#include <swamp-capture/in_capture.h>
#include <swamp-snapshot/read_typeinfo.h>
#include <swamp-dump/dump_ascii.h>

#include <swamp-runtime/types.h>
#include <swamp-typeinfo/chunk.h>
#include <swamp-typeinfo/equal.h>
#include <swamp-typeinfo-serialize/deserialize.h>

static int readCaptureChunk(SwampInCapture* self, uint8_t* stateRef, uint8_t* inputRef, uint64_t* startTime)
{
    RaffTag foundIcon, foundName;
    uint32_t foundChunkSize;
    int octets = raffReadChunkHeader(self->inStream->p, self->inStream->size - self->inStream->pos, foundIcon,
                                     foundName, &foundChunkSize);
    if (octets < 0) {
        return octets;
    }
    self->inStream->p += octets;
    self->inStream->pos += octets;

    if (foundChunkSize != 2 + 4) {
        return -2;
    }

    RaffTag expectedIcon = {0xF0, 0x9F, 0x8E, 0xA5};
    RaffTag expectedName = {'s', 'c', 'a', '1'};

    if (!raffTagEqual(foundIcon, expectedIcon)) {
        return -3;
    }

    if (!raffTagEqual(foundName, expectedName)) {
        return -4;
    }

    fldInStreamReadUInt8(self->inStream, stateRef);
    fldInStreamReadUInt8(self->inStream, inputRef);
    uint32_t shortStartTime;

    fldInStreamReadUInt32(self->inStream, &shortStartTime);
    *startTime = shortStartTime;
    return octets;
}


static int readStateAndInputChunk(SwampInCapture* self)
{
    RaffTag foundIcon, foundName;
    uint32_t foundChunkSize;
    int octets = raffReadChunkHeader(self->inStream->p, self->inStream->size - self->inStream->pos, foundIcon,
                                     foundName, &foundChunkSize);
    if (octets < 0) {
        return octets;
    }
    self->inStream->p += octets;
    self->inStream->pos += octets;

    if (foundChunkSize != 0) {
        return -2;
    }

    RaffTag expectedIcon = {0xF0, 0x9F, 0x93, 0xB7};
    RaffTag expectedName = {'s', 'i', 's', '0'};

    if (!raffTagEqual(foundIcon, expectedIcon)) {
        return -3;
    }

    if (!raffTagEqual(foundName, expectedName)) {
        return -4;
    }

    return octets;
}

static int readHeader(SwampInCapture* self)
{
    int octetsWritten = raffReadAndVerifyHeader(self->inStream->p, self->inStream->size - self->inStream->pos);
    if (octetsWritten < 0) {
        return octetsWritten;
    }
    self->inStream->p += octetsWritten;
    self->inStream->pos += octetsWritten;
    return octetsWritten;
}

const static uint32_t lastFrame = 0xffffffff;
const static uint8_t endOfStream = 0xff;
const static uint8_t completeStateFollows = 0xfe;

int swampInCaptureCmdIsEnd(uint8_t cmd)
{
    return cmd == endOfStream;
}

int swampInCaptureCmdIsState(uint8_t cmd)
{
    return cmd == completeStateFollows;
}

int swampInCaptureCmdIsInput(uint8_t cmd)
{
    return cmd < completeStateFollows;
}

int swampInCaptureInit(SwampInCapture* self, struct FldInStream* inStream, uint64_t* startTime,
                       const struct SwtiType* expectedStateType, const struct SwtiType* expectedInputType,
                       int verbosity, struct ImprintAllocator* allocator)
{
    self->inStream = inStream;
    self->verbosity = verbosity;
    self->expectingSimulationFrame = lastFrame;

    swtiChunkInit((SwtiChunk*) &self->typeInformationChunk, 0, 0);

    int errorCode = readHeader(self);
    if (errorCode < 0) {
        return errorCode;
    }

    uint8_t foundStateRef;
    uint8_t foundInputRef;
    errorCode = readCaptureChunk(self, &foundStateRef, &foundInputRef, startTime);
    if (errorCode < 0) {
        return errorCode;
    }

    errorCode = swsnReadTypeInformationChunk(self->inStream, (SwtiChunk*) &self->typeInformationChunk, allocator);
    if (errorCode < 0) {
        return errorCode;
    }

    if (verbosity > 2) {
        swtiChunkDebugOutput(&self->typeInformationChunk, 0, "read in capture");
    }

    const SwtiType* deserializedStateType = swtiChunkTypeFromIndex(&self->typeInformationChunk, foundStateRef);
    const SwtiType* deserializedInputType = swtiChunkTypeFromIndex(&self->typeInformationChunk, foundInputRef);

    if (expectedStateType != 0) {
        if (swtiTypeEqual(expectedStateType, deserializedStateType) != 0) {
            return -4;
        }
    }

    if (expectedInputType != 0) {
        if (swtiTypeEqual(expectedInputType, deserializedInputType) != 0) {
            return -5;
        }
    }

    self->stateType = deserializedStateType;
    self->inputType = deserializedInputType;

    return readStateAndInputChunk(self);
}

int swampInCaptureReadCommand(struct SwampInCapture* self, uint8_t* outCommand, uint32_t* outSimulationFrame)
{
    uint8_t command;
    int errorCode = fldInStreamReadUInt8(self->inStream, &command);
    if (errorCode < 0) {
        return -1;
    }

    self->lastCommand = command;

    if (command == 0xff) {
        if (self->verbosity) {
            CLOG_DEBUG("> ** read command: end of stream **")
        }
        *outSimulationFrame = 0xffffffff;
        *outCommand = 0xff;
        return command;
    }

    if (command == 0xfe) {
        errorCode = fldInStreamReadUInt32(self->inStream, outSimulationFrame);
        if (self->verbosity) {
            CLOG_DEBUG("> %08X: read command: full state follows", *outSimulationFrame)
        }
        self->expectingSimulationFrame = *outSimulationFrame;
        *outCommand = command;
        return errorCode;
    }

    uint8_t deltaWaitFrame = command;

    self->expectingSimulationFrame += deltaWaitFrame;

    if (self->verbosity) {
        CLOG_INFO("> %08X: read command: input  (delta:%d)", self->expectingSimulationFrame, deltaWaitFrame)
    }
    *outSimulationFrame = self->expectingSimulationFrame;
    *outCommand = command;
    self->expectingSimulationFrame++;

    return command;
}

int swampInCaptureReadState(SwampInCapture* self, const void** stateValue)
{
    if (self->lastCommand != 0xfe) {
        return -4;
    }
    void* creator = 0;
    void* context = 0;
    int errorCode = 0; // TODO Fix this: swampDumpFromOctets(self->inStream, self->stateType, creator, context, stateValue);
    if (errorCode != 0) {
        return errorCode;
    }

    if (self->verbosity) {
        char temp[1024];
        fprintf(stderr, "%08X: read state: %s\n", self->expectingSimulationFrame,
                swampDumpToAsciiString(*stateValue, self->stateType, 0, temp, 1024));
    }

    return 0;
}

int swampInCaptureReadInput(SwampInCapture* self, const void** inputValue)
{
    if (self->lastCommand >= 0xfe) {
        return -4;
    }

    int errorCode = 0; // TODO: Fix this: swampDumpFromOctets(self->inStream, self->inputType, 0, 0, inputValue);
    if (errorCode != 0) {
        return errorCode;
    }

    if (self->verbosity) {
        char temp[1024];
        fprintf(stderr, "%08X: read input: %s\n", self->expectingSimulationFrame - 1,
                swampDumpToAsciiString(*inputValue, self->inputType, 0, temp, 1024));
    }

    return 0;
}

void swampInCaptureClose(SwampInCapture* self)
{
}
