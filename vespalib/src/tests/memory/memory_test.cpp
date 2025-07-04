// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/vespalib/gtest/gtest.h>
#include <vespa/vespalib/util/memory.h>

using namespace vespalib;

class B
{
public:
    virtual ~B() = default;
    virtual B * clone() const { return new B(*this); }
};

class A : public B
{
public:
    ~A() override = default;
    A * clone() const override { return new A(*this); }
};

TEST(MemoryTest, require_that_MallocAutoPtr_works_as_expected) {
    MallocAutoPtr a(malloc(30));
    EXPECT_TRUE(a.get() != nullptr);
    void * tmp = a.get();
    MallocAutoPtr b(std::move(a));
    EXPECT_TRUE(tmp == b.get());
    EXPECT_TRUE(a.get() == nullptr);
    MallocAutoPtr c;
    c = std::move(b);
    EXPECT_TRUE(b.get() == nullptr);
    EXPECT_TRUE(tmp == c.get());
    MallocAutoPtr d(malloc(30));
    EXPECT_TRUE(d.get() != nullptr);
    tmp = c.get();
    d = std::move(c);
    EXPECT_TRUE(tmp == d.get());
    EXPECT_TRUE(c.get() == nullptr);
}

TEST(MemoryTest, require_that_MallocPtr_works_as_expected) {
    MallocPtr a(100);
    EXPECT_TRUE(a.size() == 100);
    EXPECT_TRUE(a.get() != nullptr);
    memset(a.get(), 17, a.size());
    MallocPtr b(a);
    EXPECT_TRUE(a.size() == 100);
    EXPECT_TRUE(a.get() != nullptr);
    EXPECT_TRUE(b.size() == 100);
    EXPECT_TRUE(b.get() != nullptr);
    EXPECT_TRUE(a.get() != b.get());
    EXPECT_TRUE(memcmp(a.get(), b.get(), a.size()) == 0);
    void * tmp = a.get();
    a = b;
    EXPECT_TRUE(a.size() == 100);
    EXPECT_TRUE(a.get() != nullptr);
    EXPECT_TRUE(a.get() != tmp);
    EXPECT_TRUE(memcmp(a.get(), b.get(), a.size()) == 0);
    MallocPtr d = std::move(b);
    EXPECT_TRUE(d.size() == 100);
    EXPECT_TRUE(d.get() != nullptr);
    EXPECT_TRUE(b.size() == 0);
    EXPECT_TRUE(b.get() == nullptr);
    MallocPtr c;
    c.realloc(89);
    EXPECT_EQ(c.size(), 89u);
    c.realloc(0);
    EXPECT_EQ(c.size(), 0u);
    EXPECT_TRUE(c == nullptr);    
}

TEST(MemoryTest, require_that_CloneablePtr_works_as_expected) {
    CloneablePtr<B> a(new A());
    EXPECT_TRUE(a.get() != nullptr);
    CloneablePtr<B> b(a);
    EXPECT_TRUE(a.get() != nullptr);
    EXPECT_TRUE(b.get() != nullptr);
    EXPECT_TRUE(b.get() != a.get());
    CloneablePtr<B> c;
    c = a;
    EXPECT_TRUE(a.get() != nullptr);
    EXPECT_TRUE(c.get() != nullptr);
    EXPECT_TRUE(c.get() != a.get());

    b = CloneablePtr<B>(new B());
    EXPECT_TRUE(dynamic_cast<B*>(b.get()) != nullptr);
    EXPECT_TRUE(dynamic_cast<A*>(b.get()) == nullptr);
    EXPECT_TRUE(dynamic_cast<B*>(a.get()) != nullptr);
    EXPECT_TRUE(dynamic_cast<A*>(a.get()) != nullptr);
    EXPECT_TRUE(dynamic_cast<B*>(c.get()) != nullptr);
    EXPECT_TRUE(dynamic_cast<A*>(c.get()) != nullptr);
    c = b;
    EXPECT_TRUE(dynamic_cast<B*>(c.get()) != nullptr);
    EXPECT_TRUE(dynamic_cast<A*>(c.get()) == nullptr);
}

