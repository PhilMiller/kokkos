/*
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 3.0
//       Copyright (2020) National Technology & Engineering
//               Solutions of Sandia, LLC (NTESS).
//
// Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY NTESS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NTESS OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Christian R. Trott (crtrott@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#include <TestStdAlgorithmsCommon.hpp>
#include <std_algorithms/Kokkos_BeginEnd.hpp>
#include <std_algorithms/Kokkos_PartitioningOperations.hpp>
#include <utility>
#include <algorithm>

namespace Test {
namespace stdalgos {
namespace PartitionCopy {

namespace KE = Kokkos::Experimental;

template <class ValueType>
struct UnifDist;

template <>
struct UnifDist<int> {
  using dist_type = std::uniform_int_distribution<int>;
  std::mt19937 m_gen;
  dist_type m_dist;

  UnifDist() : m_dist(-100, 100) { m_gen.seed(1034343); }

  int operator()() { return m_dist(m_gen); }
};

template <class ViewType>
void fill_view(ViewType dest_view, const std::string& name) {
  using value_type      = typename ViewType::value_type;
  using exe_space       = typename ViewType::execution_space;
  const std::size_t ext = dest_view.extent(0);
  using aux_view_t      = Kokkos::View<value_type*, exe_space>;
  aux_view_t aux_view("aux_view", ext);
  auto v_h = create_mirror_view(Kokkos::HostSpace(), aux_view);

  if (name == "empty") {
    // no op
  }

  else if (name == "one-element-a") {
    v_h(0) = static_cast<value_type>(1);
  }

  else if (name == "one-element-b") {
    v_h(0) = static_cast<value_type>(2);
  }

  else if (name == "two-elements-a") {
    v_h(0) = static_cast<value_type>(1);
    v_h(1) = static_cast<value_type>(2);
  }

  else if (name == "two-elements-b") {
    v_h(0) = static_cast<value_type>(2);
    v_h(1) = static_cast<value_type>(-1);
  }

  else if (name == "small-a") {
    for (std::size_t i = 0; i < ext; ++i) {
      v_h(i) = value_type{-5} + static_cast<value_type>(i + 1);
    }
  }

  else if (name == "small-b") {
    for (std::size_t i = 0; i < ext; ++i) {
      v_h(i) = static_cast<value_type>(22);
    }
  }

  else if (name == "small-c") {
    for (std::size_t i = 0; i < ext; ++i) {
      v_h(i) = static_cast<value_type>(-13);
    }
  }

  else if (name == "medium" || name == "large") {
    UnifDist<value_type> randObj;
    for (std::size_t i = 0; i < ext; ++i) {
      v_h(i) = randObj();
    }
  }

  else {
    throw std::runtime_error("invalid choice");
  }

  Kokkos::deep_copy(aux_view, v_h);
  CopyFunctor<aux_view_t, ViewType> F1(aux_view, dest_view);
  Kokkos::parallel_for("copy", dest_view.extent(0), F1);
}

template <class ViewTypeFrom, class ResultType, class ViewTypeDestTrue,
          class ViewTypeDestFalse, class PredType>
void verify_data(const std::string& name, ResultType my_result,
                 ViewTypeFrom view_from, ViewTypeDestTrue view_dest_true,
                 ViewTypeDestFalse view_dest_false, PredType pred) {
  using value_type = typename ViewTypeFrom::value_type;
  static_assert(
      std::is_same<value_type, typename ViewTypeDestTrue::value_type>::value,
      "");
  static_assert(
      std::is_same<value_type, typename ViewTypeDestFalse::value_type>::value,
      "");

  const std::size_t ext = view_from.extent(0);

  // create host clone of view_from and run std::partition_copy on it
  auto view_from_h = create_host_space_copy(view_from);
  std::vector<value_type> std_vec_true(ext, 0);
  std::vector<value_type> std_vec_false(ext, 0);
  auto std_result =
      std::partition_copy(KE::cbegin(view_from_h), KE::cend(view_from_h),
                          std_vec_true.begin(), std_vec_false.begin(), pred);
  const std::size_t std_diff_true  = std_result.first - std_vec_true.begin();
  const std::size_t std_diff_false = std_result.second - std_vec_false.begin();
  const std::size_t my_diff_true = my_result.first - KE::begin(view_dest_true);
  const std::size_t my_diff_false =
      my_result.second - KE::begin(view_dest_false);
  EXPECT_TRUE(std_diff_true == my_diff_true);
  EXPECT_TRUE(std_diff_false == my_diff_false);

  auto view_dest_true_h = create_host_space_copy(view_dest_true);
  for (std::size_t i = 0; i < std_diff_true; ++i) {
    EXPECT_TRUE(std_vec_true[i] == view_dest_true_h(i));
    // std::cout << "i= " << i << " "
    // 	      << " std_true = " << std_vec_true[i] << " "
    // 	      << " mine     = " << view_dest_true_h(i) << '\n';
  }

  auto view_dest_false_h = create_host_space_copy(view_dest_false);
  for (std::size_t i = 0; i < std_diff_false; ++i) {
    EXPECT_TRUE(std_vec_false[i] == view_dest_false_h(i));
    // std::cout << "i= " << i << " "
    // 	      << " std_false = " << std_vec_false[i] << " "
    // 	      << " mine     = " << view_dest_false_h(i) << '\n';
  }

  if (name == "empty") {
    EXPECT_TRUE(my_diff_true == 0);
    EXPECT_TRUE(my_diff_false == 0);
  }

  else if (name == "one-element-a") {
    EXPECT_TRUE(my_diff_true == 0);
    EXPECT_TRUE(my_diff_false == 1);
  }

  else if (name == "one-element-b") {
    EXPECT_TRUE(my_diff_true == 1);
    EXPECT_TRUE(my_diff_false == 0);
  }

  else if (name == "two-elements-a") {
    EXPECT_TRUE(my_diff_true == 1);
    EXPECT_TRUE(my_diff_false == 1);
  }

  else if (name == "two-elements-b") {
    EXPECT_TRUE(my_diff_true == 1);
    EXPECT_TRUE(my_diff_false == 1);
  }

  else if (name == "small-b") {
    EXPECT_TRUE(my_diff_true == 13);
    EXPECT_TRUE(my_diff_false == 0);
  }

  else if (name == "small-c") {
    EXPECT_TRUE(my_diff_true == 0);
    EXPECT_TRUE(my_diff_false == 15);
  }
}

std::string value_type_to_string(int) { return "int"; }
std::string value_type_to_string(double) { return "double"; }

template <class Tag, class ValueType, class InfoType>
void run_single_scenario(const InfoType& scenario_info) {
  using exespace             = Kokkos::DefaultExecutionSpace;
  const auto name            = std::get<0>(scenario_info);
  const std::size_t view_ext = std::get<1>(scenario_info);
  std::cout << "partition_copy: " << name << ", " << view_tag_to_string(Tag{})
            << ", " << value_type_to_string(ValueType()) << std::endl;

  auto view_from =
      create_view<ValueType>(Tag{}, view_ext, "partition_copy_from");
  IsEvenFunctor<ValueType> pred;

  {
    auto view_dest_true =
        create_view<ValueType>(Tag{}, view_ext, "partition_copy_dest_true");
    auto view_dest_false =
        create_view<ValueType>(Tag{}, view_ext, "partition_copy_dest_false");
    fill_view(view_from, name);
    auto result = KE::partition_copy(
        exespace(), KE::cbegin(view_from), KE::cend(view_from),
        KE::begin(view_dest_true), KE::begin(view_dest_false), pred);
    verify_data(name, result, view_from, view_dest_true, view_dest_false, pred);
  }

  {
    auto view_dest_true =
        create_view<ValueType>(Tag{}, view_ext, "partition_copy_dest_true");
    auto view_dest_false =
        create_view<ValueType>(Tag{}, view_ext, "partition_copy_dest_false");
    fill_view(view_from, name);
    auto result = KE::partition_copy(
        "my_label", exespace(), KE::cbegin(view_from), KE::cend(view_from),
        KE::begin(view_dest_true), KE::begin(view_dest_false), pred);
    verify_data(name, result, view_from, view_dest_true, view_dest_false, pred);
  }

  {
    auto view_dest_true =
        create_view<ValueType>(Tag{}, view_ext, "partition_copy_dest_true");
    auto view_dest_false =
        create_view<ValueType>(Tag{}, view_ext, "partition_copy_dest_false");
    fill_view(view_from, name);
    auto result = KE::partition_copy(exespace(), view_from, view_dest_true,
                                     view_dest_false, pred);
    verify_data(name, result, view_from, view_dest_true, view_dest_false, pred);
  }

  {
    auto view_dest_true =
        create_view<ValueType>(Tag{}, view_ext, "partition_copy_dest_true");
    auto view_dest_false =
        create_view<ValueType>(Tag{}, view_ext, "partition_copy_dest_false");
    fill_view(view_from, name);
    auto result = KE::partition_copy("my_label", exespace(), view_from,
                                     view_dest_true, view_dest_false, pred);
    verify_data(name, result, view_from, view_dest_true, view_dest_false, pred);
  }

  Kokkos::fence();
}

template <class Tag, class ValueType>
void run_all_scenarios() {
  const std::map<std::string, std::size_t> scenarios = {
      {"empty", 0},          {"one-element-a", 1},
      {"one-element-b", 1},  {"two-elements-a", 2},
      {"two-elements-b", 2}, {"small-a", 9},
      {"small-b", 13},       {"small-c", 15},
      {"medium", 103}};  //      {"large", 101513}};

  for (const auto& it : scenarios) {
    run_single_scenario<Tag, ValueType>(it);
  }
}

TEST(std_algorithms_partitioning_ops, partition_copy) {
  run_all_scenarios<DynamicTag, int>();
  run_all_scenarios<StridedThreeTag, int>();
}

}  // namespace PartitionCopy
}  // namespace stdalgos
}  // namespace Test
