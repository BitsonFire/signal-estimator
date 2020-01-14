/* Copyright (c) 2020 Victor Gaydov
 *
 * This code is licensed under the MIT License.
 */

#include "LatencyEstimator.hpp"
#include "Log.hpp"
#include "Time.hpp"

namespace signal_estimator {

LatencyEstimator::LatencyEstimator(const Config& config)
    : config_(config)
    , output_trigger_(config)
    , input_trigger_(config)
    , sma_(config.sma_window) {
}

void LatencyEstimator::add_output(nanoseconds_t ts, const int16_t* buf, size_t bufsz) {
    output_trigger_.add_signal(ts, Dir::Playback, buf, bufsz);

    Report report;

    if (process_output_(report)) {
        print_report_(report);
    }
}

void LatencyEstimator::add_input(nanoseconds_t ts, const int16_t* buf, size_t bufsz) {
    input_trigger_.add_signal(ts, Dir::Recording, buf, bufsz);

    Report report;

    if (process_input_(report)) {
        print_report_(report);
    }
}

bool LatencyEstimator::process_output_(Report& report) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (output_trigger_.is_triggered()) {
        if (!output_triggered_) {
            output_triggered_ = true;
            output_ts_ = output_trigger_.trigger_ts_msec();
            return check_triggers_(report);
        }
    } else {
        output_triggered_ = false;
        latency_reported_ = false;
    }

    return false;
}

bool LatencyEstimator::process_input_(Report& report) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (input_trigger_.is_triggered()) {
        if (!input_triggered_) {
            input_triggered_ = true;
            input_ts_ = input_trigger_.trigger_ts_msec();
            return check_triggers_(report);
        }
    } else {
        input_triggered_ = false;
    }

    return false;
}

bool LatencyEstimator::check_triggers_(Report& report) {
    if (!output_triggered_ || !input_triggered_) {
        return false;
    }

    if (!(output_ts_ < input_ts_)) {
        return false;
    }

    if (latency_reported_) {
        return false;
    }

    report.latency_msec = (input_ts_ - output_ts_);
    report.avg_latency_msec = sma_.add(report.latency_msec);

    latency_reported_ = true;
    return true;
}

void LatencyEstimator::print_report_(const Report& report) {
    log_info("latency:  cur %7.3fms  avg%d %7.3fms", report.latency_msec,
        (int)config_.sma_window, report.avg_latency_msec);
}

} // namespace signal_estimator