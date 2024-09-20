#pragma once

// clang-format off
// Any class with selectable features and small object optimization.
// N - is the maximum size of the small objects
// For the features, see the README.md files.
// clang-format on

#include <algorithm>
#include <any>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>

#include "type_name.hpp"

namespace ext {

    // clang-format off
// any_properties - The properties of an any<N,Fs...>. The Features based properties are gathered using inheritance.
//  It includes the template parameters and values, type_id information, and the different operations.
//  Basic function operations: typeid, type_name, clone, destroy, destruct, copy constructor, move constructor, copy assignment, move assignment,
//  and the extra operation and properties from the Features
    // clang-format on

    // forward declarations.
    template <std::size_t N, template <typename> class... Features>
    struct any;
    template <typename T, typename A>
    struct any_properties_t_data_type;

    // The any_type_tag<> is used as first argument for any<N> constructor - to specify the type we
    // want, in case it is different from the variable type provided to the constructor. Similar to
    // std::in_place_type_t
    template <typename T>
    struct any_type_tag {
        using type = T;
    };

    template <typename T>
    struct is_an_any;  // true_type for all any<N, ...> types, false_type otherwise.
    template <typename T>
    constexpr bool is_an_any_v{is_an_any<T>::value};
    template <typename T>
    struct is_an_any : std::false_type {};
    template <std::size_t N, template <typename> class... Features>
    struct is_an_any<any<N, Features...>> : std::true_type {};

    template <typename T>
    constexpr std::size_t required_size() {
        if constexpr (requires { T::min_required_size(); }) {
            return T::min_required_size();
        } else {
            return 0;
        }
    }

    template <std::size_t N = 16, template <typename> class... Features>
    struct any final : public Features<any<N, Features...>>... {
        using A = any<N, Features...>;

        constexpr static std::size_t storage_size() noexcept {
            if constexpr (sizeof...(Features) > 0) {
                return std::max({N, required_size<Features<A>>()...});
            }
            return N;
        }

        struct alignas(64) any_properties : public Features<A>::extend_properties... {
            const bool _inplace_flag{false};  // Is SOO active for this type?
            const bool _is_move_constructible{false};
            const bool _is_copy_constructible{false};
            const std::type_info& _type_info;
            const std::type_index _type_index{std::type_index(typeid(void))};
            const std::string_view _src_type_name{};
            const std::size_t _value_size{0};
            void (*_destroy)(A&){nullptr};
            void (*_delete)(A&){nullptr};
            void (*_clone)(A&, const A&){nullptr};
            void (*_move)(A&, A&&){nullptr};
            void (*_assign_clone)(A&, const A&){nullptr};
            void (*_assign_move)(A&, void*){nullptr};
        };
        friend std::ostream& operator<<(std::ostream& os, const any_properties& prop) {
            // clang-format off
        os << "\nAny: " << ext::src_type_name<A>()
           << "\n   any::operations<>:" << static_cast<void*>(&prop)
           << "\n   type name: " << prop._src_type_name
           << "\n   typeinfo name: " << prop._type_info.name()
           << "\n   type_index hash: " << prop._type_index.hash_code() // std::type_index(prop._type_info)
           << "\n   value size: " << prop._value_size
           << "\n   inplace: " << prop._inplace_flag
           << "\n   move constructible: " << prop._is_move_constructible
           << "\n   copy constructible: " << prop._is_copy_constructible
           << "\n--\n";
            // clang-format on
            return os;
        }

        const any_properties* _properties{nullptr};
        union {
            char _storage[storage_size()];
            void* _pointer;
        };

        template <typename T>
        T* get_pointer() {
            return reinterpret_cast<T*>(_pointer);
        }
        template <typename T>
        const T* get_pointer() const {
            return reinterpret_cast<const T*>(_pointer);
        }
        template <typename T>
        void set_pointer(T* value) {
            _pointer = reinterpret_cast<void*>(value);
        }

        static_assert(sizeof(_storage) == storage_size(), "N is too small");
        static_assert(sizeof(_storage) >= sizeof(void*), "_storage size too small");

        template <typename T>
        T& storage_inplace() noexcept {
            return *static_cast<T*>(&_storage);
        }
        template <typename T>
        const T& storage_inplace() const noexcept {
            return *static_cast<const T*>(&_storage);
        }

        template <typename T>
        T& storage_dynamic() {
            return *reinterpret_cast<T*>(_pointer);
        }
        template <typename T>
        const T& storage_dynamic() const {
            return *reinterpret_cast<const T*>(_pointer);
        }

