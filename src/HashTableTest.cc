/* Copyright (c) 2009-2012 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "TestUtil.h"

#include "HashTable.h"

namespace RAMCloud {

class TestObject {
  public:
    TestObject()
        : tableId(0),
          stringKeyPtr(NULL),
          stringKeyLength(0),
          count(0)
    {
    }

    TestObject(uint64_t tableId, string stringKey)
        : tableId(tableId),
          stringKeyPtr(strdup(stringKey.c_str())),
          stringKeyLength(downCast<uint16_t>(stringKey.length())),
          count(0)
    {
    }

    ~TestObject()
    {
        if (stringKeyPtr != NULL)
            free(stringKeyPtr);
    }

    void
    setKey(string stringKey)
    {
        if (stringKeyPtr != NULL)
            free(stringKeyPtr);

        stringKeyPtr = strdup(stringKey.c_str());
        stringKeyLength = downCast<uint16_t>(stringKey.length());
    }

    uint64_t
    u64Address()
    {
        return reinterpret_cast<uint64_t>(this);
    }

    uint64_t tableId;
    void* stringKeyPtr;
    uint16_t stringKeyLength;
    uint64_t count;             // Used in forEach callback tests

    DISALLOW_COPY_AND_ASSIGN(TestObject);
} __attribute__((aligned(64)));

class TestObjectKeyComparer : public HashTable::KeyComparer {
  public:
    bool
    doesMatch(Key& key, HashTable::Reference candidate)
    {
        // For these unit tests, we'll squeeze a 48-bit object pointer
        // into each reference. This lets us use the HashTable as though
        // it's just a regular map.
        TestObject* candidateObject =
            reinterpret_cast<TestObject*>(candidate.get());
        Key candidateKey(candidateObject->tableId,
                         candidateObject->stringKeyPtr,
                         candidateObject->stringKeyLength);
        return (key == candidateKey);
    }
};

/**
 * Unit tests for HashTable::PerfDistribution.
 */
class HashTablePerfDistributionTest : public ::testing::Test {
  public:
    HashTablePerfDistributionTest() {}

    DISALLOW_COPY_AND_ASSIGN(HashTablePerfDistributionTest);
};

TEST_F(HashTablePerfDistributionTest, constructor) {
    RAMCloud::HashTable::PerfDistribution d;
    EXPECT_EQ(~0UL, d.min);
    EXPECT_EQ(0UL, d.max);
    EXPECT_EQ(0UL, d.binOverflows);
    EXPECT_EQ(0UL, d.bins[0]);
    EXPECT_EQ(0UL, d.bins[1]);
    EXPECT_EQ(0UL, d.bins[2]);
}

TEST_F(HashTablePerfDistributionTest, storeSample) {
    HashTable::PerfDistribution d;

    // You can't use EXPECT_TRUE here because it tries to take a
    // reference to BIN_WIDTH. See 10.4.6.2 Member Constants of The C++
    // Programming Language by Bjarne Stroustrup for more about static
    // constant integers.
    EXPECT_TRUE(10 == HashTable::PerfDistribution::BIN_WIDTH);  // NOLINT

    d.storeSample(3);
    EXPECT_EQ(3UL, d.min);
    EXPECT_EQ(3UL, d.max);
    EXPECT_EQ(0UL, d.binOverflows);
    EXPECT_EQ(1UL, d.bins[0]);
    EXPECT_EQ(0UL, d.bins[1]);
    EXPECT_EQ(0UL, d.bins[2]);

    d.storeSample(3);
    d.storeSample(d.NBINS * d.BIN_WIDTH + 40);
    d.storeSample(12);
    d.storeSample(78);

    EXPECT_EQ(3UL, d.min);
    EXPECT_EQ(d.NBINS * d.BIN_WIDTH + 40, d.max);
    EXPECT_EQ(1UL, d.binOverflows);
    EXPECT_EQ(2UL, d.bins[0]);
    EXPECT_EQ(1UL, d.bins[1]);
    EXPECT_EQ(0UL, d.bins[2]);
}


/**
 * Unit tests for HashTable::Entry.
 */
class HashTableEntryTest : public ::testing::Test {
  public:
    HashTableEntryTest() {}

