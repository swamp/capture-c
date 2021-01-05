/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <flood/out_stream.h>
#include <raff/write.h>
#include <swamp-capture/out_capture.h>
#include <swamp-dump/dump.h>
#include <swamp-dump/dump_ascii.h>
#include <swamp-typeinfo/chunk.h>
#include <swamp-typeinfo/consume.h>
#include <swamp-typeinfo/deep_equal.h>
#include <swamp-typeinfo/serialize.h>
#include <tiny-libc/tiny_libc.h>

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

static int writeTypeInformationChunk(FldOutStream* stream, const SwtiChunk* chunk)
{
    uint8_t typeInformationOctets[1024];

    int payloadCount = swtiSerialize(typeInformationOctets, 1024, chunk);
    if (payloadCount < 0) {
        return payloadCount;
    }

    RaffTag icon = {0xF0, 0x9F, 0x93, 0x9C};
    RaffTag name = {'s', 't', 'i', '0'};
    int octets = raffWriteChunkHeader(stream->p, stream->size - stream->pos, icon, name, payloadCount);
    if (octets < 0) {
        return octets;
    }
    stream->p += octets;
    stream->pos += octets;

    fldOutStreamWriteOctets(stream, typeInformationOctets, payloadCount);

    return octets + payloadCount;
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
                        const struct SwtiType* stateType, const struct SwtiType* inputType, int verbosity)
{
    self->outStream = outStream;

    SwtiChunk tempChunk;
    swtiChunkInit(&tempChunk, 0, 0);
    tempChunk.maxCount = 256;
    tempChunk.types = tc_malloc_type_count(const SwtiType*, 256);

    int stateIndex;
    if ((stateIndex = swtiTypeConsume(&tempChunk, stateType)) < 0) {
        return stateIndex;
    }

    int inputIndex;
    if ((inputIndex = swtiTypeConsume(&tempChunk, inputType)) < 0) {
        return inputIndex;
    }

    swtiChunkInit((SwtiChunk*) &self->typeInformationChunk, tempChunk.types, tempChunk.typeCount);
    // HACK
    SwtiChunk* p = (SwtiChunk*) &self->typeInformationChunk;
    p->maxCount = tempChunk.typeCount;
    p->typeCount = tempChunk.typeCount;
    p->types = tempChunk.types;
    // swtiChunkDestroy(&tempChunk);

    const SwtiType* copiedStateType = swtiChunkTypeFromIndex(&self->typeInformationChunk, stateIndex);
    const SwtiType* copiedInputType = swtiChunkTypeFromIndex(&self->typeInformationChunk, inputIndex);

    if (swtiTypeDeepEqual(stateType, copiedStateType) != 0) {
        return -4;
    }

    if (swtiTypeDeepEqual(inputType, copiedInputType) != 0) {
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

    writeTypeInformationChunk(self->outStream, &self->typeInformationChunk);

    writeStateAndInputHeader(self->outStream);

    return 0;
}

int swampOutCaptureAddState(SwampOutCapture* self, uint32_t simulationFrame, const struct swamp_value* stateValue)
{
    fldOutStreamWriteUInt8(self->outStream, completeStateFollows);
    fldOutStreamWriteUInt32(self->outStream, simulationFrame);
    int tell = self->outStream->pos;
    int errorCode = swampDumpToOctets(self->outStream, stateValue, self->stateType);
    if (errorCode != 0) {
        return errorCode;
    }
    int octetCount = self->outStream->pos - tell;

    if (self->verbosity) {
        char temp[1024];
        fprintf(stderr, "%08X: write state: octetCount:%d %s\n", simulationFrame, octetCount,
                swampDumpToAsciiString(stateValue, self->stateType, temp, 1024));
    }

    return 0;
}

int swampOutCaptureAddInput(SwampOutCapture* self, uint32_t simulationFrame, const struct swamp_value* inputValue)
{
    uint8_t waitFrameCount;

    if (self->lastSimulationFrame == lastFrame) {
        waitFrameCount = 0;
    } else if (simulationFrame > self->lastSimulationFrame) {
        int delta = simulationFrame - self->lastSimulationFrame - 1;
        if (delta >= 0xff) {
            return -1;
        }
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
                swampDumpToAsciiString(inputValue, self->inputType, temp, 1024));
    }

    return 0;
}

void swampOutCaptureClose(SwampOutCapture* self)
{
    fldOutStreamWriteUInt8(self->outStream, endOfStream);
}