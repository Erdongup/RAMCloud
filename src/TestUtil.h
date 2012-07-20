/* Copyright (c) 2010-2012 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright
 * notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * \file
 * Declares various things that help in writing tests.
 */

#ifndef RAMCLOUD_TESTUTIL_H
#define RAMCLOUD_TESTUTIL_H

#include <gtest/gtest.h>
#include <regex.h>

// Arrange for private and protected structure members to be public so they
// can easily be accessed by gtest tests (see Common.h for details).
#ifdef RAMCLOUD_COMMON_H
#error "TestUtil.h must be included before Common.h"
#endif
#define EXPOSE_PRIVATES

#include "Common.h"
#include "Cycles.h"
#include "Buffer.h"
#include "ClientException.h"
#include "Dispatch.h"
#include "Transport.h"

namespace RAMCloud {

/**
 * Various utilities that are useful in writing unit tests.
 */
class TestUtil {
  public:
    static string bufferToDebugString(Buffer* buffer);
    static string checkLargeBuffer(Buffer* buffer, int expectedLength);
    static void convertChar(char c, string *out);
    static ::testing::AssertionResult doesNotMatchPosixRegex(
            const string& pattern, const string& subject);
    static void fillPrintableRandom(void* buf, uint32_t size);
    static void fillRandom(void* buf, uint32_t size);
    static void fillLargeBuffer(Buffer* buffer, int size);
    static const char *getStatus(Buffer* buffer);
    static ::testing::AssertionResult matchesPosixRegex(
            const string& pattern, const string& subject);
    static string readFile(const char* fileName);
    static string toString(const void* buf, uint32_t length);
    static string toString(Buffer* buffer, uint32_t offset, uint32_t length);
    static string toString(Buffer* buffer);
    static bool waitForRpc(Context& context, Transport::ClientRpc& rpc,
            int ms = 1000);
};

} // namespace RAMCloud

#endif  // RAMCLOUD_TESTUTIL_H
