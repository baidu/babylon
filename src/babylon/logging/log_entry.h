#pragma once

#include "babylon/io/entry.h" // babylon::io::Entry

BABYLON_NAMESPACE_BEGIN

// 表达一条日志的结构，实际内容分页存储管理
// 由于需要做缓存行隔离，因此结构本身可以内联存储少量页指针
// 当日志页超过可内联容量时，展开单链表组织的独立页表进行管理
using LogEntry = io::Entry;

// 支持渐进式构造一条日志
// 采用std::streambuf接口实现，便于集成到std::ostream格式化机制
//
// 极简用法
// LogStreamBuffer streambuf;
// streambuf.set_page_allocator(allocator);
// loop:
//   streambuf.begin();
//   streambuf.sputn(..., sizeof(...));
//   streambuf.sputn(..., sizeof(...));
//   appender.write(streambuf.end(), file_object);
using LogStreamBuffer = io::EntryBuffer;

BABYLON_NAMESPACE_END
