#ifndef SPONGE_LIBSPONGE_TIMER_HH
#define SPONGE_LIBSPONGE_TIMER_HH

#include <cstddef>

class Timer {
    bool _is_open{};
    size_t _time_cost{};
    size_t _time_out{};

  public:
    Timer() = default;

    Timer(size_t time_out) : _time_out(time_out) {}

    bool time_out() const { return _is_open && _time_cost >= _time_out; }

    void set_time_out(size_t time_out) { _time_out = time_out; }

    size_t get_time_out() const { return _time_out; }

    void close() { _is_open = false; }

    void reset() {
        _is_open = true;
        _time_cost = 0;
    }

    void tick(size_t ms_since_last_tick) {
        if (_is_open) {
            _time_cost += ms_since_last_tick;
        }
    }

    bool state() const { return _is_open; }
};

#endif  // SPONGE_LIBSPONGE_TIMER_HH