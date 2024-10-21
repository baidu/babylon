#include "babylon/application_context.h"

#include "gtest/gtest.h"

struct ApplicationContextTest : public ::testing::Test {
  using ApplicationContext = ::babylon::ApplicationContext;
  template <typename T, typename... BS>
  using DefaultComponentHolder =
      ApplicationContext::DefaultComponentHolder<T, BS...>;
  template <typename T, typename... BS>
  using FactoryComponentHolder =
      ApplicationContext::FactoryComponentHolder<T, BS...>;
  using Any = ::babylon::Any;

  static size_t construct_times;
  static size_t initialize_times;

  virtual void SetUp() override {
    construct_times = 0;
    initialize_times = 0;
  }

  ::babylon::ApplicationContext context;
};
size_t ApplicationContextTest::construct_times;
size_t ApplicationContextTest::initialize_times;

TEST_F(ApplicationContextTest, get_component_after_register) {
  ASSERT_FALSE(context.component_accessor<::std::string>());
  context.register_component(DefaultComponentHolder<::std::string>::create());
  ASSERT_TRUE(context.component_accessor<::std::string>());
  for (auto& holder : context) {
    ASSERT_EQ(&::babylon::TypeId<::std::string>::ID, holder.type_id());
    ASSERT_EQ(1, holder.accessible_path_number());
  }
}

TEST_F(ApplicationContextTest,
       component_with_same_type_is_ambiguous_to_get_by_type) {
  ASSERT_FALSE(context.component_accessor<::std::string>());
  context.register_component(DefaultComponentHolder<::std::string>::create());
  context.register_component(DefaultComponentHolder<::std::string>::create());
  ASSERT_FALSE(context.component_accessor<::std::string>());
  for (auto& holder : context) {
    ASSERT_EQ(0, holder.accessible_path_number());
  }
}

TEST_F(ApplicationContextTest,
       component_with_same_type_can_disambiguate_by_name) {
  ASSERT_FALSE(context.component_accessor<::std::string>("A"));
  context.register_component(DefaultComponentHolder<::std::string>::create(),
                             "A");
  context.register_component(DefaultComponentHolder<::std::string>::create(),
                             "B");
  ASSERT_FALSE(context.component_accessor<::std::string>());
  ASSERT_TRUE(context.component_accessor<::std::string>("A"));
  ASSERT_TRUE(context.component_accessor<::std::string>("B"));
  ASSERT_FALSE(context.component_accessor<::std::string>("C"));
  for (auto& holder : context) {
    ASSERT_EQ(1, holder.accessible_path_number());
  }
}

TEST_F(ApplicationContextTest,
       component_with_same_type_and_name_is_not_usable) {
  ASSERT_FALSE(context.component_accessor<::std::string>("A"));
  context.register_component(DefaultComponentHolder<::std::string>::create(),
                             "A");
  context.register_component(DefaultComponentHolder<::std::string>::create(),
                             "A");
  ASSERT_FALSE(context.component_accessor<::std::string>());
  ASSERT_FALSE(context.component_accessor<::std::string>("A"));
  for (auto& holder : context) {
    ASSERT_EQ(0, holder.accessible_path_number());
  }
}

TEST_F(ApplicationContextTest,
       component_of_different_type_is_fine_with_same_name) {
  ASSERT_FALSE(context.component_accessor<::std::string>("A"));
  ASSERT_FALSE(context.component_accessor<::std::vector<int>>("A"));
  context.register_component(DefaultComponentHolder<::std::string>::create(),
                             "A");
  context.register_component(
      DefaultComponentHolder<::std::vector<int>>::create(), "A");
  ASSERT_TRUE(context.component_accessor<::std::string>());
  ASSERT_TRUE(context.component_accessor<::std::string>("A"));
  ASSERT_TRUE(context.component_accessor<::std::vector<int>>());
  ASSERT_TRUE(context.component_accessor<::std::vector<int>>("A"));
  for (auto& holder : context) {
    ASSERT_EQ("A", holder.name());
    ASSERT_EQ(2, holder.accessible_path_number());
  }
}

TEST_F(ApplicationContextTest, create_convertible_to_parent_registered) {
  struct F {
    int vf = 1;
  };
  struct M {
    int vm = 2;
  };
  struct X {
    int vx = 3;
  };
  struct S : public F, public M, public X {
    int vs = 4;
  };

  context.register_component(DefaultComponentHolder<S, F, M>::create());
  ASSERT_TRUE(context.component_accessor<S>());
  ASSERT_EQ(4, context.component_accessor<S>().create()->vs);
  ASSERT_TRUE(context.component_accessor<F>());
  ASSERT_EQ(1, context.component_accessor<F>().create()->vf);
  ASSERT_TRUE(context.component_accessor<M>());
  ASSERT_EQ(2, context.component_accessor<M>().create()->vm);
  ASSERT_FALSE(context.component_accessor<X>());
}