        template <typename T>
        constexpr static bool is_inplace() {
            return sizeof(T) <= storage_size() && alignof(T) <= alignof(A);
        }

        template <typename T>
        constexpr T& inplace_data() noexcept {
            static_assert(is_inplace<T>(), "Something is wrong");
            return *reinterpret_cast<T*>(&this->_storage);
        }

        template <typename T>
        constexpr const T& inplace_data() const noexcept {
            static_assert(is_inplace<T>(), "Never use storage_inplace for too large types");
            return *reinterpret_cast<const T*>(&this->_storage);
        }

        template <typename T>
        T& data() {
            if constexpr (is_inplace<T>()) {
                return inplace_data<T>();
            } else {
                return storage_dynamic<T>();
            }
        }

        template <typename T>
        const T& data() const {
            if constexpr (is_inplace<T>()) {
                return inplace_data<T>();
            } else {
                return storage_dynamic<T>();
            }
        }

      public:
        constexpr explicit any() {
            _properties = nullptr;
            clear_storage();
        }

        any(const any& rhs) {
            if (rhs.has_value()) {
                _properties = rhs._properties;
                _properties->_clone(*this, rhs);
            } else {
                _properties = nullptr;
                clear_storage();
            }
        }

        any(any&& rhs) noexcept {
            if (rhs.has_value()) {
                _properties = rhs._properties;
                _properties->_move(*this, std::move(rhs));
            } else {
                _properties = nullptr;
                clear_storage();
            }
        }

        template <typename U,
                  typename DU = std::enable_if_t<!std::is_same_v<A, std::remove_cvref_t<U>>,
                                                 std::remove_cvref_t<U>>>
        constexpr any(const U& value)
            requires(!is_an_any_v<U> && !is_an_any_v<std::remove_cvref_t<U>>)
        {
            _properties = &any_properties_t_data_type<DU, A>::instance;
            if constexpr (is_inplace<DU>()) {
                new (&inplace_data<DU>()) DU(value);
            } else {
                set_pointer<DU>(new DU(value));
            }
        }

        template <typename U,
                  typename DU = std::enable_if_t<!std::is_same_v<A, std::remove_cvref_t<U>>, U>>
        explicit constexpr any(U&& value) {
            _properties = &any_properties_t_data_type<DU, A>::instance;
            if constexpr (is_inplace<DU>()) {
                new (&inplace_data<DU>()) DU(std::forward<U>(value));
            } else {
                set_pointer<DU>(new DU(std::forward<U>(value)));
            }
        }

        any& operator=(const any& rhs) {
            if (this == &rhs) [[unlikely]]
                return *this;

            if (rhs.has_value()) [[likely]] {
                if (has_value() && _properties == rhs._properties) {
                    _properties->_assign_clone(*this, rhs);
                } else {
                    reset();
                    _properties = rhs._properties;
                    _properties->_clone(*this, rhs);
                }
            } else {
                reset();
            }
            return *this;
        }

        any& operator=(any&& rhs) noexcept {
            if (this == &rhs) [[unlikely]]
                return *this;

            if (!rhs.has_value()) [[unlikely]] {
                reset();
                return *this;
            }
            if (has_value()) {
                if (_properties == rhs._properties) {
                    _properties->_assign_move(*this, static_cast<void*>(&rhs));
                    return *this;
                }
                reset();
            }
            _properties = rhs._properties;
            _properties->_move(*this, std::move(rhs));
            return *this;
        }

        ~any() { reset(); }

        void clear_storage() {
            // not needed in release version.
            // memset(_storage, '\0', sizeof(_storage));
        }

        void reset() {
            // _delete in case of inplace - is not releasing memory, only calling destructor of the
            // element inside _storage.
            if (has_value()) {
                _properties->_delete(*this);
            }
            _properties = nullptr;
            clear_storage();
        }

        // FIXME: TODO: implement template for any<M> where M != N, with different features

        template <typename U,
                  typename DU = std::enable_if_t<!std::is_same_v<any<N>, std::remove_cvref_t<U>>,
                                                 std::remove_cvref_t<U>>>
        constexpr any& operator=(U& value)  // Check that it is / isn't an Any.
        {
            if (has_value() && _properties->_type_info == typeid(DU)) {
                _properties->_assign_clone(*this, value);
                return *this;
            }
            reset();
            _properties = &any_properties_t_data_type<DU, A>::instance;
            if constexpr (is_inplace<DU>()) {
                inplace_data<DU>() = value;
            } else {
                set_pointer<DU>(new DU(value));
            }
            return *this;
        }

