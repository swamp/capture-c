/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <stdio.h>
#include <swamp-dump/dump.h>
#include <swamp-runtime/allocator.h>

#include <swamp-typeinfo/chunk.h>
#include <swamp-typeinfo/deserialize.h>
#include <swamp-typeinfo/typeinfo.h>

#include <flood/in_stream.h>
#include <flood/out_stream.h>

#include <swamp-capture/in_capture.h>
#include <swamp-capture/out_capture.h>

clog_config g_clog;

static void tyran_log_implementation(enum clog_type type, const char* string)
{
    (void) type;
    fprintf(stderr, "%s\n", string);
}

int rtti(SwtiChunk* chunk)
{
    const uint8_t octets[] = {
        0,
        1,
        2,
        0x09,
        SwtiTypeInt,
        SwtiTypeList,
        0x05,
        SwtiTypeArray,
        0x05,
        SwtiTypeAlias,
        0x4,
        'C',
        'o',
        'o',
        'l',
        4,
        SwtiTypeRecord,
        4,
        1,
        't',
        8,
        4,
        'n',
        'a',
        'm',
        'e',
        6,
        3,
        'p',
        'o',
        's',
        5,
        2,
        'a',
        'r',
        1,
        SwtiTypeRecord,
        2,
        1,
        'x',
        0,
        1,
        'y',
        0,
        SwtiTypeString,
        SwtiTypeFunction,
        2,
        1,
        2,
        SwtiTypeBoolean,
    };

    int error = swtiDeserialize(octets, sizeof(octets), chunk);
    if (error < 0) {
        CLOG_ERROR("deserialize problem");
        return error;
    }

    return 0;
}

static const swamp_value* createStateValue(swamp_allocator* allocator)
{
    swamp_struct* tempStruct = swamp_allocator_alloc_struct(allocator, 4);

    //    const swamp_value *v = swamp_allocator_alloc_integer(&allocator, 42);

    const swamp_value* b = swamp_allocator_alloc_boolean(allocator, 1);

    swamp_struct* posStruct = swamp_allocator_alloc_struct(allocator, 2);
    const swamp_value* x = swamp_allocator_alloc_integer(allocator, 65);
    const swamp_value* y = swamp_allocator_alloc_integer(allocator, 120);
    posStruct->fields[0] = x;
    posStruct->fields[1] = y;

    const swamp_value* str = swamp_allocator_alloc_string(allocator, "hello");

    swamp_struct* ar = swamp_allocator_alloc_struct(allocator, 1);
    ar->fields[0] = posStruct;

    swamp_struct* posStruct2 = swamp_allocator_alloc_struct(allocator, 2);
    const swamp_value* x2 = swamp_allocator_alloc_integer(allocator, 41);
    const swamp_value* y2 = swamp_allocator_alloc_integer(allocator, -1250);
    posStruct2->fields[0] = x2;
    posStruct2->fields[1] = y2;

    swamp_list* lr = swamp_allocator_alloc_list_create(allocator, &posStruct2, 1);

    //    swamp_enum* custom = swamp_allocator_alloc_enum(allocator, 1, 1);
    //    const swamp_value *maybeParamInt = swamp_allocator_alloc_integer(allocator, 99);
    // custom->fields[0] = maybeParamInt;

    swamp_enum* custom = swamp_allocator_alloc_enum(allocator, 0, 0);

    tempStruct->fields[0] = b;
    tempStruct->fields[1] = str;
    tempStruct->fields[2] = posStruct;
    tempStruct->fields[3] = lr;

    return tempStruct;
}

static const swamp_value* createInputValue(swamp_allocator* allocator, int x, int y)
{
    swamp_struct* posStruct = swamp_allocator_alloc_struct(allocator, 2);

    const swamp_value* xValue = swamp_allocator_alloc_integer(allocator, x);
    const swamp_value* yValue = swamp_allocator_alloc_integer(allocator, y);
    posStruct->fields[0] = xValue;
    posStruct->fields[1] = yValue;

    return posStruct;
}

