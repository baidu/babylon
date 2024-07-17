#include "babylon/logging/logger.h"

#include "gtest/gtest.h"

struct LoggerTest : public ::testing::Test {
  virtual void SetUp() override {
    ::babylon::LoggerManager::instance().~LoggerManager();
    new (&::babylon::LoggerManager::instance())::babylon::LoggerManager;
  }

  virtual void TearDown() override {}

  ::std::stringbuf buffer;
  ::babylon::LoggerBuilder builder;

  ::std::stringbuf buffer2;
};

TEST_F(LoggerTest, default_logger_do_minimal_job) {
  ::babylon::Logger logger;
  BABYLON_LOG_STREAM(logger, INFO) << "this text appear in stderr";
  ASSERT_FALSE(logger.initialized());
  ASSERT_EQ(::babylon::LogSeverity::DEBUG, logger.min_severity());
}

TEST_F(LoggerTest, default_builder_build_default_logger_in_production_mode) {
  auto logger = builder.build();
  BABYLON_LOG_STREAM(logger, INFO) << "this text also appear in stderr";
  ASSERT_TRUE(logger.initialized());
  ASSERT_EQ(::babylon::LogSeverity::INFO, logger.min_severity());
}

TEST_F(LoggerTest, uninitialized_manager_get_uninitialized_root_logger) {
  auto& logger = ::babylon::LoggerManager::instance().get_root_logger();
  BABYLON_LOG_STREAM(logger, INFO) << "this text also appear in stderr";
  ASSERT_FALSE(logger.initialized());
  ASSERT_EQ(::babylon::LogSeverity::DEBUG, logger.min_severity());
}

TEST_F(LoggerTest, uninitialized_manager_get_uninitialized_named_logger) {
  auto& logger = ::babylon::LoggerManager::instance().get_logger("name");
  BABYLON_LOG_STREAM(logger, INFO) << "this text also appear in stderr";
  ASSERT_FALSE(logger.initialized());
  ASSERT_EQ(::babylon::LogSeverity::DEBUG, logger.min_severity());
}

TEST_F(LoggerTest, assign_stream_for_severity) {
  builder.set_log_stream_creator(::babylon::LogSeverity::INFO, [this] {
    auto ptr = new ::babylon::LogStream(buffer);
    return ::std::unique_ptr<::babylon::LogStream>(ptr);
  });
  auto logger = builder.build();
  ASSERT_TRUE(logger.initialized());
  ASSERT_EQ(::babylon::LogSeverity::INFO, logger.min_severity());
  BABYLON_LOG_STREAM(logger, INFO) << "this text appear in string";
  BABYLON_LOG_STREAM(logger, WARNING) << "this text appear in stderr";
  ASSERT_EQ("this text appear in string", buffer.str());
}

TEST_F(LoggerTest, assign_stream_for_all_severity) {
  builder.set_log_stream_creator([this] {
    auto ptr = new ::babylon::LogStream(buffer);
    return ::std::unique_ptr<::babylon::LogStream>(ptr);
  });
  auto logger = builder.build();
  ASSERT_TRUE(logger.initialized());
  ASSERT_EQ(::babylon::LogSeverity::INFO, logger.min_severity());
  BABYLON_LOG_STREAM(logger, DEBUG) << "this text is ignore";
  BABYLON_LOG_STREAM(logger, INFO) << "this text appear in string";
  BABYLON_LOG_STREAM(logger, WARNING) << "this text also appear in string";
  ASSERT_EQ(
      "this text appear in string"
      "this text also appear in string",
      buffer.str());
}

TEST_F(LoggerTest, stream_has_correct_basic_info) {
  builder.set_log_stream_creator(::babylon::LogSeverity::INFO, [this] {
    auto ptr = new ::babylon::LogStream(buffer);
    return ::std::unique_ptr<::babylon::LogStream>(ptr);
  });
  builder.set_min_severity(::babylon::LogSeverity::DEBUG);
  auto logger = builder.build();
  ASSERT_TRUE(logger.initialized());
  ASSERT_EQ(::babylon::LogSeverity::DEBUG, logger.min_severity());
  for (auto i = 0; i < static_cast<int>(::babylon::LogSeverity::NUM); ++i) {
    auto severity = static_cast<::babylon::LogSeverity>(i);
    ASSERT_EQ(severity, logger.stream(severity, __FILE__, __LINE__).severity());
    ASSERT_EQ(__FILE__, logger.stream(severity, __FILE__, __LINE__).file());
    ASSERT_EQ(__LINE__, logger.stream(severity, __FILE__, __LINE__).line());
  }
}