        template <typename U,
                  typename DU = std::enable_if_t<!std::is_same_v<any<N>, std::decay_t<U>>, U>>
        constexpr any& operator=(U&& value) {
            if (has_value() && _properties->_type_info == typeid(DU)) {
                _properties->_assign_move(*this, static_cast<void*>(&value));
                return *this;
            }
            reset();
            _properties = &any_properties_t_data_type<DU, A>::instance;
            if constexpr (is_inplace<DU>()) {
                new (&_storage) DU(std::forward<U>(value));
            } else {
                set_pointer<DU>(new DU(std::forward<U>(value)));
            }
            return *this;
        }

        [[nodiscard]] constexpr bool inplace() const noexcept {  // return true if no stored value
            if (!has_value() || _properties->_inplace_flag) return true;
            return false;
        }
        constexpr static std::size_t in_place_capacity() noexcept { return storage_size(); }

        [[nodiscard]] constexpr bool has_value() const noexcept { return nullptr != _properties; }

        // Note: standard cast_any<T> returns T value, a copy of the content of A, while ext::any<>
        // returns a T& std::any_cast is returning T a copy of the stored item, the any_cast below
        // returns T& to the stored item.

        template <typename T>
        friend T& any_cast(any& a) {
            if (!a.has_value() || a._properties->_type_info != typeid(T)) throw std::bad_any_cast{};
            return a.data<T>();
        }

        template <typename T>
        friend const T& any_cast(const any& a) {
            if (!a.has_value() || a._properties->_type_info != typeid(T)) throw std::bad_any_cast{};
            return &a.data<T>();
        }

        template <typename T>
        friend T* any_cast(any* ap) {
            if (!ap->has_value() || ap->_properties->_type_info != typeid(T)) return nullptr;
            return &ap->data<T>();
        }

        template <typename T>
        friend const T* any_cast(const any* ap) {
            if (!ap->has_value() || ap->_properties->_type_info != typeid(T)) return nullptr;
            return &ap->data<T>();
        }

        [[nodiscard]] constexpr const std::type_info& type() const noexcept {
            if (has_value()) return _properties->_type_info;
            return typeid(void);
        }

        template <typename T, typename... Arg>
        T& emplace(Arg&&... args) {
            using DT = std::decay_t<T>;
            reset();
            _properties = &any_properties_t_data_type<DT, A>::instance;
            if constexpr (is_inplace<DT>()) {
                new (&_storage) DT{std::forward<Arg>(args)...};
                return inplace_data<DT>();
            } else {
                set_pointer<DT>(new DT{std::forward<Arg>(args)...});
                return storage_dynamic<DT>();
            }
        }

        [[nodiscard]] std::string_view src_type_name() const {
            if (has_value()) return _properties->_src_type_name;
            return "empty";
        }

        [[nodiscard]] std::size_t value_size() const {
            if (has_value()) return _properties->_value_size;
            return 0;
        }

        explicit any(const char* p) : any(std::string{p}) {}

        [[nodiscard]] constexpr const any_properties* properties() const noexcept {
            return _properties;
        }
    };

    static_assert(sizeof(any<8>) == 8 + 8, "wrong storage for any");
    static_assert(sizeof(any<16>) == 16 + 8, "wrong storage for any");
    static_assert(sizeof(any<24>) == 24 + 8, "wrong storage for any");
    static_assert(sizeof(any<32>) == 32 + 8, "wrong storage for any");

    template <typename T, std::size_t N, template <typename> class... Features>
    struct any_properties_t_data_type<T, any<N, Features...>> final {
        using A = any<N, Features...>;