TEST(MemoryTest, require_that_CloneablePtr_bool_conversion_works_as_expected) {
    {
        CloneablePtr<B> null;
        if (null) {
            EXPECT_TRUE(false);
        } else {
            EXPECT_FALSE(bool(null));
            EXPECT_TRUE(!null);
        }
    }
    {
        CloneablePtr<B> notNull(new A());
        if (notNull) {
            EXPECT_TRUE(bool(notNull));
            EXPECT_FALSE(!notNull);
        } else {
            EXPECT_TRUE(false);
        }
    }
}

TEST(MemoryTest, require_that_VESPA_NELEMS_works_as_expected) {
    int a[3];
    int b[4] = {0,1,2,3};
    int c[4] = {0,1,2};
    int d[] = {0,1,2,3,4};
    EXPECT_EQ(VESPA_NELEMS(a), 3u);
    EXPECT_EQ(VESPA_NELEMS(b), 4u);
    EXPECT_EQ(VESPA_NELEMS(c), 4u);
    EXPECT_EQ(VESPA_NELEMS(d), 5u);
}

TEST(MemoryTest, require_that_memcpy_safe_works_as_expected) {
    std::string a("abcdefgh");
    std::string b("01234567");
    memcpy_safe(&b[0], &a[0], 4);
    memcpy_safe(nullptr, &a[0], 0);
    memcpy_safe(&b[0], nullptr, 0);
    memcpy_safe(nullptr, nullptr, 0);
    EXPECT_EQ(std::string("abcdefgh"), a);
    EXPECT_EQ(std::string("abcd4567"), b);
}

TEST(MemoryTest, require_that_memmove_safe_works_as_expected) {
    std::string str("0123456789");
    memmove_safe(&str[2], &str[0], 5);
    memmove_safe(nullptr, &str[0], 0);
    memmove_safe(&str[0], nullptr, 0);
    memmove_safe(nullptr, nullptr, 0);
    EXPECT_EQ(std::string("0101234789"), str);
}

TEST(MemoryTest, require_that_memcmp_safe_works_as_expected) {
    std::string a("ab");
    std::string b("ac");
    EXPECT_EQ(memcmp_safe(&a[0], &b[0], 0), 0);
    EXPECT_EQ(memcmp_safe(nullptr, &b[0], 0), 0);
    EXPECT_EQ(memcmp_safe(&a[0], nullptr, 0), 0);
    EXPECT_EQ(memcmp_safe(nullptr, nullptr, 0), 0);
    EXPECT_EQ(memcmp_safe(&a[0], &b[0], 1), 0);
    EXPECT_LT(memcmp_safe(&a[0], &b[0], 2), 0);
    EXPECT_GT(memcmp_safe(&b[0], &a[0], 2), 0);
}

TEST(MemoryTest, require_that_Unaligned_wrapper_works_as_expected) {
    struct Data {
        char buf[sizeof(uint32_t) * 11]; // space for 10 unaligned values
        void *get(size_t idx) { return buf + (idx * sizeof(uint32_t)) + 3; }
        const void *cget(size_t idx) { return get(idx); }
        Data() { memset(buf, 0, sizeof(buf)); }
    };
    Data data;
    EXPECT_EQ(sizeof(Unaligned<uint32_t>), sizeof(uint32_t));
    EXPECT_EQ(alignof(Unaligned<uint32_t>), 1u);
    Unaligned<uint32_t> *arr = Unaligned<uint32_t>::ptr(data.get(0));
    const Unaligned<uint32_t> *carr = Unaligned<uint32_t>::ptr(data.cget(0));
    Unaligned<uint32_t>::at(data.get(0)).write(123);
    Unaligned<uint32_t>::at(data.get(1)) = 456;
    arr[2] = 789;
    arr[3] = arr[0];
    arr[4] = arr[1].read();
    arr[5].write(arr[2]);
    EXPECT_EQ(Unaligned<uint32_t>::at(data.get(0)).read(), 123u);
    EXPECT_EQ(Unaligned<uint32_t>::at(data.get(1)), 456u);
    EXPECT_EQ(arr[2], 789u);
    EXPECT_EQ(carr[3].read(), 123u);
    EXPECT_EQ(carr[4], 456u);
    EXPECT_EQ(carr[5], 789u);
}

GTEST_MAIN_RUN_ALL_TESTS()
