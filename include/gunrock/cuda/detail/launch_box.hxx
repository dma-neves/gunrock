/**
 * @file launch_box.hxx
 * @author Cameron Shinn (ctshinn@ucdavis.edu)
 * @brief
 * @version 0.1
 * @date 2021-12-16
 *
 * @copyright Copyright (c) 2021
 *
 */
#pragma once

#include <gunrock/cuda/cuda.hxx>

namespace gunrock {
namespace cuda {

namespace launch_box {
namespace detail {

/**
 * @brief Abstract base class for launch parameters.
 *
 * @tparam sm_flags_ Bitwise flags indicating SM versions (`sm_flag_t` enum).
 * @tparam items_per_thread_ (default = `1`) Number of items per thread.
 * @tparam shared_memory_bytes_ Number of bytes of shared memory to allocate.
 */
template <sm_flag_t sm_flags_,
          std::size_t items_per_thread_,
          std::size_t shared_memory_bytes_>
struct launch_params_base_t {
  static constexpr sm_flag_t sm_flags = sm_flags_;
  static constexpr std::size_t shared_memory_bytes = shared_memory_bytes_;
  static constexpr std::size_t items_per_thread = items_per_thread_;
};

/**
 * @brief False value dependent on template param so compiler can't optimize.
 *
 * @tparam T Arbitrary type.
 */
template <typename T>
struct always_false {
  enum { value = false };
};

/**
 * @brief Raises static assert when template is instantiated.
 *
 * @tparam T Arbitrary type.
 */
template <typename T>
struct raise_not_found_error_t {
  static_assert(always_false<T>::value,
                "Launch box could not find valid launch parameters");
};

// Macro for the flag of the current device's SM version
#define SM_TARGET_FLAG _SM_FLAG_WRAPPER(SM_TARGET)
// "ver" will be expanded before the call to _SM_FLAG
#define _SM_FLAG_WRAPPER(ver) _SM_FLAG(ver)
#define _SM_FLAG(ver) sm_##ver

/**
 * @brief Subsets a pack of launch parameters (children of
 * `launch_params_base_t`), selecting the ones that match the architecture being
 * compiled for, stored in a tuple type.
 *
 * @par Overview
 * This template alias is a tuple type of all the launch parameter types that
 * have `sm_flags` matching the current SM architecture being compiled for. It
 * uses the `tuple_cat()` funtion to concatenate tuples that are empty or
 * contain the launch parameter type if the SM version matches. The `lp_v` pack
 * is placed inside a `conditional_t`, which checks for a match and is then
 * expanded into the arguments of `tuple_cat()` using the `...` operator. This
 * was inspired by this Stack Overflow solution:
 * https://stackoverflow.com/a/67155114/13232647.
 *
 * @tparam lp_v Pack of `launch_params_t` types for each corresponding
 * architecture(s).
 */
template <typename... lp_v>
using match_launch_params_t = decltype(std::tuple_cat(
    std::declval<std::conditional_t<(bool)(lp_v::sm_flags& SM_TARGET_FLAG),
                                    std::tuple<lp_v>,
                                    std::tuple<>>>()...));

template <unsigned int threads_per_block,
          unsigned int items_per_thread,
          typename func_t,
          typename... args_t>
__global__ __launch_bounds__(threads_per_block,
                             items_per_thread)  // strict launch bounds
    void blocked_strided_kernel(func_t f,
                                const std::size_t bound,
                                args_t... args) {
  const int stride = cuda::block::size::x() * cuda::grid::size::x();
  for (int i = cuda::thread::global::id::x();  // global id
       i < bound;                              // bound check
       i += (stride * items_per_thread)        // offset
  ) {
#pragma unroll
    for (int j = 0; j < items_per_thread; ++j) {
      // Simple blocking per thread (unrolled items_per_thread_t)
      f(i + (stride * j), cuda::block::id::x(), args...);
    }
  }
}

template <unsigned int threads_per_block, typename func_t, typename... args_t>
__global__ __launch_bounds__(threads_per_block, 1)  // strict launch bounds
    void strided_kernel(func_t f, const std::size_t bound, args_t... args) {
  const int stride = cuda::block::size::x() * cuda::grid::size::x();
  for (int i = cuda::thread::global::id::x();  // global id
       i < bound;                              // bound check
       i += stride                             // offset
  ) {
    f(i, cuda::block::id::x(), args...);
  }
}

}  // namespace detail
}  // namespace launch_box

}  // namespace cuda
}  // namespace gunrock
