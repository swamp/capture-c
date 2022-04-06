/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <clog/console.h>
#include <stdio.h>
#include <swamp-dump/dump.h>

#include <swamp-typeinfo/chunk.h>
#include <swamp-typeinfo-serialize/deserialize.h>
#include <swamp-typeinfo/typeinfo.h>

#include <flood/in_stream.h>
#include <flood/out_stream.h>

#include <swamp-capture/in_capture.h>
#include <swamp-capture/out_capture.h>

clog_config g_clog;


int main(int argc, char* argv[])
{
    g_clog.log = clog_console;

    if (argc <= 1) {
        CLOG_OUTPUT("usage: capture file");
        return -1;
    }


    return 0;
}