    /**
     * Return whether fields make it through #HashTable::Entry::pack() and
     * #HashTable::Entry::unpack() successfully.
     * \param hash
     *      See #HashTable::Entry::pack().
     * \param chain
     *      See #HashTable::Entry::pack().
     * \param ptr
     *      See #HashTable::Entry::pack().
     * \return
     *      Whether the fields out of #HashTable::Entry::unpack() are the same.
     */
    static bool
    packable(uint64_t hash, bool chain, uint64_t ptr)
    {
        HashTable::Entry e;

        HashTable::Entry::UnpackedEntry in;
        HashTable::Entry::UnpackedEntry out;

        in.hash = hash;
        in.chain = chain;
        in.ptr = ptr;

        e.pack(in.hash, in.chain, in.ptr);
        out = e.unpack();

        return (in.hash == out.hash &&
                in.chain == out.chain &&
                in.ptr == out.ptr);
    }
    DISALLOW_COPY_AND_ASSIGN(HashTableEntryTest);
};

TEST_F(HashTableEntryTest, size) {
    EXPECT_EQ(8U, sizeof(HashTable::Entry));
}

// also tests unpack
TEST_F(HashTableEntryTest, pack) {
    // first without normal cases
    EXPECT_TRUE(packable(0x0000UL, false, 0x000000000000UL));
    EXPECT_TRUE(packable(0xffffUL, true,  0x7fffffffffffUL));
    EXPECT_TRUE(packable(0xffffUL, false, 0x7fffffffffffUL));
    EXPECT_TRUE(packable(0xa257UL, false, 0x3cdeadbeef98UL));

    // and now test the exception cases of pack()
    HashTable::Entry e;
    EXPECT_THROW(e.pack(0, false, 0xffffffffffffUL), Exception);
}

// No tests for test_unpack, since test_pack tested it.

TEST_F(HashTableEntryTest, clear) {
    HashTable::Entry e;
    e.value = 0xdeadbeefdeadbeefUL;
    e.clear();
    HashTable::Entry::UnpackedEntry out;
    out = e.unpack();
    EXPECT_EQ(0UL, out.hash);
    EXPECT_FALSE(out.chain);
    EXPECT_EQ(0UL, out.ptr);
}

TEST_F(HashTableEntryTest, trivial_clear) {
    HashTable::Entry e;
    e.value = 0xdeadbeefdeadbeefUL;
    e.clear();
    HashTable::Entry f;
    f.value = 0xdeadbeefdeadbeefUL;
    f.pack(0, false, 0);
    EXPECT_EQ(e.value, f.value);
}

TEST_F(HashTableEntryTest, setReference) {
    HashTable::Entry e;
    e.value = 0xdeadbeefdeadbeefUL;
    e.setReference(0xaaaaUL, HashTable::Reference( 
        0x7fffffffffffUL));
    HashTable::Entry::UnpackedEntry out;
    out = e.unpack();
    EXPECT_EQ(0xaaaaUL, out.hash);
    EXPECT_FALSE(out.chain);
    EXPECT_EQ(0x7fffffffffffUL, out.ptr);
}

TEST_F(HashTableEntryTest, setChainPointer) {
    HashTable::Entry e;
    e.value = 0xdeadbeefdeadbeefUL;
    {
        HashTable::CacheLine *cl;
        cl = reinterpret_cast<HashTable::CacheLine*>(
            0x7fffffffffffUL);
        e.setChainPointer(cl);
    }
    HashTable::Entry::UnpackedEntry out;
    out = e.unpack();
    EXPECT_EQ(0UL, out.hash);
    EXPECT_TRUE(out.chain);
    EXPECT_EQ(0x7fffffffffffUL, out.ptr);
}

TEST_F(HashTableEntryTest, isAvailable) {
    HashTable::Entry e;
    e.clear();
    EXPECT_TRUE(e.isAvailable());
    e.setChainPointer(reinterpret_cast<HashTable::CacheLine*>(
        0x1UL));
    EXPECT_FALSE(e.isAvailable());
    e.setReference(0UL, HashTable::Reference(0x1UL));
    EXPECT_FALSE(e.isAvailable());
    e.clear();
    EXPECT_TRUE(e.isAvailable());
}

TEST_F(HashTableEntryTest, getReference) {
    HashTable::Entry e;
    TestObject o;
    HashTable::Reference oRef(o.u64Address());
    e.setReference(0xaaaaUL, oRef);
    EXPECT_EQ(oRef, e.getReference());
    EXPECT_EQ(&o, reinterpret_cast<TestObject*>(e.getReference().get()));
}

