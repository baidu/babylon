#include "babylon/time.h"

#include BABYLON_EXTERNAL(absl/time/clock.h)
#include "gtest/gtest.h"

#include <random>

TEST(time, calculate_date_correct) {
    ::std::string spec = "a[%a]A[%A]b[%b]B[%B]C[%C]d[%d]e[%e]";
    spec += "F[%F]G[%G]g[%g]h[%h]H[%H]I[%I]j[%j]k[%k]l[%l]m[%m]";
    spec += "M[%M]n[%n]p[%p]P[%P]r[%r]R[%R]s[%s]S[%S]t[%t]T[%T]";
    spec += "u[%u]U[%U]V[%V]w[%w]x[%x]X[%X]y[%y]Y[%Y]z[%z]Z[%Z]%%[%%]";

    // 500年覆盖了完整的闰年周期，测试日期正确性
    auto now = time(nullptr);
    ::std::string text1(5000, '\0');
    ::std::string text2(5000, '\0');
    ::std::mt19937_64 gen {::std::random_device{}()};
    for (size_t i = 0; i < 500; ++i) {
        for (size_t j = 0; j < 365; ++j) {
            time_t time = now + (i * 365 + j) * 24 * 60 * 60 + gen() % (24 * 60 * 60);
            struct ::tm tm1;
            ::memset(&tm1, 0, sizeof(tm1));
            localtime_r(&time, &tm1);
            struct ::tm tm2;
            ::memset(&tm2, 0, sizeof(tm2));
            ::babylon::localtime(&time, &tm2);
            ASSERT_EQ(tm1.tm_sec, tm2.tm_sec);
            ASSERT_EQ(tm1.tm_min, tm2.tm_min);
            ASSERT_EQ(tm1.tm_hour, tm2.tm_hour);
            ASSERT_EQ(tm1.tm_mday, tm2.tm_mday);
            ASSERT_EQ(tm1.tm_mon, tm2.tm_mon);
            ASSERT_EQ(tm1.tm_year, tm2.tm_year);
            ASSERT_EQ(tm1.tm_wday, tm2.tm_wday);
            ASSERT_EQ(tm1.tm_yday, tm2.tm_yday);
            ASSERT_EQ(tm1.tm_isdst, tm2.tm_isdst);
            ASSERT_EQ(tm1.tm_gmtoff, tm2.tm_gmtoff);
            ASSERT_STREQ(tm1.tm_zone, tm2.tm_zone);
            ::strftime(&text1[0], text1.size(), spec.c_str(), &tm1);
            ::strftime(&text2[0], text2.size(), spec.c_str(), &tm2);
            ASSERT_STREQ(text1.c_str(), text2.c_str());
        }
    }
}

TEST(time, calculate_near_sequentially_correct) {
    auto now = time(nullptr);
    ::std::mt19937_64 gen {::std::random_device{}()};
    // 10年覆盖了至少一个闰年
    for (size_t i = 0; i < 10; ++i) {
        for (size_t j = 0; j < 365; ++j) {
            // 从随机时间点开始连续推演10分钟
            // 整体向前但也有部分倒退验证功能正确
            size_t base = gen() % (24 * 60 * 60);
            for (size_t k = 0; k < 600; ++k) {
                time_t time = now + (i * 365 + j) * 24 * 60 * 60 +
                    base + k + gen() % 4;
                struct ::tm tm1;
                localtime_r(&time, &tm1);
                struct ::tm tm2;
                ::babylon::localtime(&time, &tm2);
                ASSERT_EQ(tm1.tm_sec, tm2.tm_sec);
                ASSERT_EQ(tm1.tm_min, tm2.tm_min);
                ASSERT_EQ(tm1.tm_hour, tm2.tm_hour);
                ASSERT_EQ(tm1.tm_mday, tm2.tm_mday);
                ASSERT_EQ(tm1.tm_mon, tm2.tm_mon);
                ASSERT_EQ(tm1.tm_year, tm2.tm_year);
                ASSERT_EQ(tm1.tm_wday, tm2.tm_wday);
                ASSERT_EQ(tm1.tm_yday, tm2.tm_yday);
                ASSERT_EQ(tm1.tm_isdst, tm2.tm_isdst);
            }
        }
    }
}