TEST_F(LoggerTest, apply_empty_manager_get_default_production_logger) {
  ::babylon::LoggerManager::instance().apply();
  {
    auto& logger = ::babylon::LoggerManager::instance().get_root_logger();
    BABYLON_LOG_STREAM(logger, INFO) << "this text also appear in stderr";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::INFO, logger.min_severity());
  }
  {
    auto& logger = ::babylon::LoggerManager::instance().get_logger("name");
    BABYLON_LOG_STREAM(logger, INFO) << "this text also appear in stderr";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::INFO, logger.min_severity());
  }
}

TEST_F(LoggerTest, builder_set_to_manager_cover_a_sub_tree) {
  builder.set_log_stream_creator([this] {
    auto ptr = new ::babylon::LogStream(buffer);
    return ::std::unique_ptr<::babylon::LogStream>(ptr);
  });
  builder.set_min_severity(::babylon::LogSeverity::WARNING);
  ::babylon::LoggerManager::instance().set_builder("a.b", ::std::move(builder));
  ::babylon::LoggerManager::instance().apply();
  {
    auto& logger = ::babylon::LoggerManager::instance().get_root_logger();
    BABYLON_LOG_STREAM(logger, INFO) << "this text appear in stderr";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::INFO, logger.min_severity());
  }
  {
    auto& logger = ::babylon::LoggerManager::instance().get_logger("a.bc");
    BABYLON_LOG_STREAM(logger, INFO) << "this text appear in stderr";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::INFO, logger.min_severity());
  }
  {
    auto& logger = ::babylon::LoggerManager::instance().get_logger("a.b");
    BABYLON_LOG_STREAM(logger, INFO) << "this text also appear in stderr";
    BABYLON_LOG_STREAM(logger, WARNING) << "this text appear in string";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::WARNING, logger.min_severity());
  }
  {
    auto& logger = ::babylon::LoggerManager::instance().get_logger("a.b.c");
    BABYLON_LOG_STREAM(logger, INFO) << "this text also appear in stderr";
    BABYLON_LOG_STREAM(logger, WARNING) << "this text also appear in string";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::WARNING, logger.min_severity());
  }
  {
    auto& logger = ::babylon::LoggerManager::instance().get_logger("a.b::c");
    BABYLON_LOG_STREAM(logger, INFO) << "this text also appear in stderr";
    BABYLON_LOG_STREAM(logger, WARNING) << "this text still appear in string";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::WARNING, logger.min_severity());
  }
  ASSERT_EQ(
      "this text appear in string"
      "this text also appear in string"
      "this text still appear in string",
      buffer.str());
}

TEST_F(LoggerTest, builder_set_to_root_cover_all) {
  builder.set_log_stream_creator([this] {
    auto ptr = new ::babylon::LogStream(buffer);
    return ::std::unique_ptr<::babylon::LogStream>(ptr);
  });
  builder.set_min_severity(::babylon::LogSeverity::WARNING);
  ::babylon::LoggerManager::instance().set_root_builder(::std::move(builder));
  ::babylon::LoggerManager::instance().apply();
  {
    auto& logger = ::babylon::LoggerManager::instance().get_root_logger();
    BABYLON_LOG_STREAM(logger, INFO) << "this text appear in stderr";
    BABYLON_LOG_STREAM(logger, WARNING) << "this text appear in string";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::WARNING, logger.min_severity());
  }
  {
    auto& logger = ::babylon::LoggerManager::instance().get_logger("a.b");
    BABYLON_LOG_STREAM(logger, INFO) << "this text also appear in stderr";
    BABYLON_LOG_STREAM(logger, WARNING) << "this text also appear in string";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::WARNING, logger.min_severity());
  }
  {
    auto& logger = ::babylon::LoggerManager::instance().get_logger("a");
    BABYLON_LOG_STREAM(logger, INFO) << "this text also appear in stderr";
    BABYLON_LOG_STREAM(logger, WARNING) << "this text still appear in string";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::WARNING, logger.min_severity());
  }
  ASSERT_EQ(
      "this text appear in string"
      "this text also appear in string"
      "this text still appear in string",
      buffer.str());
}