      public:
        static inline const A::any_properties instance{[]() -> A::any_properties {
            typename A::any_properties pro{
                ._inplace_flag = A::template is_inplace<T>(),
                ._is_move_constructible = std::is_move_constructible_v<T>,
                ._is_copy_constructible = std::is_copy_constructible_v<T>,
                ._type_info = typeid(T),
                ._type_index = std::type_index(typeid(T)),
                ._src_type_name = src_type_name<T>(),
                ._value_size = sizeof(T),
                ._destroy = [](A& a) -> void {
                    T* p = &a.template data<T>();
                    p->T::~T();
                },
                ._delete = [](A& a) -> void {
                    if constexpr (A::template is_inplace<T>()) {
                        (&a.template inplace_data<T>())->T::~T();
                    } else {
                        T* p{a.template get_pointer<T>()};
                        delete p;
                        a.template set_pointer<void>(nullptr);
                    }
                },
                ._clone = [](A& a, const A& b) -> void {
                    if constexpr (!std::is_move_constructible_v<T> ||
                                  !std::is_copy_constructible_v<T>) {
                        throw std::runtime_error("trying to clone non-copyable type");
                    } else {
                        if constexpr (A::template is_inplace<T>()) {
                            new (&a.template inplace_data<T>()) T(b.template inplace_data<T>());
                        } else {
                            const T* cp{b.template get_pointer<T>()};
                            a.template set_pointer<T>(new T(*cp));
                        }
                    }
                },
                ._move = [](A& lhs, A&& rhs) -> void {
                    if constexpr (A::template is_inplace<T>()) {
                        new (&lhs.template inplace_data<T>())
                            T(std::move(rhs.template inplace_data<T>()));
                    } else {
                        lhs._properties = rhs._properties;
                        lhs.template set_pointer<T>(rhs.template get_pointer<T>());
                        rhs.template set_pointer<void>(nullptr);
                        rhs._properties = nullptr;
                    }
                },
                ._assign_clone = [](A& a, const A& b) -> void {
                    if constexpr (A::template is_inplace<T>()) {
                        a.template inplace_data<T>() = b.template inplace_data<T>();
                    } else {
                        auto& ap{*reinterpret_cast<T**>(&a._pointer)};
                        auto& bp{*reinterpret_cast<T* const*>(&b._pointer)};
                        *ap = *bp;
                    }
                },
                ._assign_move = [](A& a, void* bvp) -> void {
                    if constexpr (A::template is_inplace<T>()) {
                        a.template inplace_data<T>() = std::move(*reinterpret_cast<T*>(bvp));
                    } else {
                        auto ap{*reinterpret_cast<T**>(&a._pointer)};
                        auto bp{reinterpret_cast<T*>(bvp)};
                        *ap = std::move(*bp);
                    }
                }};
            (void)((Features<A>::template construct_extend_properties<T>(pro), 1), ...);
            return pro;
        }()};
    };

    // ====================================================== any_features.h
    // Features - optional features for any<N, Features...>
    // construct - on the objects themselves.
    // properties_construct - once per-type when creating the instance of the properties.
    template <typename T>
    struct af_streamed;

    template <std::size_t N,
              template <typename> class... Features>  // A is the specific ext::any type.
    struct af_streamed<any<N, Features...>> {
        using A = any<N, Features...>;
        struct extend_properties {
            void (*_ostream)(std::ostream&, const A&);
        };

        template <typename T>
        static void construct_extend_properties(auto& prop) {
            prop._ostream = [](std::ostream& os, const A& a) -> void {
                if constexpr (requires(T t) { os << t; }) {
                    if (a.has_value()) {
                        const T& value{a.template data<T>()};
                        os << value;
                    }
                }
            };
        }

        friend std::ostream& operator<<(std::ostream& os, const A& a) {
            if (a.has_value()) {
                a.properties()->_ostream(os, a);
            }
            return os;
        }
    };

    template <typename T>
    struct af_strict_streamed;

    template <std::size_t N, template <typename> class... Features>
    struct af_strict_streamed<any<N, Features...>> {
        using A = any<N, Features...>;
        struct extend_properties {
            void (*_strict_ostream)(std::ostream&, const A&);
        };

        template <typename T>
        static void construct_extend_properties(auto& prop)
            requires requires(T t) { std::cout << t; }
        {
            static_assert(
                requires(T t) { std::cout << t; },
                "af_strict_streamed requires type supporting 'operator<< T{}");
            prop._strict_ostream = [](std::ostream& os, const A& a) -> void {
                if (a.has_value()) {
                    const T& value{a.template data<T>()};
                    os << value;
                }
            };
        }

        friend std::ostream& operator<<(std::ostream& os, const A& a) {
            if (a.has_value()) {
                a.properties()->_strict_ostream(os, a);
            }
            return os;
        }
    };

    template <typename T>
    struct af_strict_less;

    template <std::size_t N, template <typename> class... Features>
    struct af_strict_less<any<N, Features...>> {
        using A = any<N, Features...>;

        struct extend_properties {
            bool (*_strict_less)(const A&, const A&){nullptr};
        };