TEST_F(ApplicationContextTest, fix_conflict_by_remove_some_convertible_type) {
  struct P {
    int vp = 1;
  };
  struct C : public P {
    int vc = 1;
  };
  struct CAsPHolder : public DefaultComponentHolder<C, P> {
    CAsPHolder() : DefaultComponentHolder {} {
      remove_convertible_type<C>();
    }
    static ::std::unique_ptr<CAsPHolder> create() {
      return {new CAsPHolder, {}};
    }
  };
  {
    context.register_component(DefaultComponentHolder<C>::create());
    context.register_component(DefaultComponentHolder<C, P>::create());
    ASSERT_FALSE(context.component_accessor<C>());
    ASSERT_TRUE(context.component_accessor<P>());
    context.clear();
  }
  {
    context.register_component(DefaultComponentHolder<C>::create());
    context.register_component(CAsPHolder::create());
    ASSERT_TRUE(context.component_accessor<C>());
    ASSERT_TRUE(context.component_accessor<P>());
    context.clear();
  }
  {
    context.register_component(DefaultComponentHolder<C>::create());
    context.register_component(DefaultComponentHolder<C, P>::create(), "P");
    ASSERT_FALSE(context.component_accessor<C>());
    ASSERT_TRUE(context.component_accessor<P>());
    ASSERT_TRUE(context.component_accessor<C>("P"));
    ASSERT_TRUE(context.component_accessor<P>("P"));
    context.clear();
  }
  {
    context.register_component(DefaultComponentHolder<C>::create());
    context.register_component(CAsPHolder::create(), "P");
    ASSERT_TRUE(context.component_accessor<C>());
    ASSERT_TRUE(context.component_accessor<P>());
    ASSERT_FALSE(context.component_accessor<C>("P"));
    ASSERT_TRUE(context.component_accessor<P>("P"));
    context.clear();
  }
}

TEST_F(ApplicationContextTest, create_with_auto_initialize_if_exist) {
  {
    struct Initializable {
      int initialize(ApplicationContext&, const Any&) {
        initialized = 1;
        return 0;
      }
      int initialize(ApplicationContext&) {
        initialized = 2;
        return 0;
      }
      int initialize(const Any&) {
        initialized = 3;
        return 0;
      }
      int initialize() {
        initialized = 4;
        return 0;
      }
      int initialized = 0;
    };
    context.register_component(DefaultComponentHolder<Initializable>::create());
    auto instance = context.component_accessor<Initializable>().create();
    ASSERT_TRUE(instance);
    ASSERT_EQ(1, instance->initialized);
  }
  {
    struct Initializable {
      int initialize(ApplicationContext&) {
        initialized = 2;
        return 0;
      }
      int initialize(const Any&) {
        initialized = 3;
        return 0;
      }
      int initialize() {
        initialized = 4;
        return 0;
      }
      int initialized = 0;
    };
    context.register_component(DefaultComponentHolder<Initializable>::create());
    auto instance = context.component_accessor<Initializable>().create();
    ASSERT_TRUE(instance);
    ASSERT_EQ(2, instance->initialized);
  }
  {
    struct Initializable {
      int initialize(const Any&) {
        initialized = 3;
        return 0;
      }
      int initialize() {
        initialized = 4;
        return 0;
      }
      int initialized = 0;
    };
    context.register_component(DefaultComponentHolder<Initializable>::create());
    auto instance = context.component_accessor<Initializable>().create();
    ASSERT_TRUE(instance);
    ASSERT_EQ(3, instance->initialized);
  }
  {
    struct Initializable {
      int initialize() {
        initialized = 4;
        return 0;
      }
      int initialized = 0;
    };
    context.register_component(DefaultComponentHolder<Initializable>::create());
    auto instance = context.component_accessor<Initializable>().create();
    ASSERT_TRUE(instance);
    ASSERT_EQ(4, instance->initialized);
  }
  {
    context.register_component(DefaultComponentHolder<::std::string>::create());
    auto instance = context.component_accessor<::std::string>().create();
    ASSERT_TRUE(instance);
    instance->assign("10086");
    ASSERT_EQ("10086", *instance);
  }
}

TEST_F(ApplicationContextTest, create_fail_if_auto_initialize_fail) {
  struct Initializable {
    int initialize() {
      return -1;
    }
  };
  context.register_component(DefaultComponentHolder<Initializable>::create());
  ASSERT_TRUE(context.component_accessor<Initializable>());
  ASSERT_FALSE(context.component_accessor<Initializable>().create());
}

