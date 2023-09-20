// Copyright © 2021 Felix Schütz.
// Licensed under the MIT license. See the LICENSE file for details.

#pragma once

#include <cassert>    // assert
#include <cmath>      // std::log2
#include <coroutine>  // std::coroutine_handle, std::suspend_never
#include <cstddef>    // std::size_t
#include <functional> // std::function, std::hash
#include <memory>     // std::shared_ptr, std::make_shared
#include <utility>    // std::exchange, std::move
#include <vector>     // std::vector

namespace simcpp20 {
template <typename Time> class simulation;

/**
 * One event.
 *
 * @tparam Time Type used for simulation time.
 */
template <typename Time = double> class event {
public:
  /**
   * Constructor.
   *
   * @param simulation Reference to the simulation.
   */
  explicit event(simulation<Time> &sim) : data_{std::make_shared<data>(sim)} {
    assert(data_);
  }

  /// Destructor.
  virtual ~event() {}

  /**
   * Copy constructor.
   *
   * @param other Event to copy.
   */
  event(const event &other) : data_{other.data_} { assert(data_); }

  /**
   * Move constructor.
   *
   * @param other Event to move.
   */
  event(event &&other) noexcept : data_{std::move(other.data_)} {
    assert(data_);
  }

  /**
   * Copy assignment operator.
   *
   * @param other Event to replace this event with.
   * @return Reference to this instance.
   */
  event &operator=(const event &other) {
    data_ = other.data_;
    assert(data_);
    return *this;
  }

  /**
   * Move assignment operator.
   *
   * @param other Event to replace this event with.
   * @return Reference to this instance.
   */
  event &operator=(event &&other) noexcept {
    data_ = std::move(other.data_);
    assert(data_);
    return *this;
  }

  /**
   * Set the event state to triggered and schedule it to be processed
   * immediately. If the event is not pending, nothing is done.
   *
   * TODO(fschuetz04): Check whether used on a process?
   */
  void trigger() const {
    assert(data_);

    if (!pending()) {
      return;
    }

    data_->sim_.schedule(*this);
    data_->state_ = state::triggered;
  }

  /**
   * Set the event state to aborted. If the event is not pending, nothing is
   * done.
   *
   * TODO(fschuetz04): Check whether used on a process? Destroy process
   * coroutine?
   */
  void abort() const {
    assert(data_);

    if (!pending()) {
      return;
    }

    data_->state_ = state::aborted;

    for (auto &handle : data_->handles_) {
      handle.destroy();
    }
    data_->handles_.clear();
    data_->cbs_.clear();
  }

  /// @param cb Callback to be called when the event is processed.
  void add_callback(std::function<void(const event<Time> &)> cb) const {
    assert(data_);

    if (processed() || aborted()) {
      return;
    }

    data_->cbs_.emplace_back(cb);
  }

  /// @return Whether the event is pending.
  bool pending() const {
    assert(data_);
    return data_->state_ == state::pending;
  }

  /// @return Whether the event is triggered or processed.
  bool triggered() const {
    assert(data_);
    return data_->state_ == state::triggered || processed();
  }

  /// @return Whether the event is processed.
  bool processed() const {
    assert(data_);
    return data_->state_ == state::processed;
  }

  /// @return Whether the event is aborted.
  bool aborted() const {
    assert(data_);
    return data_->state_ == state::aborted;
  }

  /**
   * Called when using co_await on the event. The calling coroutine is only
   * suspended if this method returns false.
   *
   * @return Whether the event is processed.
   */
  bool await_ready() const {
    assert(data_);
    return processed();
  }

  /**
   * Called when a coroutine is suspended after using co_await on the event.
   *
   * @tparam Promise Promise type of the coroutine.
   * @param handle Coroutine handle.
   */
  template <typename Promise>
  void await_suspend(std::coroutine_handle<Promise> handle) {
    assert(data_);

    if (aborted()) {
      handle.destroy();
      return;
    }

    data_->handles_.push_back(handle);
  }

  /**
   * Called when a coroutine is resumed after using co_await on the event or if
   * the coroutine did not need to be suspended.
   */
  void await_resume() { assert(data_); }

  /**
   * Alias for simulation::any_of.
   *
   * @param other Other event.
   * @return New pending event which is triggered when this event or the other
   * event is processed.
   */
  event<Time> operator|(const event<Time> &other) const {
    assert(data_);
    return data_->sim_.any_of({*this, other});
  }

  /**
   * Alias for simulation::all_of.
   *
   * @param other Other event.
   * @return New pending event which is triggered when this event and the other
   * event are processed.
   */
  event<Time> operator&(const event<Time> &other) const {
    assert(data_);
    return data_->sim_.all_of({*this, other});
  }

  /** Comparison operator.
   *
   * @param other Other event.
   * @return Whether this event is equal to the other event.
   */
  bool operator==(const event<Time> &other) const {
    return data_ == other.data_;
  }

  /// Promise type for a coroutine returning an event.
  class promise_type {
  public:
    /**
     * Constructor.
     *
     * @tparam Args Types of additional arguments passed to the coroutine
     * function.
     * @param sim Reference to the simulation.
     */
    template <typename... Args>
    explicit promise_type(simulation<Time> &sim, Args &&...)
        : sim_{sim}, ev_{sim} {}

    /**
     * Constructor.
     *
     * @tparam Class Class type if the coroutine function is a lambda or a
     * member function of a class.
     * @tparam Args Types of additional arguments passed to the coroutine
     * function.
     * @param sim Reference to the simulation.
     */
    template <typename Class, typename... Args>
    explicit promise_type(Class &&, simulation<Time> &sim, Args &&...)
        : sim_{sim}, ev_{sim} {}

    /**
     * Constructor.
     *
     * @tparam Class Class type if the coroutine function is a member function
     * of a class. Must contain a member variable sim referencing the simulation
     * instance.
     * @tparam Args Types of additional arguments passed to the coroutine
     * function.
     * @param c Class instance.
     */
    template <typename Class, typename... Args>
    explicit promise_type(Class &&c, Args &&...) : sim_{c.sim}, ev_{c.sim} {}

#ifdef __INTELLISENSE__
    // IntelliSense fix. See https://stackoverflow.com/q/67209981.
    promise_type();
#endif

    /**
     * Called to get the return value of the coroutine function.
     *
     * @return Event associated with the coroutine. This event is triggered
     * when the coroutine returns.
     */
    event<Time> &get_return_object() { return ev_; }

    /**
     * Called when the coroutine is started. The coroutine awaits the return
     * value before running.
     *
     * @return Event which will be processed at the current simulation time.
     */
    event<Time> initial_suspend() {
      sim_.schedule(ev_, 0);
      return ev_;
    }

    /// Called when an exception is thrown inside the coroutine and not handled.
    void unhandled_exception() const { assert(false); }

    /**
     * Called when the coroutine returns. Trigger the event associated with the
     * coroutine.
     */
    void return_void() const { ev_.trigger(); }

    /**
     * Called after the coroutine returns.
     *
     * @return Awaitable which is always ready.
     */
    std::suspend_always final_suspend() const noexcept { return {}; }

    /// Reference to the simulation.
    simulation<Time> &sim_;

    /**
     * Event associated with the coroutine. This event is triggered when the
     * coroutine returns.
     */
    event<Time> ev_;
  };

protected:
  /**
   * Set the event state to processed, resume all coroutines awaiting this
   * event, and call all callbacks added to the event.
   */
  void process() {
    assert(data_);

    if (processed() || aborted()) {
      return;
    }

    data_->state_ = state::processed;

    for (auto &handle : data_->handles_) {
      handle.resume();
    }
    data_->handles_.clear();

    for (auto &cb : data_->cbs_) {
      cb(*this);
    }
    data_->cbs_.clear();
  }

  /// State of the event.
  enum class state {
    /// Event is not yet triggered or aborted.
    pending,

    /// Event is triggered and will be processed at the current simulation
    /// time.
    triggered,

    /// Event is processed.
    processed,

    /// Event is aborted.
    aborted
  };

  /// Shared data of the event.
  class data {
  public:
    /**
     * Constructor.
     *
     * @param sim Reference to the simulation.
     */
    explicit data(simulation<Time> &sim) : sim_{sim} {}

    /// Destructor.
    virtual ~data() {
      for (auto &handle : handles_) {
        handle.destroy();
      }
    }

    /// State of the event.
    state state_ = state::pending;

    /// Handles of coroutines awaiting the event.
    std::vector<std::coroutine_handle<>> handles_ = {};

    /// Callbacks added to the event.
    std::vector<std::function<void(const event<Time> &)>> cbs_ = {};

    /// Reference to the simulation.
    simulation<Time> &sim_;
  };

  /**
   * Constructor.
   *
   * @param data Shared data.
   */
  explicit event(const std::shared_ptr<data> &data_) : data_{data_} {
    assert(data_);
  }

  /// Shared data of the event.
  std::shared_ptr<data> data_;

  friend class simulation<Time>;
  friend struct std::hash<event<Time>>;
};
} // namespace simcpp20

namespace std {
/// Specialization of std::hash for simcpp20::event.
template <typename Time> struct hash<simcpp20::event<Time>> {
  /**
   * @param ev Event.
   * @return Hash of the event.
   */
  std::size_t operator()(const simcpp20::event<Time> &ev) const {
    static const std::size_t shift =
        std::log2(1 + sizeof(simcpp20::event<Time>));
    return reinterpret_cast<std::size_t>(ev.data_) >> shift;
  }
};
} // namespace std