TEST_F(HashTableEntryTest, getChainPointer) {
    HashTable::CacheLine *cl;
    cl = reinterpret_cast<HashTable::CacheLine*>(0x7fffffffffffUL);
    HashTable::Entry e;
    e.setChainPointer(cl);
    EXPECT_EQ(cl, e.getChainPointer());
    e.clear();
    EXPECT_TRUE(NULL == e.getChainPointer());
    e.setReference(0UL, HashTable::Reference(0x1UL));
    EXPECT_TRUE(NULL == e.getChainPointer());
}

TEST_F(HashTableEntryTest, hashMatches) {
    HashTable::Entry e;
    e.clear();
    EXPECT_TRUE(!e.hashMatches(0UL));
    e.setChainPointer(reinterpret_cast<HashTable::CacheLine*>(
        0x1UL));
    EXPECT_TRUE(!e.hashMatches(0UL));
    e.setReference(0UL, HashTable::Reference(0x1UL));
    EXPECT_TRUE(e.hashMatches(0UL));
    EXPECT_TRUE(!e.hashMatches(0xbeefUL));
    e.setReference(0xbeefUL, HashTable::Reference(0x1UL));
    EXPECT_TRUE(!e.hashMatches(0UL));
    EXPECT_TRUE(e.hashMatches(0xbeefUL));
    EXPECT_TRUE(!e.hashMatches(0xfeedUL));
}

/**
 * Unit tests for HashTable.
 */
class HashTableTest : public ::testing::Test {
  public:

    uint64_t tableId;
    uint64_t numEnt;
    TestObjectKeyComparer keyComparer;
    HashTable ht;
    vector<TestObject*> values;

    HashTableTest()
        : tableId(),
          numEnt(),
          keyComparer(),
          ht(1, keyComparer),
          values()
    {
    }

    ~HashTableTest()
    {
        foreach(TestObject* o, values)
            delete o;
    }

    /**
     * Common setup code for the lookupEntry and insert tests.
     * \param[in] tableId
     *      The table id to use for all objects placed in the hashtable.
     * \param[in] numEnt
     *      The number of entries to place in the hashtable.
     */
    void setup(uint64_t tableId, uint64_t numEnt)
    {
        this->tableId = tableId;
        this->numEnt = numEnt;
        uint64_t numCacheLines =
                ((numEnt + HashTable::ENTRIES_PER_CACHE_LINE - 2) /
                (HashTable::ENTRIES_PER_CACHE_LINE - 1));
        if (numCacheLines == 0)
            numCacheLines = 1;
        LargeBlockOfMemory<HashTable::CacheLine> cacheLines(
                numCacheLines * sizeof(HashTable::CacheLine));
        insertArray(&ht, values, tableId, numEnt, &cacheLines,
                numCacheLines);
    }

    // convenient abbreviation
#define seven (HashTable::ENTRIES_PER_CACHE_LINE - 1)

    /**
     * Insert an array of values into a single-bucket hash table.
     * \param[in] ht
     *      A hash table with a single bucket.
     * \param[in] values
     *      An array of values to add to the bucket (in order). These need not
     *      be initialized and will be set counting up from 0.
     * \param[in] tableId
     *      The table ID to use for each object inserted.
     * \param[in] numEnt
     *      The number of values in \a values.
     * \param[in] cacheLines
     *      An array of cache lines to back the bucket with. 
     * \param[in] numCacheLines
     *      The number of cache lines in \a cacheLines.
     */
    void insertArray(HashTable *ht, vector<TestObject*>& values,
                     uint64_t tableId, uint64_t numEnt,
                     LargeBlockOfMemory<HashTable::CacheLine> *cacheLines,
                     uint64_t numCacheLines)
    {
        HashTable::CacheLine *cl;

        // chain all the cache lines
        cl = &cacheLines->get()[0];
        while (cl < &cacheLines->get()[numCacheLines - 1]) {
            cl->entries[seven].setChainPointer(cl + 1);
            cl++;
        }

        // wipe any old values
        foreach(TestObject* o, values)
            delete o;
        values.clear();

        // fill in the "log" entries
        for (uint64_t i = 0; i < numEnt; i++) {
            string stringKey = format("%lu", i);
            values.push_back(new TestObject(tableId, stringKey));
            Key key(values[i]->tableId,
                    values[i]->stringKeyPtr,
                    values[i]->stringKeyLength);

            uint64_t littleHash;
            (void) ht->findBucket(key, &littleHash);

            HashTable::Entry *entry;
            if (0 < i && i == numEnt - 1 && i % seven == 0)
                entry = &cacheLines->get()[i / seven - 1].entries[seven];
            else
                entry = &cacheLines->get()[i / seven].entries[i % seven];
            HashTable::Reference reference(values[i]->u64Address());
            entry->setReference(littleHash, reference);
        }

        ht->buckets.swap(*cacheLines);
    }

