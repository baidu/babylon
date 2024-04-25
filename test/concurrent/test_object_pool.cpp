#include <babylon/concurrent/object_pool.h>

#include <gtest/gtest.h>

#include <future>

using ::babylon::ObjectPool;

struct ObjectPoolTest : public ::testing::Test {
    void SetUp() override {
        auto_pool.reserve_and_clear(4);
        auto_pool.set_creator([&] {
            create_times++;
            return new ::std::string {"auto"};
        });

        pool.reserve_and_clear(4);
    }

    ObjectPool<::std::string> auto_pool;
    ObjectPool<::std::string> pool;
    int create_times {0};

    ::std::promise<void> p;
    ::std::future<void> f = p.get_future();
};

TEST_F(ObjectPoolTest, pop_wait_push) {
    ::std::thread([&] {
        {
            auto ptr = pool.pop();
            ASSERT_EQ("10086", *ptr);
        }
        p.set_value();
    }).detach();
    ASSERT_EQ(::std::future_status::timeout, f.wait_for(::std::chrono::milliseconds(100)));
    pool.push(::std::unique_ptr<::std::string>(
                new ::std::string {"10086"}));
    f.get();
}

TEST_F(ObjectPoolTest, push_wait_pop) {
    ::std::thread([&] {
        for (size_t i = 0; i < 10; ++i) {
            pool.push(::std::unique_ptr<::std::string>(
                        new ::std::string));
        }
        p.set_value();
    }).detach();
    ASSERT_EQ(::std::future_status::timeout, f.wait_for(::std::chrono::milliseconds(100)));
    for (size_t i = 0; i < 10; ++i) {
        delete pool.pop().release();
    }
    f.get();
}

TEST_F(ObjectPoolTest, try_pop_fail_when_empty) {
    ASSERT_FALSE(pool.try_pop());
    pool.push(::std::unique_ptr<::std::string>(new ::std::string {"10086"}));
    auto p = pool.try_pop();
    ASSERT_TRUE(p);
    ASSERT_EQ("10086", *p);
}

TEST_F(ObjectPoolTest, pop_auto_create) {
    auto ptr = auto_pool.pop();
    ASSERT_EQ(1, create_times);
    ASSERT_EQ("auto", *ptr);
}

TEST_F(ObjectPoolTest, pop_auto_push) {
    pool.push(::std::unique_ptr<::std::string>(
                new ::std::string));
    for (size_t i = 0; i < 10; ++i) {
        pool.pop()->append(1, '0' + i);
    }
    ASSERT_EQ("0123456789", *pool.pop());

    for (ssize_t i = 9; i >= 0; --i) {
        auto_pool.pop()->append(1, '0' + i);
    }
    ASSERT_EQ("auto9876543210", *auto_pool.pop());
}

TEST_F(ObjectPoolTest, pop_manual_push) {
    pool.push(::std::unique_ptr<::std::string>(
                new ::std::string));
    for (size_t i = 0; i < 10; ++i) {
        auto ptr = pool.pop();
        ptr->append(1, '0' + i);
        pool.push(::std::move(ptr));
    }
    ASSERT_EQ("0123456789", *pool.pop());

    for (ssize_t i = 9; i >= 0; --i) {
        auto ptr = auto_pool.pop();
        ptr->append(1, '0' + i);
        auto_pool.push(::std::move(ptr));
    }
    ASSERT_EQ("auto9876543210", *auto_pool.pop());
}

TEST_F(ObjectPoolTest, push_recycle_object) {
    pool.set_recycler([] (::std::string& s) {
        s.append(1, ' ');
    });
    pool.push(::std::unique_ptr<::std::string>(
                new ::std::string));
    for (size_t i = 0; i < 10; ++i) {
        pool.pop()->append(1, '0' + i);
    }
    ASSERT_EQ(" 0 1 2 3 4 5 6 7 8 9 ", *pool.pop());

    auto_pool.set_recycler([] (::std::string& s) {
        s.append(1, ' ');
    });
    for (ssize_t i = 9; i >= 0; --i) {
        auto_pool.pop()->append(1, '0' + i);
    }
    ASSERT_EQ("auto9 8 7 6 5 4 3 2 1 0 ", *auto_pool.pop());
}
