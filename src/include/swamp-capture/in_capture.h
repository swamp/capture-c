/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef SWAMP_IN_CAPTURE_H
#define SWAMP_IN_CAPTURE_H

#include <stdint.h>
#include <swamp-typeinfo/chunk.h>
struct SwtiChunk;
struct SwtiType;
struct FldInStream;
struct ImprintAllocator;


typedef struct SwampInCapture {
    struct FldInStream* inStream;
    const struct SwtiChunk typeInformationChunk;
    const struct SwtiType* stateType;
    const struct SwtiType* inputType;
    uint32_t expectingSimulationFrame;
    uint8_t lastCommand;
    int verbosity;
} SwampInCapture;

int swampInCaptureCmdIsEnd(uint8_t cmd);
int swampInCaptureCmdIsState(uint8_t cmd);
int swampInCaptureCmdIsInput(uint8_t cmd);

int swampInCaptureInit(SwampInCapture* self, struct FldInStream* outStream, uint64_t* startTime,
                       const struct SwtiType* stateType, const struct SwtiType* inputType, int verbosity, struct ImprintAllocator* allocator);

int swampInCaptureReadCommand(struct SwampInCapture* self, uint8_t* outCommand, uint32_t* outSimulationFrame);

int swampInCaptureReadState(SwampInCapture* self,
                            const void** stateValue);

int swampInCaptureReadInput(SwampInCapture* self,
                            const void** inputValue);

void swampInCaptureClose(SwampInCapture* self);

#endif