    /**
     * Find an entry in a single-bucket hash table by position.
     * \param[in] ht
     *      A hash table with a single bucket.
     * \param[in] x
     *      The number of the cache line in the chain, starting from 0.
     * \param[in] y
     *      The number of the entry in the cache line, starting from 0.
     * \return
     *      The entry at \a x and \a y in the only bucket of \a ht.
     */
    HashTable::Entry& entryAt(HashTable *ht, uint64_t x,
                                    uint64_t y)
    {
        HashTable::CacheLine *cl = &ht->buckets.get()[0];
        while (x > 0) {
            cl = cl->entries[seven].getChainPointer();
            x--;
        }
        return cl->entries[y];
    }

    /**
     * Ensure an entry in a single-bucket hash table contains a given pointer.
     * \param[in] ht
     *      A hash table with a single bucket.
     * \param[in] x
     *      The number of the cache line in the chain, starting from 0.
     * \param[in] y
     *      The number of the entry in the cache line, starting from 0.
     * \param[in] ptr
     *      The pointer that we expect to find at the given position.
     */
    void assertEntryIs(HashTable *ht, uint64_t x, uint64_t y,
                       TestObject *ptr)
    {
        Key key(ptr->tableId,
                ptr->stringKeyPtr,
                ptr->stringKeyLength);
        uint64_t littleHash;
        (void) ht->findBucket(key, &littleHash);

        HashTable::Entry& entry = entryAt(ht, x, y);
        EXPECT_TRUE(entry.hashMatches(littleHash));
        EXPECT_EQ(ptr->u64Address(), entry.getReference().get());
    }

    HashTable::Entry *findBucketAndLookupEntry(HashTable *ht,
                                               uint64_t tableId,
                                               const void* stringKey,
                                               uint16_t stringKeyLength)
    {
        uint64_t secondaryHash;
        HashTable::CacheLine *bucket;
        Key key(tableId, stringKey, stringKeyLength);
        bucket = ht->findBucket(key, &secondaryHash);
        return ht->lookupEntry(bucket, secondaryHash, key);
    }

    DISALLOW_COPY_AND_ASSIGN(HashTableTest);
};

TEST_F(HashTableTest, constructor) {
    HashTable ht(16, keyComparer);
    for (uint32_t i = 0; i < 16; i++) {
        for (uint32_t j = 0; j < ht.entriesPerCacheLine(); j++)
            EXPECT_TRUE(ht.buckets.get()[i].entries[j].isAvailable());
    }
}

TEST_F(HashTableTest, constructor_truncate) {
    // This is effectively testing nearestPowerOfTwo.
    EXPECT_EQ(1UL, HashTable(1, keyComparer).numBuckets);
    EXPECT_EQ(2UL, HashTable(2, keyComparer).numBuckets);
    EXPECT_EQ(2UL, HashTable(3, keyComparer).numBuckets);
    EXPECT_EQ(4UL, HashTable(4, keyComparer).numBuckets);
    EXPECT_EQ(4UL, HashTable(5, keyComparer).numBuckets);
    EXPECT_EQ(4UL, HashTable(6, keyComparer).numBuckets);
    EXPECT_EQ(4UL, HashTable(7, keyComparer).numBuckets);
    EXPECT_EQ(8UL, HashTable(8, keyComparer).numBuckets);
}

TEST_F(HashTableTest, destructor) {
}

