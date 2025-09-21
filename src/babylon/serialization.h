#pragma once

#include "babylon/serialization/traits.h"

#include "google/protobuf/stubs/common.h" // GOOGLE_PROTOBUF_VERSION

#if BABYLON_USE_PROTOBUF && GOOGLE_PROTOBUF_VERSION >= 3000000

////////////////////////////////////////////////////////////////////////////////
// 内置类一些常用数据类型的序列化支持
// 方便大多数情况下可以利用宏声明即可实现组合类型的序列化，内置支持的类型包含
//
// 基础类型：bool, int*_t, uint*_t, float, double, enum, enum class
// 智能指针：std::unique_ptr<T>, std::shared_ptr<T>
// 基础容器：T[], std::string, std::vector<T>, std::list<T>,
//           std::unordered_map<K, V>, std::unordered_set<T>
// protobuf：google::protobuf::MessageLite的子类
#include "babylon/serialization/aggregate.h"
#include "babylon/serialization/array.h"
#include "babylon/serialization/list.h"
#include "babylon/serialization/message.h"
#include "babylon/serialization/scalar.h"
#include "babylon/serialization/shared_ptr.h"
#include "babylon/serialization/string.h"
#include "babylon/serialization/unique_ptr.h"
#include "babylon/serialization/unordered_map.h"
#include "babylon/serialization/unordered_set.h"
#include "babylon/serialization/vector.h"
////////////////////////////////////////////////////////////////////////////////

#endif // BABYLON_USE_PROTOBUF && GOOGLE_PROTOBUF_VERSION >= 3000000
