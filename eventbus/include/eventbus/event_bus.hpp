#pragma once

#include <any>
#include <atomic>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <system_error>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <utility>

#include "detail/function_traits.hpp"

namespace dp {
    class event_bus;

    /**
     * @brief A registration handle for a particular handler of an event type.
     * @details This class is move constructible only. It also assumed that the lifespan of this
     * object will be as long or shorter than that of the event bus. This class is move
     * constructible for that reason, but there are still some cases where you can run into life
     * time issues.
     */
    class handler_registration {
        const void* handle_{nullptr};
        dp::event_bus* event_bus_{nullptr};

      public:
        handler_registration(const handler_registration& other) = delete;
        handler_registration(handler_registration&& other) noexcept;
        handler_registration& operator=(const handler_registration& other) = delete;
        handler_registration& operator=(handler_registration&& other) noexcept;
        ~handler_registration() noexcept;

        /**
         * @brief Pointer to the underlying handle.
         */
        [[nodiscard]] const void* handle() const noexcept;

        /**
         * @brief Unregister this handler from the event bus.
         */
        void unregister() noexcept;

      protected:
        handler_registration(const void* handle, dp::event_bus* bus) noexcept;
        friend class event_bus;
    };

    /**
     * @brief A central event handler class that connects event handlers with the events.
     */
    class event_bus {
      public:
        /**
         * @brief Register an event handler for a given event type.
         * @tparam EventHandler The invocable event handler type.
         * @param handler A callable handler of the event type. This invocation is designed for when
         * `handler` takes the EventType as an argument.
         * @return A handler_registration instance for the given handler.
         */
        template <typename EventHandler>
        [[nodiscard]] auto register_handler(EventHandler&& handler) noexcept {
            using EventType = typename detail::function_traits<EventHandler>::template arg<0>::type;

            static_assert(std::is_invocable_v<EventHandler, EventType>,
                          "EventHandler must be invocable with EventType as an argument.");

            return register_handler_impl<EventType>(std::forward<EventHandler>(handler));
        }

        /**
         * @brief Register an event handler for a given event type.
         * @tparam EventType The event type
         * @tparam EventHandler The invocable event handler type.
         * @param handler A callable handler of the event type. This invocation is used for when
         * `handler` takes no parameters but wants to be fired when EventType is fired.
         * @return A handler_registration instance for the given handler.
         */
        template <typename EventType, typename EventHandler>
        [[nodiscard]] handler_registration register_handler(EventHandler&& handler) noexcept {
            static_assert(std::is_invocable_v<EventHandler>,
                          "EventHandler must be invocable with no arguments.");

            return register_handler_impl<EventType>(std::forward<EventHandler>(handler));
        }

        /**
         * @brief Register an event handler for a given event type.
         * @tparam EventType The event type
         * @tparam ClassType Event handler class
         * @tparam MemberFunction Event handler member function
         * @param class_instance Instance of ClassType that will handle the event.
         * @param function Pointer to the MemberFunction of the ClassType. This invocation is for
         * when `function` takes the EventType as an argument.
         * @return A handler_registration instance for the given handler.
         */
        template <typename ClassType, typename MemberFunction>
        [[nodiscard]] handler_registration register_handler(ClassType* class_instance,
                                                            MemberFunction&& function) noexcept {
            using EventType =
                typename detail::function_traits<MemberFunction>::template arg<0>::type;

            static_assert(
                std::is_invocable_v<MemberFunction, ClassType*, EventType>,
                "EventHandler must be a member function of ClassType and one EventType argument.");

            return register_handler_impl<EventType>(
                [class_instance, func = std::forward<MemberFunction>(function)](
                    const EventType& event) { (class_instance->*func)(event); });
        }

        /**
         * @brief Register an event handler for a given event type.
         * @tparam EventType The event type
         * @tparam ClassType Event handler class
         * @tparam MemberFunction Event handler member function
         * @param class_instance Instance of ClassType that will handle the event.
         * @param function Pointer to the MemberFunction of the ClassType. This invocation is for
         * when `function` takes no arguments but wants to be fired when EventType is fired.
         * @return A handler_registration instance for the given handler.
         */
        template <typename EventType, typename ClassType, typename MemberFunction>
        [[nodiscard]] handler_registration register_handler(ClassType* class_instance,
                                                            MemberFunction&& function) noexcept {
            static_assert(
                std::is_invocable_v<MemberFunction, ClassType*>,
                "EventHandler must be a member function of ClassType and take no arguments.");

            return register_handler_impl<EventType>(
                [class_instance, func = std::forward<MemberFunction>(function)]() {
                    (class_instance->*func)();
                });
        }