TEST_F(HashTableTest, simple) {
    HashTable ht(1024, keyComparer);

    TestObject a(0, "0");
    TestObject b(0, "10");

    Key aKey(a.tableId, a.stringKeyPtr, a.stringKeyLength);
    Key bKey(b.tableId, b.stringKeyPtr, b.stringKeyLength);

    HashTable::Reference aRef(a.u64Address());
    HashTable::Reference bRef(b.u64Address());
    HashTable::Reference outRef;

    EXPECT_FALSE(ht.lookup(aKey, outRef));
    ht.replace(aKey, aRef);
    EXPECT_TRUE(ht.lookup(aKey, outRef));
    EXPECT_EQ(aRef, outRef);

    EXPECT_FALSE(ht.lookup(bKey, outRef));
    ht.replace(bKey, bRef);
    EXPECT_TRUE(ht.lookup(bKey, outRef));
    EXPECT_EQ(bRef, outRef);
}

TEST_F(HashTableTest, multiTable) {
    HashTable ht(1024, keyComparer);

    TestObject a(0, "0");
    TestObject b(1, "0");
    TestObject c(0, "1");

    Key aKey(a.tableId, a.stringKeyPtr, a.stringKeyLength);
    Key bKey(b.tableId, b.stringKeyPtr, b.stringKeyLength);
    Key cKey(c.tableId, c.stringKeyPtr, c.stringKeyLength);

    HashTable::Reference outRef;

    EXPECT_FALSE(ht.lookup(aKey, outRef));
    EXPECT_FALSE(ht.lookup(bKey, outRef));
    EXPECT_FALSE(ht.lookup(cKey, outRef));

    HashTable::Reference aRef(a.u64Address());
    HashTable::Reference bRef(b.u64Address());
    HashTable::Reference cRef(c.u64Address());

    ht.replace(aKey, aRef);
    ht.replace(bKey, bRef);
    ht.replace(cKey, cRef);

    EXPECT_TRUE(ht.lookup(aKey, outRef));
    EXPECT_EQ(aRef, outRef);

    EXPECT_TRUE(ht.lookup(bKey, outRef));
    EXPECT_EQ(bRef, outRef);

    EXPECT_TRUE(ht.lookup(cKey, outRef));
    EXPECT_EQ(cRef, outRef);
}

TEST_F(HashTableTest, findBucket) {
    HashTable ht(1024, keyComparer);
    HashTable::CacheLine *bucket;
    uint64_t hashValue;
    uint64_t secondaryHash;

    Key key(0, "4327", 4);
    bucket = ht.findBucket(key, &secondaryHash);
    hashValue = key.getHash();

    uint64_t actualBucketIdx = static_cast<uint64_t>(bucket - ht.buckets.get());
    uint64_t expectedBucketIdx = (hashValue & 0x0000ffffffffffffffffUL) % 1024;
    EXPECT_EQ(actualBucketIdx, expectedBucketIdx);
    EXPECT_EQ(secondaryHash, hashValue >> 48);
}

/**
 * Test #RAMCloud::HashTable::lookupEntry() when the key is not
 * found.
 */
TEST_F(HashTableTest, lookupEntry_notFound) {
    {
        setup(0, 0);
        EXPECT_EQ(static_cast<HashTable::Entry*>(NULL),
                  findBucketAndLookupEntry(&ht, 0, "0", 1));
        EXPECT_EQ(1UL, ht.getPerfCounters().lookupEntryCalls);
        EXPECT_LT(0U, ht.getPerfCounters().lookupEntryCycles);
        EXPECT_LT(0U, ht.getPerfCounters().lookupEntryDist.max);
    }
    {
        setup(0, HashTable::ENTRIES_PER_CACHE_LINE * 5);

        string key = format("%lu", numEnt + 1);

        EXPECT_EQ(static_cast<HashTable::Entry*>(NULL),
                  findBucketAndLookupEntry(&ht, 0, key.c_str(),
                        downCast<uint16_t>(key.length())));
        EXPECT_EQ(5UL, ht.getPerfCounters().lookupEntryChainsFollowed);
    }
}

/**
 * Test #RAMCloud::HashTable::lookupEntry() when the key is found in
 * the first entry of the first cache line.
 */
TEST_F(HashTableTest, lookupEntry_cacheLine0Entry0) {
    setup(0, 1);
    EXPECT_EQ(&entryAt(&ht, 0, 0),
              findBucketAndLookupEntry(&ht, 0, "0", 1));
}

