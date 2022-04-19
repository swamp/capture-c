/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <flood/out_stream.h>
#include <raff/write.h>
#include <swamp-capture/out_capture.h>
#include <swamp-dump/dump_ascii.h>
#include <swamp-typeinfo/chunk.h>
#include <swamp-typeinfo/add.h>
#include <swamp-typeinfo/equal.h>
#include <swamp-typeinfo-serialize/serialize.h>
#include <swamp-snapshot/write_typeinfo.h>
#include <tiny-libc/tiny_libc.h>
#include <clog/clog.h>
#include <swamp-dump/dump.h>

static int writeCaptureChunk(SwampOutCapture* self, uint32_t startTime, uint8_t stateRef, uint8_t inputRef)
{
    RaffTag icon = {0xF0, 0x9F, 0x8E, 0xA5};
    RaffTag name = {'s', 'c', 'a', '1'};
    int octets = raffWriteChunkHeader(self->outStream->p, self->outStream->size - self->outStream->pos, icon, name,
                                      2 + sizeof(startTime));
    if (octets < 0) {
        return octets;
    }
    self->outStream->p += octets;
    self->outStream->pos += octets;

    fldOutStreamWriteUInt8(self->outStream, stateRef);
    fldOutStreamWriteUInt8(self->outStream, inputRef);
    fldOutStreamWriteUInt32(self->outStream, startTime);

    return octets;
}



static int writeStateAndInputHeader(FldOutStream* stream)
{
    RaffTag icon = {0xF0, 0x9F, 0x93, 0xB7};
    RaffTag name = {'s', 'i', 's', '0'};
    int octets = raffWriteChunkHeader(stream->p, stream->size - stream->pos, icon, name, 0);
    if (octets < 0) {
        return octets;
    }
    stream->p += octets;
    stream->pos += octets;

    return 0;
}

static void writeHeader(SwampOutCapture* self)
{
    int octetsWritten = raffWriteHeader(self->outStream->p, self->outStream->size - self->outStream->pos);
    self->outStream->p += octetsWritten;
    self->outStream->pos += octetsWritten;
}

static const uint32_t lastFrame = 0xffffffff;
static const uint8_t endOfStream = 0xff;
static const uint8_t completeStateFollows = 0xfe;

int swampOutCaptureInit(SwampOutCapture* self, struct FldOutStream* outStream, uint32_t startTime,
                        const struct SwtiType* stateType, const struct SwtiType* inputType, int verbosity, struct ImprintAllocator* allocator)
{
    self->outStream = outStream;

    SwtiChunk tempChunk;
    swtiChunkInit(&tempChunk, 0, 0, allocator);
    tempChunk.maxCount = 256;
    tempChunk.types = tc_malloc_type_count(const SwtiType*, 256);

    int stateIndex;
    if ((stateIndex = swtiChunkAddType(&tempChunk, stateType, allocator)) < 0) {
        return stateIndex;
    }

    int inputIndex;
    if ((inputIndex = swtiChunkAddType(&tempChunk, inputType, allocator)) < 0) {
        return inputIndex;
    }

    swtiChunkInit((SwtiChunk*) &self->typeInformationChunk, tempChunk.types, tempChunk.typeCount, allocator);
    // HACK
    SwtiChunk* p = (SwtiChunk*) &self->typeInformationChunk;
    p->maxCount = tempChunk.typeCount;
    p->typeCount = tempChunk.typeCount;
    p->types = tempChunk.types;
    // swtiChunkDestroy(&tempChunk);

    const SwtiType* copiedStateType = swtiChunkTypeFromIndex(&self->typeInformationChunk, stateIndex);
    const SwtiType* copiedInputType = swtiChunkTypeFromIndex(&self->typeInformationChunk, inputIndex);

    if (swtiTypeEqual(stateType, copiedStateType) != 0) {
        CLOG_ERROR("internal error. couldn't copy stateType")
        return -4;
    }

    if (swtiTypeEqual(inputType, copiedInputType) != 0) {
        CLOG_ERROR("internal error. couldn't copy inputType")
        return -5;
    }

    self->stateType = copiedStateType;
    self->inputType = copiedInputType;
    self->verbosity = verbosity;
    self->lastSimulationFrame = lastFrame;

    writeHeader(self);
    int stateRef = copiedStateType->index;
    int inputRef = copiedInputType->index;

    writeCaptureChunk(self, startTime, stateRef, inputRef);

    swsnWriteTypeInformationChunk(self->outStream, &self->typeInformationChunk);

    writeStateAndInputHeader(self->outStream);

    return 0;
}

int swampOutCaptureAddState(SwampOutCapture* self, uint32_t simulationFrame, const void* stateValue)
{
    fldOutStreamWriteUInt8(self->outStream, completeStateFollows);
    fldOutStreamWriteUInt32(self->outStream, simulationFrame);
    int tell = self->outStream->pos;
    int errorCode = swampDumpToOctets(self->outStream, stateValue, self->stateType);
    if (errorCode < 0) {
        return errorCode;
    }
    int octetCount = self->outStream->pos - tell;

    if (self->verbosity) {
        char temp[1024];
        fprintf(stderr, "%08X: write state: octetCount:%d %s\n", simulationFrame, octetCount,
                swampDumpToAsciiString(stateValue, self->stateType, 0, temp, 1024));
    }

    return 0;
}

int swampOutCaptureAddInput(SwampOutCapture* self, uint32_t simulationFrame, const void* inputValue)
{
    uint8_t waitFrameCount;

    if (self->lastSimulationFrame == lastFrame) {
        waitFrameCount = 0;
    } else if (simulationFrame > self->lastSimulationFrame) {
        int delta = simulationFrame - self->lastSimulationFrame - 1;
        if (delta >= 0xff) {
            CLOG_SOFT_ERROR("simulation frame is way off: %d vs %d", simulationFrame, self->lastSimulationFrame);
            return -1;
        }
        waitFrameCount = 0;
    } else {
        return -3;
    }

    fldOutStreamWriteUInt8(self->outStream, waitFrameCount);

    int tell = self->outStream->pos;
    int errorCode = swampDumpToOctets(self->outStream, inputValue, self->inputType);
    if (errorCode != 0) {
        return errorCode;
    }
    int octetCount = self->outStream->pos - tell;

    self->lastSimulationFrame = simulationFrame;

    if (self->verbosity) {
        char temp[1024];
        fprintf(stderr, "%08X: write input: octetCount:%d %s\n", simulationFrame, octetCount,
                swampDumpToAsciiString(inputValue, self->inputType, 0, temp, 1024));
    }

    return 0;
}

void swampOutCaptureClose(SwampOutCapture* self)
{
    fldOutStreamWriteUInt8(self->outStream, endOfStream);
}