TEST_F(LoggerTest, builder_set_cover_logger_get_before) {
  auto root_logger = &::babylon::LoggerManager::instance().get_root_logger();
  auto a_logger = &::babylon::LoggerManager::instance().get_logger("a");
  auto a_b_logger = &::babylon::LoggerManager::instance().get_logger("a.b");
  auto a_b_c_logger = &::babylon::LoggerManager::instance().get_logger("a.b.c");
  BABYLON_LOG_STREAM(*root_logger, INFO) << "this text appear in stderr";
  BABYLON_LOG_STREAM(*a_logger, INFO) << "this text appear in stderr";
  BABYLON_LOG_STREAM(*a_b_logger, INFO) << "this text appear in stderr";
  BABYLON_LOG_STREAM(*a_b_c_logger, INFO) << "this text appear in stderr";
  ASSERT_FALSE(root_logger->initialized());
  ASSERT_FALSE(a_logger->initialized());
  ASSERT_FALSE(a_b_logger->initialized());
  ASSERT_FALSE(a_b_c_logger->initialized());
  ASSERT_EQ(::babylon::LogSeverity::DEBUG, root_logger->min_severity());
  ASSERT_EQ(::babylon::LogSeverity::DEBUG, a_logger->min_severity());
  ASSERT_EQ(::babylon::LogSeverity::DEBUG, a_b_logger->min_severity());
  ASSERT_EQ(::babylon::LogSeverity::DEBUG, a_b_c_logger->min_severity());
  builder.set_log_stream_creator([this] {
    auto ptr = new ::babylon::LogStream(buffer);
    return ::std::unique_ptr<::babylon::LogStream>(ptr);
  });
  builder.set_min_severity(::babylon::LogSeverity::WARNING);
  ::babylon::LoggerManager::instance().set_root_builder(::std::move(builder));
  builder.set_log_stream_creator([this] {
    auto ptr = new ::babylon::LogStream(buffer2);
    return ::std::unique_ptr<::babylon::LogStream>(ptr);
  });
  builder.set_min_severity(::babylon::LogSeverity::WARNING);
  ::babylon::LoggerManager::instance().set_builder("a.b", ::std::move(builder));
  ::babylon::LoggerManager::instance().apply();
  {
    auto& logger = *root_logger;
    BABYLON_LOG_STREAM(logger, INFO) << "this text appear in stderr";
    BABYLON_LOG_STREAM(logger, WARNING) << "this text appear in root";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::WARNING, logger.min_severity());
  }
  {
    auto& logger = *a_logger;
    BABYLON_LOG_STREAM(logger, INFO) << "this text also appear in stderr";
    BABYLON_LOG_STREAM(logger, WARNING) << "this text also appear in root";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::WARNING, logger.min_severity());
  }
  {
    auto& logger = *a_b_logger;
    BABYLON_LOG_STREAM(logger, INFO) << "this text also appear in stderr";
    BABYLON_LOG_STREAM(logger, WARNING) << "this text appear in a.b";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::WARNING, logger.min_severity());
  }
  {
    auto& logger = *a_b_c_logger;
    BABYLON_LOG_STREAM(logger, INFO) << "this text also appear in stderr";
    BABYLON_LOG_STREAM(logger, WARNING) << "this text also appear in a.b";
    ASSERT_TRUE(logger.initialized());
    ASSERT_EQ(::babylon::LogSeverity::WARNING, logger.min_severity());
  }
  ASSERT_EQ(
      "this text appear in root"
      "this text also appear in root",
      buffer.str());
  ASSERT_EQ(
      "this text appear in a.b"
      "this text also appear in a.b",
      buffer2.str());
}

TEST_F(LoggerTest, concise_log_macro_use_root_logger) {
  builder.set_log_stream_creator([this] {
    auto ptr = new ::babylon::LogStream(buffer);
    return ::std::unique_ptr<::babylon::LogStream>(ptr);
  });
  builder.set_min_severity(::babylon::LogSeverity::INFO);
  ::babylon::LoggerManager::instance().set_root_builder(::std::move(builder));
  ::babylon::LoggerManager::instance().apply();
  BABYLON_LOG(INFO) << "this text appear in root";
  ASSERT_EQ("this text appear in root", buffer.str());
}