/**
 * Test #RAMCloud::HashTable::lookupEntry() when the key is found in
 * the last entry of the first cache line.
 */
TEST_F(HashTableTest, lookupEntry_cacheLine0Entry7) {
    setup(0, HashTable::ENTRIES_PER_CACHE_LINE);
    string key = format("%u", HashTable::ENTRIES_PER_CACHE_LINE - 1);

    EXPECT_EQ(&entryAt(&ht, 0, seven),
              findBucketAndLookupEntry(&ht, 0, key.c_str(),
                    downCast<uint16_t>(key.length())));
}

/**
 * Test #RAMCloud::HashTable::lookupEntry() when the key is found in
 * the first entry of the third cache line.
 */
TEST_F(HashTableTest, lookupEntry_cacheLine2Entry0) {
    setup(0, HashTable::ENTRIES_PER_CACHE_LINE * 5);

    // with 8 entries per cache line:
    // cl0: [ k00, k01, k02, k03, k04, k05, k06, cl1 ]
    // cl1: [ k07, k09, k09, k10, k11, k12, k13, cl2 ]
    // cl2: [ k14, k15, k16, k17, k18, k19, k20, cl3 ]
    // ...

    string key =
            format("%u", (HashTable::ENTRIES_PER_CACHE_LINE - 1) * 2);
    EXPECT_EQ(&entryAt(&ht, 2, 0),
              findBucketAndLookupEntry(&ht, 0, key.c_str(),
                    downCast<uint16_t>(key.length())));
}

/**
 * Test #RAMCloud::HashTable::lookupEntry() when there is a hash collision
 * with another Entry.
 */
TEST_F(HashTableTest, lookupEntry_hashCollision) {
    setup(0, 1);
    EXPECT_EQ(&entryAt(&ht, 0, 0),
              findBucketAndLookupEntry(&ht, 0, "0", 1));
    EXPECT_LT(0U, ht.getPerfCounters().lookupEntryDist.max);
    values[0]->setKey("randomKeyValue");
    EXPECT_EQ(static_cast<HashTable::Entry*>(NULL),
              findBucketAndLookupEntry(&ht, 0, "0", 1));
    EXPECT_EQ(1UL, ht.getPerfCounters().lookupEntryHashCollisions);
}

TEST_F(HashTableTest, lookup) {
    HashTable ht(1, keyComparer);
    TestObject *v = new TestObject(0, "0");
    Key vKey(v->tableId, v->stringKeyPtr, v->stringKeyLength);

    HashTable::Reference outRef;
    EXPECT_FALSE(ht.lookup(vKey, outRef));

    HashTable::Reference vRef(v->u64Address());
    ht.replace(vKey, vRef);
    EXPECT_TRUE(ht.lookup(vKey, outRef));
    EXPECT_EQ(outRef, vRef);

    delete v;
}

TEST_F(HashTableTest, remove) {
    HashTable ht(1, keyComparer);

    Key key(0, "0", 1);
    EXPECT_FALSE(ht.remove(key));

    TestObject *v = new TestObject(0, "0");
    HashTable::Reference vRef(v->u64Address());

    ht.replace(key, vRef);
    EXPECT_TRUE(ht.remove(key));

    HashTable::Reference outRef;
    EXPECT_FALSE(ht.lookup(key, outRef));
    EXPECT_FALSE(ht.remove(key));

    delete v;
}

TEST_F(HashTableTest, replace_normal) {
    HashTable ht(1, keyComparer);

    TestObject *v = new TestObject(0, "0");
    TestObject *w = new TestObject(0, "0");

    HashTable::Reference vRef(v->u64Address());
    HashTable::Reference wRef(w->u64Address());

    // key is identical for both
    Key key(v->tableId, v->stringKeyPtr, v->stringKeyLength);

    EXPECT_FALSE(ht.replace(key, vRef));
    EXPECT_EQ(1UL, ht.getPerfCounters().replaceCalls);
    EXPECT_LT(0U, ht.getPerfCounters().replaceCycles);

    HashTable::Reference outRef;

    EXPECT_TRUE(ht.lookup(key, outRef));
    EXPECT_EQ(vRef, outRef);

    EXPECT_TRUE(ht.replace(key, vRef));
    EXPECT_TRUE(ht.lookup(key, outRef));
    EXPECT_EQ(vRef, outRef);

    EXPECT_TRUE(ht.replace(key, wRef));
    EXPECT_TRUE(ht.lookup(key, outRef));
    EXPECT_EQ(wRef, outRef);

    delete v;
    delete w;
}

