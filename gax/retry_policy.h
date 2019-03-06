// Copyright 2019 Google Inc.  All rights reserved
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_GAX_RETRY_POLICY_H_
#define GOOGLE_GAX_RETRY_POLICY_H_

#include <chrono>
#include <memory>

namespace google {
namespace gax {

/**
 * Define the interface for controlling how clients retry RPC operations.
 *
 * Idempotent (and certain non-idempotent) operations can be retried
 * transparently to the user. However, we need to give the users enough flexiblity
 * to control when to stop retrying.
 *
 * The application provides an instance of this class when the client is created.
 */
template <typename StatusType, typename RetryablePolicy>
class RetryPolicy {
 public:
  virtual ~RetryPolicy() = default;

  /**
   * Return a new copy of this object with the same retry criteria and fresh state.
   */
  virtual std::unique_ptr<RetryPolicy<StatusType, RetryablePolicy>> clone() const = 0;

  /**
   * Handle an RPC failure
   *
   * @return true if the RPC operation should be retried.
   */
  virtual bool OnFailure(StatusType const& status) = 0;
};

/**
 * Implement a simple "count errors and then stop" retry policy.
 */
template <typename StatusType, typename RetryablePolicy>
class LimitedErrorCountRetryPolicy : RetryPolicy<StatusType, RetryablePolicy> {
  using BaseType = RetryPolicy<StatusType, RetryablePolicy>;

 public:
  LimitedErrorCountRetryPolicy(int max_failures)
      : failure_count_(0), max_failures_(max_failures) {}

  LimitedErrorCountRetryPolicy(LimitedErrorCountRetryPolicy const& rhs) noexcept
      : LimitedErrorCountRetryPolicy(rhs.max_failures_) {}

  LimitedErrorCountRetryPolicy(LimitedErrorCountRetryPolicy&& rhs) noexcept
      : LimitedErrorCountRetryPolicy(rhs.max_failures_) {}

  std::unique_ptr<RetryPolicy<StatusType, RetryablePolicy>> clone() const override {
    return std::unique_ptr<BaseType>(new LimitedErrorCountRetryPolicy<StatusType, RetryablePolicy>(*this));
  }

  bool OnFailure(StatusType const& status) override {
    return (!RetryablePolicy::IsPermanentFailure(status) &&
            (failure_count_++) < max_failures_);
  }

 private:
  int failure_count_;
  int const max_failures_;
};

/**
 * Implement a simple "keep trying for this time" retry policy.
 */
template <typename StatusType, typename RetryablePolicy, typename Clock>
class LimitedDurationRetryPolicy : RetryPolicy<StatusType, RetryablePolicy> {
  using BaseType = RetryPolicy<StatusType, RetryablePolicy>;

 public:
  template <typename duration_t>
  LimitedDurationRetryPolicy(duration_t max_duration) : max_duration_(max_duration),
                                                        deadline_(max_duration_ + Clock::now()) {}

  LimitedDurationRetryPolicy(LimitedDurationRetryPolicy const& rhs) noexcept
      : LimitedDurationRetryPolicy(rhs.max_duration_) {}

  LimitedDurationRetryPolicy(LimitedDurationRetryPolicy&& rhs) noexcept
      : LimitedDurationRetryPolicy(rhs.max_duration_) {}

  std::unique_ptr<RetryPolicy<StatusType, RetryablePolicy>> clone() const override {
    return std::unique_ptr<BaseType>(new LimitedDurationRetryPolicy<StatusType, RetryablePolicy, Clock>(*this));
  }

  bool OnFailure(StatusType const & status) override {
    return (!RetryablePolicy::IsPermanentFailure(status) &&
            Clock::now() < deadline_);
  }

 private:
  std::chrono::milliseconds const max_duration_;
  std::chrono::system_clock::time_point const deadline_;
};

}  // namespace gax
}  // namespace google

#endif  // GOOGLE_GAX_RETRY_POLICY_H_