TEST_F(ApplicationContextTest, get_as_singleton_only_create_once) {
  struct S {
    S() {
      construct_times++;
    }
    int initialize() {
      initialize_times++;
      return 0;
    }
  };
  context.register_component(DefaultComponentHolder<S>::create());
  auto ptr = context.component_accessor<S>().get();
  ASSERT_NE(nullptr, ptr);
  ASSERT_EQ(1, construct_times);
  ASSERT_EQ(1, initialize_times);
  ASSERT_EQ(ptr, context.component_accessor<S>().get());
  ASSERT_EQ(1, construct_times);
  ASSERT_EQ(1, initialize_times);
}

TEST_F(ApplicationContextTest, singleton_convertible_to_parent_registered) {
  struct F {
    int vf = 1;
  };
  struct M {
    int vm = 2;
  };
  struct X {
    int vx = 3;
  };
  struct S : public F, public M, public X {
    int vs = 4;
  };

  context.register_component(DefaultComponentHolder<S, F, M>::create());
  ASSERT_TRUE(context.component_accessor<S>());
  ASSERT_EQ(4, context.component_accessor<S>().get()->vs);
  ASSERT_TRUE(context.component_accessor<F>());
  ASSERT_EQ(1, context.component_accessor<F>().get()->vf);
  ASSERT_TRUE(context.component_accessor<M>());
  ASSERT_EQ(2, context.component_accessor<M>().get()->vm);
  ASSERT_FALSE(context.component_accessor<X>());
}

TEST_F(ApplicationContextTest, get_singleton_fail_if_auto_initialize_fail) {
  struct Initializable {
    int initialize() {
      return -1;
    }
  };
  context.register_component(DefaultComponentHolder<Initializable>::create());
  ASSERT_TRUE(context.component_accessor<Initializable>());
  ASSERT_FALSE(context.component_accessor<Initializable>().get());
}

TEST_F(ApplicationContextTest,
       create_and_get_singleton_instance_both_support_autowire) {
  class S {
   public:
    ::std::string& s() {
      return *_s;
    }
    ::std::vector<int>& v() {
      return *_v;
    }
    ::std::list<int>& nl_a() {
      return *_nl_a;
    }
    ::std::list<int>& nl_b() {
      return *_nl_b;
    }

   private:
    BABYLON_AUTOWIRE(BABYLON_MEMBER(::std::string, _s)
                         BABYLON_MEMBER(::std::vector<int>, _v)
                             BABYLON_MEMBER(::std::list<int>, _nl_a, "A")
                                 BABYLON_MEMBER(::std::list<int>, _nl_b, "B"))
  };
  context.register_component(DefaultComponentHolder<S>::create());
  context.register_component(DefaultComponentHolder<::std::string>::create());
  context.register_component(
      FactoryComponentHolder<::std::vector<int>>::create());
  context.register_component(DefaultComponentHolder<::std::list<int>>::create(),
                             "A");
  context.register_component(FactoryComponentHolder<::std::list<int>>::create(),
                             "B");
  ASSERT_TRUE(context.component_accessor<S>());
  ASSERT_TRUE(context.component_accessor<S>().get());
  context.component_accessor<S>().get()->s() = "10086";
  context.component_accessor<S>().get()->v().assign({10086});
  context.component_accessor<S>().get()->nl_a().assign({10086});
  context.component_accessor<S>().get()->nl_b().assign({10086});
  ASSERT_TRUE(context.component_accessor<S>().create());
  ASSERT_EQ("10086", context.component_accessor<S>().create()->s());
  ASSERT_TRUE(context.component_accessor<S>().create()->v().empty());
  ASSERT_EQ(1, context.component_accessor<S>().create()->nl_a().size());
  ASSERT_EQ(10086, context.component_accessor<S>().create()->nl_a().front());
  ASSERT_TRUE(context.component_accessor<S>().create()->nl_b().empty());
}

TEST_F(ApplicationContextTest, component_autowire_is_critical) {
  struct S {
    BABYLON_AUTOWIRE(BABYLON_MEMBER(::std::string, _s))
  };
  context.register_component(DefaultComponentHolder<S>::create());
  ASSERT_TRUE(context.component_accessor<S>());
  ASSERT_FALSE(context.component_accessor<S>().create());
  ASSERT_FALSE(context.component_accessor<S>().get());
}

TEST_F(ApplicationContextTest, component_autowire_before_initialize) {
  struct S {
    int initialize() {
      s = *_s;
      return 0;
    }
    BABYLON_AUTOWIRE(BABYLON_MEMBER(::std::string, _s))
    ::std::string s;
  };
  context.register_component(DefaultComponentHolder<S>::create());
  context.register_component(DefaultComponentHolder<::std::string>::create());
  *context.component_accessor<::std::string>().get() = "10086";
  ASSERT_EQ("10086", context.component_accessor<S>().create()->s);
}