int writeCapture(SwtiChunk* chunk, struct swamp_allocator* allocator)
{
    uint8_t captureOut[4096];
    FldOutStream outStream;
    fldOutStreamInit(&outStream, captureOut, 4096);

    SwampOutCapture outCapture;

    const SwtiType* stateType = chunk->types[4];
    const SwtiType* inputType = chunk->types[5];

    const int verbosity = 1;
    swampOutCaptureInit(&outCapture, &outStream, &chunk, stateType, inputType, verbosity);

    uint32_t simulationFrame = 0;
    const swamp_value* stateValue = createStateValue(allocator);
    swampOutCaptureAddState(&outCapture, simulationFrame, stateValue);

    for (size_t i = 0; i < 10; ++i) {
        const swamp_value* inputValue = createInputValue(allocator, i * 10, i * 11);
        swampOutCaptureAddInput(&outCapture, simulationFrame, inputValue);
        simulationFrame++;
    }

    swampOutCaptureClose(&outCapture);

    FILE* fp = fopen("test.swamp-capture", "wb");
    fwrite(outCapture.outStream->octets, outCapture.outStream->pos, 1, fp);
    fclose(fp);

    return 0;
}

static int readNext(SwampInCapture* inCapture, struct swamp_allocator* allocator)
{
    uint8_t command;
    uint32_t encounteredSimulationFrame;
    int errorCode = swampInCaptureReadCommand(inCapture, &command, &encounteredSimulationFrame);
    if (errorCode < 0) {
        return errorCode;
    }
    if (swampInCaptureCmdIsEnd(command)) {
        return 0xff;
    }

    if (swampInCaptureCmdIsState(command)) {
        const swamp_value* stateValue = 0;
        swampInCaptureReadState(inCapture, allocator, &stateValue);
    } else if (swampInCaptureCmdIsInput(command)) {
        const swamp_value* inputValue = 0;
        swampInCaptureReadInput(inCapture, allocator, &inputValue);
    }

    return 0;
}

static int readCapture(SwampInCapture* inCapture, const char* filename, const SwtiType* stateType,
                       const SwtiType* inputType, struct swamp_allocator* allocator)
{
    FILE* fp = fopen(filename, "rb");
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t* data = malloc(fileSize);
    fread(data, 1, fileSize, fp);
    fclose(fp);

    FldInStream inStream;
    fldInStreamInit(&inStream, data, fileSize);

    uint32_t startTime;
    int errorCode = swampInCaptureInit(inCapture, &inStream, &startTime, stateType, inputType, 1);
    if (errorCode < 0) {
        return errorCode;
    }

    int code;
    while ((code = readNext(inCapture, allocator)) != 0xff) {
    }

    return 0;
}

int writeReadBackTest(swamp_allocator* allocator)
{
    SwtiChunk chunk;
    rtti(&chunk);

    writeCapture(&chunk, &allocator);

    CLOG_INFO("----------------");

    SwampInCapture inCapture;
    inCapture.verbosity = 1;

    swtiChunkDebugOutput(&chunk, "example: before");

    const SwtiType* stateType = chunk.types[4];
    const SwtiType* inputType = chunk.types[5]; // Position(x, y)

    readCapture(&inCapture, "test.swamp-capture", stateType, inputType, &allocator);

    swtiChunkDebugOutput(&inCapture.typeInformationChunk, "example: read back");

    return 0;
}

int main(int argc, char* argv[])
{
    g_clog.log = tyran_log_implementation;

    if (argc <= 1) {
        CLOG_OUTPUT("usage: capture file");
        return -1;
    }

    swamp_allocator allocator;
    swamp_allocator_init(&allocator);

    // writeReadBackTest(&allocator);

    SwampInCapture inCapture2;
    inCapture2.verbosity = 1;
    readCapture(&inCapture2, argv[1], 0, 0, &allocator);

    return 0;
}