        template <typename T>
        static void construct_extend_properties(auto& prop) {
            static_assert(
                requires(T ta, T tb) { ta < tb; },
                "af_strict_less requires type supporting 'a < b' compare");

            prop._strict_less = [](const A& a, const A& b) -> bool {
                const T& a_value{a.template data<T>()};
                const T& b_value{b.template data<T>()};
                return a_value < b_value;
            };
        }

        friend bool operator<(const A& lhs, const A& rhs) {
            if (lhs.has_value() && rhs.has_value()) {
                if (lhs.properties()->_type_info == rhs.properties()->_type_info) {
                    return lhs._properties->_strict_less(lhs, rhs);
                }
                throw std::runtime_error("any operator less '<': with different types");
            }
            throw std::runtime_error("no any value in operator less '<'");
        }
    };

    template <typename T>
    struct af_strict_eq;

    template <std::size_t N, template <typename> class... Features>
    struct af_strict_eq<any<N, Features...>> {
        using A = any<N, Features...>;

        struct extend_properties {
            bool (*_strict_eq)(const A&, const A&){nullptr};
        };

        template <typename T>
        static void construct_extend_properties(auto& prop) {
            static_assert(
                requires(T ta, T tb) { ta == tb; },
                "af_strict_eq requires type supporting 'a == b' compare");

            prop._strict_eq = [](const A& a, const A& b) -> bool {
                const T& a_value{a.template data<T>()};
                const T& b_value{b.template data<T>()};
                return a_value == b_value;
            };
        }

        friend bool operator==(const A& lhs, const A& rhs) {
            if (lhs.has_value() && rhs.has_value()) {
                if (lhs.properties() == rhs.properties()) {
                    return lhs._properties->_strict_eq(lhs, rhs);
                }
                throw std::runtime_error("any operator eq '==': with different types");
            }
            throw std::runtime_error("no any value in operator eq '=='");
        }
    };

    template <typename T>
    struct af_strict_inplace;

    template <std::size_t N, template <typename> class... Features>
    struct af_strict_inplace<any<N, Features...>> {
        using A = any<N, Features...>;

        struct extend_properties {};
        template <typename T>
        static void construct_extend_properties(auto&)
            requires(A::template is_inplace<T>())
        {}
    };

    template <typename T>
    struct af_strict_hash;

    template <std::size_t N, template <typename> class... Features>
    struct af_strict_hash<any<N, Features...>> {
        using A = any<N, Features...>;

        struct extend_properties {
            uint64_t (*_strict_hash)(const A&){nullptr};
        };

        template <typename T>
        static void construct_extend_properties(auto& prop) {
            static_assert(
                requires(T ta) { std::hash<T>{}(ta); },
                "af_strict_hash requires type supporting hash{}(a)");

            prop._strict_hash = [](const A& a) -> uint64_t {
                const T& value{a.template data<T>()};
                return std::hash<T>{}(value);
            };
        }

        [[nodiscard]] std::size_t get_hash() const {
            auto self = static_cast<const A*>(this);
            if (!self->has_value()) {
                throw std::runtime_error("hash on ext::any without value");
            }
            return self->properties()->_strict_hash(*self);
        }
    };

    template <typename... Ts>
    struct af_variant {
        template <typename T>
        struct types;

        template <std::size_t N, template <typename> class... Features>
        struct types<any<N, Features...>> {
            using A = any<N, Features...>;

            constexpr static std::size_t min_required_size() {
                return std::max({std::size_t{0}, sizeof(Ts)...});
            }

            struct extend_properties {};

            template <typename T>
            static void construct_extend_properties(auto&) {
                static_assert(std::disjunction_v<std::is_same<T, Ts>...>,
                              "af_variant requires specific types");
            }
        };
    };

    template <typename T>
    struct af_func;

    template <std::size_t N, template <typename> class... Features>
    struct af_func<any<N, Features...>> {
        using A = any<N, Features...>;

        struct extend_properties {};

        template <typename F, typename R, typename... Args>
        struct XYZ {
            R operator()(Args&&...) {
                // TODO: check if the if //            if (has_value()&&)
            }
        };
    };

}  // namespace ext

namespace std {
    template <std::size_t N, template <typename> class... Features>
    struct hash<ext::any<N, Features...>> {
        std::size_t operator()(const ext::any<N, Features...>& x) const { return x.get_hash(); }
    };

}  // namespace std
