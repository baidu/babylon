#include "babylon/application_context.h"

#include "gtest/gtest.h"

/*
class ContextTest : public ::testing::TestWithParam<bool> {
};
#ifdef INSTANTIATE_TEST_SUITE_P
INSTANTIATE_TEST_SUITE_P(context, ContextTest, ::testing::Bool());
#else // !INSTANTIATE_TEST_SUITE_P
INSTANTIATE_TEST_CASE_P(context, ContextTest, ::testing::Bool());
#endif // !INSTANTIATE_TEST_CASE_P

struct NormalComponent {
    NormalComponent() {
        construct_num++;
    }
    virtual ~NormalComponent() {
        destruct_num++;
    };

    static size_t construct_num;
    static size_t destruct_num;
};

size_t NormalComponent::construct_num = 0;
size_t NormalComponent::destruct_num = 0;

struct AnotherNormalComponent {
};

struct SubNormalComponent : public NormalComponent {
};

template <int32_t R>
struct InitializeableComponent {
    int32_t initialize() noexcept {
        initialized = true;
        return R;
    }
    bool initialized = false;
};

template <int32_t R>
struct WireableComponent {
    int32_t wireup(ApplicationContext&) noexcept {
        wired = true;
        return R;
    }
    bool wired = false;
};

struct AutowireComponent {
    BABYLON_AUTOWIRE(
        BABYLON_MEMBER(NormalComponent*, nc)
        BABYLON_MEMBER(const NormalComponent*, cnc)
        BABYLON_MEMBER_BY_NAME(AnotherNormalComponent*, anc1, name1)
        BABYLON_MEMBER_BY_NAME(AnotherNormalComponent*, anc2, name2)
    )
};

struct ParentAutowireComponent {
    BABYLON_AUTOWIRE(
        BABYLON_MEMBER(AutowireComponent*, child)
    )
};

// 不可默认构造
template <int32_t R>
struct InitializedComponent {
    InitializedComponent(int32_t) {}
    int32_t initialize() noexcept {
        initialized = true;
        return R;
    }

    bool initialized = false;
    int32_t init_value = 0;
    BABYLON_AUTOWIRE(
        BABYLON_MEMBER(NormalComponent*, nc)
    )
};
*/

struct ApplicationContextTest : public ::testing::Test {

    ::babylon::ApplicationContext context;
};

TEST_F(ApplicationContextTest, both_singleton_or_instance_is_ok) {
    //context.get_or_create<NormalComponent>();
    //ApplicationContext::instance().get_or_create<NormalComponent>();

  struct A {
    int initialize(::babylon::ApplicationContext&, const ::babylon::Any&) {
      return 0;
    }
  };
  struct B {
  };
  struct C : public A, B {
  };
  ::babylon::DefaultComponentHolder<C, A, B> h;
  h.create<C>(context);
  h.create<A>(context);
}

