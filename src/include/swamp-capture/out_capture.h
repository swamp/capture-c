/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef SWAMP_OUT_CAPTURE_H
#define SWAMP_OUT_CAPTURE_H

#include <stdint.h>

#include <swamp-typeinfo/chunk.h>

struct SwtiType;
struct FldOutStream;
struct ImprintAllocator;


typedef struct SwampOutCapture {
    struct FldOutStream* outStream;
    const struct SwtiChunk typeInformationChunk;
    const struct SwtiType* stateType;
    const struct SwtiType* inputType;
    uint32_t lastSimulationFrame;
    int verbosity;
} SwampOutCapture;

int swampOutCaptureInit(SwampOutCapture* self, struct FldOutStream* outStream, uint32_t startTime,
                        const struct SwtiType* stateType, const struct SwtiType* inputType, int verbosity, struct ImprintAllocator* allocator);

int swampOutCaptureAddState(SwampOutCapture* self, uint32_t simulationFrame, const void* stateValue);

int swampOutCaptureAddInput(SwampOutCapture* self, uint32_t simulationFrame, const void* inputValue);

void swampOutCaptureClose(SwampOutCapture* self);

#endif