/**
 * Test #RAMCloud::HashTable::replace() when the key is new and the
 * first entry of the first cache line is available.
 */
TEST_F(HashTableTest, replace_cacheLine0Entry0) {
    setup(0, 0);
    TestObject v(0, "newKey");
    Key vKey(v.tableId, v.stringKeyPtr, v.stringKeyLength);
    HashTable::Reference vRef(v.u64Address());
    ht.replace(vKey, vRef);
    assertEntryIs(&ht, 0, 0, &v);
}

/**
 * Test #RAMCloud::HashTable::replace() when the key is new and the
 * last entry of the first cache line is available.
 */
TEST_F(HashTableTest, replace_cacheLine0Entry7) {
    setup(0, HashTable::ENTRIES_PER_CACHE_LINE - 1);
    TestObject v(0, "newKey");
    Key vKey(v.tableId, v.stringKeyPtr, v.stringKeyLength);
    HashTable::Reference vRef(v.u64Address());
    ht.replace(vKey, vRef);
    assertEntryIs(&ht, 0, seven, &v);
}

/**
 * Test #RAMCloud::HashTable::replace() when the key is new and the
 * first entry of the third cache line is available. The third cache line
 * is already chained onto the second.
 */
TEST_F(HashTableTest, replace_cacheLine2Entry0) {
    setup(0, HashTable::ENTRIES_PER_CACHE_LINE * 2);
    ht.buckets.get()[2].entries[0].clear();
    ht.buckets.get()[2].entries[1].clear();
    TestObject v(0, "newKey");
    Key vKey(v.tableId, v.stringKeyPtr, v.stringKeyLength);
    HashTable::Reference vRef(v.u64Address());
    ht.replace(vKey, vRef);
    assertEntryIs(&ht, 2, 0, &v);
    EXPECT_EQ(2UL, ht.getPerfCounters().insertChainsFollowed);
}

/**
 * Test #RAMCloud::HashTable::replace() when the key is new and the
 * first and only cache line is full. The second cache line needs to be
 * allocated.
 */
TEST_F(HashTableTest, replace_cacheLineFull) {
    setup(0, HashTable::ENTRIES_PER_CACHE_LINE);
    TestObject v(0,  "newKey");
    Key vKey(v.tableId, v.stringKeyPtr, v.stringKeyLength);
    HashTable::Reference vRef(v.u64Address());
    ht.replace(vKey, vRef);
    EXPECT_TRUE(entryAt(&ht, 0, seven).getChainPointer() != NULL);
    EXPECT_TRUE(entryAt(&ht, 0, seven).getChainPointer() !=
                &ht.buckets.get()[1]);
    assertEntryIs(&ht, 1, 0, values[seven]);
    assertEntryIs(&ht, 1, 1, &v);
}

/**
 * Callback used by test_forEach().
 */ 
static void
test_forEach_callback(HashTable::Reference ref, void *cookie)
{
    EXPECT_EQ(cookie, reinterpret_cast<void *>(57));
    reinterpret_cast<TestObject*>(ref.get())->count++;
}

/**
 * Simple test for #RAMCloud::HashTable::forEach(), ensuring that it
 * properly traverses multiple buckets and chained cachelines.
 */
TEST_F(HashTableTest, forEach) {
    HashTable ht(2, keyComparer);
    uint32_t arrayLen = 256;
    TestObject* checkoff = new TestObject[arrayLen];

    for (uint32_t i = 0; i < arrayLen; i++) {
        string stringKey = format("%u", i);
        checkoff[i].setKey(stringKey);
        Key key(checkoff[i].tableId,
                checkoff[i].stringKeyPtr,
                checkoff[i].stringKeyLength);
        HashTable::Reference ref(checkoff[i].u64Address());
        ht.replace(key, ref);
    }

    uint64_t t = ht.forEach(test_forEach_callback,
        reinterpret_cast<void *>(57));
    EXPECT_EQ(arrayLen, t);

    for (uint32_t i = 0; i < arrayLen; i++)
        EXPECT_EQ(1U, checkoff[i].count);
}

} // namespace RAMCloud