TEST_F(ApplicationContextTest, component_create_with_empty_option_by_default) {
  struct S {
    int initialize(const Any& option) {
      o = option;
      return 0;
    }
    Any o {1};
  };
  context.register_component(DefaultComponentHolder<S>::create());
  ASSERT_TRUE(context.component_accessor<S>());
  ASSERT_TRUE(context.component_accessor<S>().create());
  ASSERT_FALSE(context.component_accessor<S>().create()->o);
}

TEST_F(ApplicationContextTest, component_create_with_external_option_if_given) {
  struct S {
    int initialize(const Any& option) {
      o = option;
      return 0;
    }
    Any o {1};
  };
  context.register_component(DefaultComponentHolder<S>::create());
  ASSERT_TRUE(context.component_accessor<S>());
  ASSERT_TRUE(context.component_accessor<S>().create(Any {10086}));
  ASSERT_EQ(10086,
            context.component_accessor<S>().create(Any {10086})->o.as<int>());
}

TEST_F(ApplicationContextTest, component_create_with_option_set_to_it) {
  struct S {
    int initialize(const Any& option) {
      o = option;
      return 0;
    }
    Any o {1};
  };
  context.register_component(DefaultComponentHolder<S>::create());
  ASSERT_TRUE(context.component_accessor<S>());
  context.component_accessor<S>().set_option(Any {10086});
  ASSERT_EQ(10086, context.component_accessor<S>().create()->o.as<int>());
}

TEST_F(ApplicationContextTest, iterable) {
  struct S {
    int initialize() {
      construct_times++;
      return 0;
    }
  };
  context.register_component(DefaultComponentHolder<S>::create());
  context.register_component(DefaultComponentHolder<S>::create());
  for (auto& component : context) {
    component.get(context);
  }
  ASSERT_EQ(2, construct_times);
}

TEST_F(ApplicationContextTest, can_clear_and_reuse) {
  context.register_component(DefaultComponentHolder<::std::string>::create());
  ASSERT_NE(nullptr, context.get_or_create<::std::string>());
  context.clear();
  ASSERT_EQ(nullptr, context.get_or_create<::std::string>());
  context.register_component(DefaultComponentHolder<::std::string>::create());
  ASSERT_NE(nullptr, context.get_or_create<::std::string>());
}

TEST_F(ApplicationContextTest, default_constructed_component_accessor_empty) {
  ApplicationContext::ComponentAccessor<::std::string> accessor;
  ASSERT_FALSE(accessor);
  ASSERT_FALSE(accessor.get_or_create());
}

TEST_F(ApplicationContextTest, register_empty_component_failed) {
  context.register_component(
      ::std::unique_ptr<DefaultComponentHolder<::std::string>> {});
  for (auto& holder : context) {
    (void)holder;
    ASSERT_TRUE(false);
  }
}

TEST_F(ApplicationContextTest, use_register_helper_to_register_component) {
  struct F {
    int vf = 1;
  };
  struct M {
    int vm = 2;
  };
  struct X {
    int vx = 3;
  };
  struct S : public F, public M, public X {
    int vs = 4;
  };
  struct SS : public F, public M, public X {
    int vs = 5;
  };
  BABYLON_REGISTER_COMPONENT(::std::string)
  BABYLON_REGISTER_COMPONENT(::std::vector<int>, "name1")
  BABYLON_REGISTER_COMPONENT(::std::vector<int>, "name2")
  BABYLON_REGISTER_COMPONENT(S, "", F, M)
  BABYLON_REGISTER_FACTORY_COMPONENT(::std::vector<int>, "name3")
  BABYLON_REGISTER_FACTORY_COMPONENT(SS, "", X)

  ApplicationContext& context = ApplicationContext::instance();
  ASSERT_NE(nullptr, context.get_or_create<::std::string>());
  ASSERT_EQ(nullptr, context.get_or_create<::std::vector<int>>());
  ASSERT_NE(nullptr, context.get_or_create<::std::vector<int>>("name1"));
  ASSERT_NE(nullptr, context.get_or_create<::std::vector<int>>("name2"));
  ASSERT_EQ(nullptr, context.get_or_create<::std::vector<int>>("name4"));
  ASSERT_NE(nullptr, context.get_or_create<S>());
  ASSERT_NE(nullptr, context.get_or_create<F>());
  ASSERT_NE(nullptr, context.get_or_create<M>());
  ASSERT_NE(nullptr, context.get_or_create<X>());
}