        /**
         * @brief Fire an event to notify event handlers.
         * @tparam EventType The event type
         * @param evt The event to pass to all event handlers.
         */
        template <typename EventType, typename = std::enable_if_t<!std::is_pointer_v<EventType>>>
        void fire_event(const EventType& evt) noexcept {
            safe_shared_registrations_access([this, &evt]() {
                // only call the functions we need to
                for (auto [begin_evt_id, end_evt_id] =
                         handler_registrations_.equal_range(std::type_index(typeid(EventType)));
                     begin_evt_id != end_evt_id; ++begin_evt_id) {
                    // call all handlers by passing a std::reference_wrapper<const EventType>
                    begin_evt_id->second(std::cref(evt));
                }
            });
        }

        /**
         * @brief Remove a given handler from the event bus.
         * @param registration The registration object returned by register_handler.
         * @return true is handler removal was successful, false otherwise.
         */
        bool remove_handler(const handler_registration& registration) noexcept {
            if (!registration.handle()) {
                return false;
            }

            auto result = false;
            safe_unique_registrations_access([this, &result, &registration]() {
                for (auto it = handler_registrations_.begin(); it != handler_registrations_.end();
                     ++it) {
                    if (static_cast<const void*>(&(it->second)) == registration.handle()) {
                        handler_registrations_.erase(it);
                        result = true;
                        break;
                    }
                }
            });
            return result;
        }

        /**
         * @brief Remove all handlers from event bus.
         */
        void remove_handlers() noexcept {
            safe_unique_registrations_access([this]() { handler_registrations_.clear(); });
        }

        /**
         * @brief Get the number of handlers registered with the event bus.
         * @return The total number of handlers.
         */
        [[nodiscard]] std::size_t handler_count() noexcept {
            std::size_t count{};
            safe_shared_registrations_access(
                [this, &count]() { count = handler_registrations_.size(); });
            return count;
        }

      private:
        using mutex_type = std::shared_mutex;
        mutable mutex_type registration_mutex_;
        std::unordered_multimap<std::type_index, std::function<void(std::any)>>
            handler_registrations_;

        template <typename Callable>
        void safe_shared_registrations_access(Callable&& callable) noexcept {
            try {
                std::scoped_lock lock{registration_mutex_};
                callable();
            } catch (std::system_error&) {
            }
        }

        template <typename Callable>
        void safe_unique_registrations_access(Callable&& callable) noexcept {
            try {
                // if this fails, an exception may be thrown.
                std::scoped_lock lock{registration_mutex_};
                callable();
            } catch (std::system_error&) {
                // do nothing
            }
        }

        // Helper function which drastically cleans up the template parameterization requirements of
        // the users of this library. EventType is now deduced from the handler function directly
        // unless it takes no arguments.
        template <typename EventType, typename EventHandler>
        [[nodiscard]] handler_registration register_handler_impl(EventHandler&& handler) noexcept {
            using traits = detail::function_traits<EventHandler>;
            using RawParameterType = std::remove_cv_t<std::remove_reference_t<EventType>>;

            const auto type_idx = std::type_index(typeid(RawParameterType));
            const void* handle;

            // check if the function takes any arguments.
            if constexpr (traits::arity == 0) {
                safe_unique_registrations_access([&]() {
                    auto it = handler_registrations_.emplace(
                        type_idx,
                        [func = std::forward<EventHandler>(handler)](std::any&&) { func(); });

                    handle = static_cast<const void*>(&(it->second));
                });
            } else {
                safe_unique_registrations_access([&]() {
                    auto it = handler_registrations_.emplace(
                        type_idx, [func = std::forward<EventHandler>(handler)](std::any&& value) {
                            std::reference_wrapper<const RawParameterType> local_event =
                                std::any_cast<std::reference_wrapper<const RawParameterType>>(
                                    std::move(value));

                            if constexpr (std::is_rvalue_reference_v<EventType>) {
                                static_assert(std::is_copy_constructible_v<RawParameterType>,
                                              "Event type must be copy constructible.");
                                func(RawParameterType(local_event.get()));
                            } else {
                                func(local_event);
                            }
                        });

                    handle = static_cast<const void*>(&(it->second));
                });
            }
            return {handle, this};
        }
    };

    inline const void* handler_registration::handle() const noexcept { return handle_; }

    inline void handler_registration::unregister() noexcept {
        if (event_bus_ && handle_) {
            event_bus_->remove_handler(*this);
            handle_ = nullptr;
        }
    }

    inline handler_registration::handler_registration(const void* handle,
                                                      dp::event_bus* bus) noexcept
        : handle_(handle), event_bus_(bus) {}

    inline handler_registration::handler_registration(handler_registration&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)),
          event_bus_(std::exchange(other.event_bus_, nullptr)) {}

    inline handler_registration& handler_registration::operator=(
        handler_registration&& other) noexcept {
        handle_ = std::exchange(other.handle_, nullptr);
        event_bus_ = std::exchange(other.event_bus_, nullptr);
        return *this;
    }

    inline handler_registration::~handler_registration() noexcept { unregister(); }
}  // namespace dp