/*
TEST_P(ContextTest, initialize_context_before_use) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<NormalComponent>());
    // 初始化前无法获取组件
    ASSERT_EQ(nullptr, context.get_or_create<NormalComponent>());
    ASSERT_EQ(0, context.initialize(GetParam()));
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>());
}

TEST_P(ContextTest, register_and_get_by_type_exactly) {
    ApplicationContext context;
    // 可以不用起名
    context.register_component(DefaultComponentHolder<SubNormalComponent>());
    ASSERT_EQ(0, context.initialize(GetParam()));
    // 不同类型取不到
    ASSERT_EQ(nullptr, context.get_or_create<AnotherNormalComponent>());
    // 不支持使用基类获取
    ASSERT_EQ(nullptr, context.get_or_create<NormalComponent>());
    // 使用模版参数指定，严格匹配
    ASSERT_NE(nullptr, context.get_or_create<SubNormalComponent>());
}

TEST_P(ContextTest, support_same_name_with_different_type) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<NormalComponent>(), "name");
    context.register_component(DefaultComponentHolder<AnotherNormalComponent>(), "name");
    ASSERT_EQ(0, context.initialize(GetParam()));
    // 可以不指定名字
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>());
    ASSERT_NE(nullptr, context.get_or_create<AnotherNormalComponent>());
    // 检测没有误返回相同对象
    ASSERT_NE((void*)context.get_or_create<NormalComponent>().get(),
        (void*)context.get_or_create<AnotherNormalComponent>().get());
}

TEST_P(ContextTest, support_same_type_with_different_name) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<NormalComponent>(), "name1");
    context.register_component(DefaultComponentHolder<NormalComponent>(), "name2");
    ASSERT_EQ(0, context.initialize(GetParam()));
    // 类型冲突，不指定名字无法获取
    ASSERT_EQ(nullptr, context.get_or_create<NormalComponent>());
    // 指定名字可以拿到对应组件
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>("name1"));
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>("name2"));
    ASSERT_NE(context.get_or_create<NormalComponent>("name1"),
        context.get_or_create<NormalComponent>("name2"));
}

TEST_P(ContextTest, reject_both_same_type_and_name) {
    // 同类型同名字会被拒绝，因为无法区分定位
    {
        ApplicationContext context;
        context.register_component(DefaultComponentHolder<NormalComponent>(), "name1");
        context.register_component(DefaultComponentHolder<NormalComponent>(), "name1");
        ASSERT_NE(0, context.initialize(GetParam()));
    }
    // 没有名字视为名字是""，等同于同名
    {
        ApplicationContext context;
        context.register_component(DefaultComponentHolder<NormalComponent>());
        context.register_component(DefaultComponentHolder<NormalComponent>());
        ASSERT_NE(0, context.initialize(GetParam()));
    }
}

TEST_P(ContextTest, initialize_is_called_automatically_if_exist) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<InitializeableComponent<0>>());
    ASSERT_EQ(0, context.initialize(GetParam()));
    ASSERT_NE(nullptr, context.get_or_create<InitializeableComponent<0>>());
    ASSERT_TRUE(context.get_or_create<InitializeableComponent<0>>()->initialized);
}

TEST(ContextTest, component_initialize_is_critical) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<InitializeableComponent<1>>());
    ASSERT_NE(0, context.initialize(false));
}

TEST(ContextTest, component_initialize_is_critical_but_lazy_fail_when_get) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<InitializeableComponent<1>>());
    ASSERT_EQ(0, context.initialize(true));
    ASSERT_EQ(nullptr, context.get_or_create<InitializeableComponent<1>>());
}

TEST_P(ContextTest, wireup_is_called_automatically_if_exist) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<WireableComponent<0>>());
    ASSERT_EQ(0, context.initialize(GetParam()));
    ASSERT_NE(nullptr, context.get_or_create<WireableComponent<0>>());
    ASSERT_TRUE(context.get_or_create<WireableComponent<0>>()->wired);
}

TEST(ContextTest, component_wireup_is_critical) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<WireableComponent<1>>());
    ASSERT_NE(0, context.initialize(false));
}

TEST(ContextTest, component_wireup_is_critical_but_lazy_fail_when_get) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<WireableComponent<1>>());
    ASSERT_EQ(0, context.initialize(true));
    ASSERT_EQ(nullptr, context.get_or_create<WireableComponent<1>>());
}

TEST_P(ContextTest, perform_autowire_when_declared) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<NormalComponent>());
    context.register_component(DefaultComponentHolder<AutowireComponent>());
    context.register_component(DefaultComponentHolder<AnotherNormalComponent>(), "name1");
    context.register_component(DefaultComponentHolder<AnotherNormalComponent>(), "name2");
    ASSERT_EQ(0, context.initialize(GetParam()));
    ASSERT_NE(nullptr, context.get_or_create<AutowireComponent>());
    ASSERT_NE(nullptr, context.get_or_create<AutowireComponent>()->nc);
    ASSERT_NE(nullptr, context.get_or_create<AutowireComponent>()->cnc);
    ASSERT_NE(nullptr, context.get_or_create<AutowireComponent>()->anc1);
    ASSERT_NE(nullptr, context.get_or_create<AutowireComponent>()->anc2);
    ASSERT_EQ(context.get_or_create<NormalComponent>(), context.get_or_create<AutowireComponent>()->nc);
    ASSERT_EQ(context.get_or_create<NormalComponent>(), context.get_or_create<AutowireComponent>()->cnc);
    ASSERT_EQ(context.get_or_create<AnotherNormalComponent>("name1"),
        context.get_or_create<AutowireComponent>()->anc1);
    ASSERT_EQ(context.get_or_create<AnotherNormalComponent>("name2"),
        context.get_or_create<AutowireComponent>()->anc2);
}

TEST_P(ContextTest, perform_autowire_when_included) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<NormalComponent>());
    context.register_component(DefaultComponentHolder<AutowireComponent>());
    context.register_component(DefaultComponentHolder<AnotherNormalComponent>(), "name1");
    context.register_component(DefaultComponentHolder<AnotherNormalComponent>(), "name2");
    context.register_component(DefaultComponentHolder<ParentAutowireComponent>());
    ASSERT_EQ(0, context.initialize(GetParam()));
    ASSERT_NE(nullptr, context.get_or_create<AutowireComponent>());
    ASSERT_NE(nullptr, context.get_or_create<AutowireComponent>()->nc);
    ASSERT_NE(nullptr, context.get_or_create<AutowireComponent>()->cnc);
    ASSERT_NE(nullptr, context.get_or_create<AutowireComponent>()->anc1);
    ASSERT_NE(nullptr, context.get_or_create<AutowireComponent>()->anc2);
    ASSERT_NE(nullptr, context.get_or_create<ParentAutowireComponent>());
    ASSERT_EQ(context.get_or_create<NormalComponent>(), context.get_or_create<AutowireComponent>()->nc);
    ASSERT_EQ(context.get_or_create<NormalComponent>(), context.get_or_create<ParentAutowireComponent>()->child->cnc);
    ASSERT_EQ(context.get_or_create<AutowireComponent>(), context.get_or_create<ParentAutowireComponent>()->child);
    ASSERT_EQ(context.get_or_create<AnotherNormalComponent>("name1"),
        context.get_or_create<AutowireComponent>()->anc1);
    ASSERT_EQ(context.get_or_create<AnotherNormalComponent>("name2"),
        context.get_or_create<AutowireComponent>()->anc2);
}

TEST(ContextTest, autowire_is_critical) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<AutowireComponent>());
    ASSERT_NE(0, context.initialize(false));
}

TEST(ContextTest, autowire_is_critical_but_lazy_fail_when_get) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<AutowireComponent>());
    ASSERT_EQ(0, context.initialize(true));
    ASSERT_EQ(nullptr, context.get_or_create<AutowireComponent>());
}

TEST_P(ContextTest, support_initialized_instance_as_component) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<NormalComponent>());
    context.register_component(InitializedComponentHolder<InitializedComponent<1>>(
            InitializedComponent<1>(0)));
    // 忽略initialize函数，但是可以获得对象，并正确装配
    ASSERT_EQ(0, context.initialize(GetParam()));
    ASSERT_NE(nullptr, context.get_or_create<InitializedComponent<1>>());
    ASSERT_FALSE(context.get_or_create<InitializedComponent<1>>()->initialized);
    ASSERT_EQ(context.get_or_create<NormalComponent>(),
        context.get_or_create<InitializedComponent<1>>()->nc);
}

TEST_P(ContextTest, support_unmanaged_instance_as_component) {
    ApplicationContext context;
    InitializedComponent<1> component(0);
    context.register_component(DefaultComponentHolder<NormalComponent>());
    context.register_component(ExternalComponentHolder<InitializedComponent<1>>(component));
    // 忽略initialize函数，但是可以获得对象，并正确装配
    // 得到的对象就是传入的component
    ASSERT_EQ(0, context.initialize(GetParam()));
    ASSERT_NE(nullptr, context.get_or_create<InitializedComponent<1>>());
    ASSERT_FALSE(context.get_or_create<InitializedComponent<1>>()->initialized);
    ASSERT_EQ(context.get_or_create<NormalComponent>(),
        context.get_or_create<InitializedComponent<1>>()->nc);
    ASSERT_EQ(&component, context.get_or_create<InitializedComponent<1>>().get());
}

TEST_P(ContextTest, support_custom_component) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<NormalComponent>());
    context.register_component(CustomComponentHolder<InitializedComponent<1>>([] () {
        auto component = new InitializedComponent<1>(1);
        component->init_value = 2;
        return component;
    }));

    // 调用自定义初始化，并正确装配
    ASSERT_EQ(0, context.initialize(GetParam()));
    ASSERT_NE(nullptr, context.get_or_create<InitializedComponent<1>>());
    ASSERT_FALSE(context.get_or_create<InitializedComponent<1>>()->initialized);
    ASSERT_EQ(context.get_or_create<NormalComponent>(),
        context.get_or_create<InitializedComponent<1>>()->nc);
    ASSERT_EQ(2, context.get_or_create<InitializedComponent<1>>()->init_value);
}

TEST_P(ContextTest, support_instance_as_factory_component) {
    ApplicationContext context;
    context.register_component(DefaultFactoryComponentHolder<NormalComponent>());
    ASSERT_EQ(0, context.initialize(GetParam()));
    context.get_or_create<NormalComponent>();
    NormalComponent::construct_num = 0;
    NormalComponent::destruct_num = 0;
    {
        auto instance1 = context.get_or_create<NormalComponent>();
        ASSERT_EQ(1, NormalComponent::construct_num);
        ASSERT_EQ(0, NormalComponent::destruct_num);
        {
            auto instance2 = context.get_or_create<NormalComponent>();
            ASSERT_NE(instance1, instance2);
            ASSERT_EQ(2, NormalComponent::construct_num);
            ASSERT_EQ(0, NormalComponent::destruct_num);
        }
        ASSERT_EQ(2, NormalComponent::construct_num);
        ASSERT_EQ(1, NormalComponent::destruct_num);
    }
    ASSERT_EQ(2, NormalComponent::construct_num);
    ASSERT_EQ(2, NormalComponent::destruct_num);
}

TEST_P(ContextTest, support_custom_create_instance_as_factory_component) {
    size_t create_count = 0;
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<NormalComponent>());
    context.register_component(CustomFactoryComponentHolder<InitializedComponent<1>>(
            [&create_count] () {
        auto component = new InitializedComponent<1>(1);
        component->init_value = 2;
        create_count++;
        return component;
    }));

    // 调用自定义初始化，并正确装配
    ASSERT_EQ(0, context.initialize(GetParam()));
    ASSERT_NE(nullptr, context.get_or_create<InitializedComponent<1>>());
    create_count = 0;
    ASSERT_NE(nullptr, context.get_or_create<InitializedComponent<1>>());
    ASSERT_FALSE(context.get_or_create<InitializedComponent<1>>()->initialized);
    ASSERT_EQ(context.get_or_create<NormalComponent>(),
        context.get_or_create<InitializedComponent<1>>()->nc);
    ASSERT_EQ(2, context.get_or_create<InitializedComponent<1>>()->init_value);
    ASSERT_EQ(4, create_count);
}

TEST_P(ContextTest, support_factory_and_singleton_component_wireup_each_other) {
    ApplicationContext context;
    context.register_component(DefaultFactoryComponentHolder<NormalComponent>());
    context.register_component(DefaultComponentHolder<AutowireComponent>());
    context.register_component(DefaultComponentHolder<AnotherNormalComponent>(), "name1");
    context.register_component(DefaultComponentHolder<AnotherNormalComponent>(), "name2");
    context.register_component(DefaultFactoryComponentHolder<ParentAutowireComponent>());
    ASSERT_EQ(0, context.initialize(GetParam()));
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>());
    ASSERT_NE(nullptr, context.get_or_create<AutowireComponent>());
    ASSERT_NE(nullptr, context.get_or_create<AutowireComponent>()->nc);
    ASSERT_NE(nullptr, context.get_or_create<AutowireComponent>()->cnc);
    ASSERT_NE(context.get_or_create<AutowireComponent>()->nc, context.get_or_create<AutowireComponent>()->cnc);
    ASSERT_NE(nullptr, context.get_or_create<ParentAutowireComponent>());
    ASSERT_EQ(context.get_or_create<AutowireComponent>(), context.get_or_create<ParentAutowireComponent>()->child);
}

TEST(ContextTest, use_register_helper_to_register_component) {
    BABYLON_REGISTER_COMPONENT(AnotherNormalComponent);
    BABYLON_REGISTER_COMPONENT_WITH_NAME(NormalComponent, name1);
    BABYLON_REGISTER_COMPONENT_WITH_NAME(NormalComponent, name2);
    BABYLON_REGISTER_COMPONENT_WITH_TYPE_NAME(SubNormalComponent, NormalComponent, name3);
    BABYLON_REGISTER_CUSTOM_COMPONENT(InitializeableComponent<1>, [] () {
        return new InitializeableComponent<1>;
    });
    BABYLON_REGISTER_CUSTOM_COMPONENT_WITH_NAME(NormalComponent, name4, [] () {
        return new NormalComponent();
    });
    BABYLON_REGISTER_CUSTOM_COMPONENT_WITH_NAME(NormalComponent, name5, [] () {
        return new NormalComponent();
    });
    BABYLON_REGISTER_CUSTOM_COMPONENT_WITH_NAME(NormalComponent, name6, [] () {
        return new SubNormalComponent();
    });
    BABYLON_REGISTER_CUSTOM_COMPONENT_WITH_TYPE_NAME(SubNormalComponent, NormalComponent, name7, [] () {
        return new SubNormalComponent();
    });
    BABYLON_REGISTER_FACTORY_COMPONENT(NormalComponent);
    BABYLON_REGISTER_FACTORY_COMPONENT_WITH_NAME(NormalComponent, name8);
    BABYLON_REGISTER_FACTORY_COMPONENT_WITH_TYPE_NAME(SubNormalComponent, NormalComponent, name9);
    BABYLON_REGISTER_CUSTOM_COMPONENT(InitializeableComponent<2>, [] () {
        return new InitializeableComponent<2>;
    });
    BABYLON_REGISTER_CUSTOM_FACTORY_COMPONENT_WITH_NAME(NormalComponent, name10, [] () {
        return new NormalComponent();
    });
    BABYLON_REGISTER_CUSTOM_FACTORY_COMPONENT_WITH_TYPE_NAME(SubNormalComponent, NormalComponent, name11, [] () {
        return new SubNormalComponent();
    });

    ApplicationContext& context = ApplicationContext::instance();
    ASSERT_EQ(0, context.initialize(true));
    ASSERT_EQ(nullptr, context.get_or_create<NormalComponent>());
    ASSERT_NE(nullptr, context.get_or_create<AnotherNormalComponent>());
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>("name1"));
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>("name2"));
    ASSERT_NE(nullptr, dynamic_cast<SubNormalComponent*>(context.get_or_create<NormalComponent>("name3").get()));
    ASSERT_NE(nullptr, context.get_or_create<InitializeableComponent<1>>());
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>("name4"));
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>("name5"));
    ASSERT_NE(nullptr, dynamic_cast<SubNormalComponent*>(context.get_or_create<NormalComponent>("name6").get()));
    ASSERT_NE(nullptr, dynamic_cast<SubNormalComponent*>(context.get_or_create<NormalComponent>("name7").get()));
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>(""));
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>("name8"));
    ASSERT_NE(nullptr, dynamic_cast<SubNormalComponent*>(context.get_or_create<NormalComponent>("name9").get()));
    ASSERT_NE(nullptr, context.get_or_create<InitializeableComponent<2>>());
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>("name10"));
    ASSERT_NE(nullptr, dynamic_cast<SubNormalComponent*>(context.get_or_create<NormalComponent>("name11").get()));
}

TEST_P(ContextTest, can_clear_and_reuse) {
    ApplicationContext context;

    context.register_component(DefaultComponentHolder<NormalComponent>());
    ASSERT_EQ(0, context.initialize(GetParam()));
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>());

    context.clear();
    ASSERT_EQ(nullptr, context.get_or_create<NormalComponent>());

    context.register_component(DefaultComponentHolder<NormalComponent>());
    ASSERT_EQ(0, context.initialize(GetParam()));
    ASSERT_NE(nullptr, context.get_or_create<NormalComponent>());
}

TEST(application_context, default_constructed_component_accessor_empty) {
    ApplicationContext::ComponentAccessor<NormalComponent> accessor;
    ASSERT_FALSE(accessor);
    ASSERT_FALSE(accessor.get_or_create());
}

TEST(application_context, get_component_by_accessor) {
    ApplicationContext context;
    context.register_component(DefaultComponentHolder<NormalComponent>());
    context.register_component(DefaultFactoryComponentHolder<AnotherNormalComponent>());
    ASSERT_EQ(0, context.initialize(true));
    {
        auto accessor = context.get_component_accessor<NormalComponent>();
        ASSERT_TRUE(accessor);
        ASSERT_NE(nullptr, accessor.get_or_create().get());
        ASSERT_EQ(accessor.get_or_create(), accessor.get_or_create());
    }
    {
        auto accessor = context.get_component_accessor<AnotherNormalComponent>();
        ASSERT_TRUE(accessor);
        ASSERT_NE(nullptr, accessor.get_or_create().get());
        ASSERT_NE(accessor.get_or_create(), accessor.get_or_create());
    }
}
*/
