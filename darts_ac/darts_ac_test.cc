#include "darts_ac/darts_ac.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "absl/strings/string_view.h"

darts_ac::DoubleArrayAhoCorasick* build(const std::vector<std::string>& keys) {
  std::vector<const char*> c_keys;
  std::vector<std::size_t> lengths;
  std::size_t num_keys = keys.size();
  for (const auto& key : keys) {
    c_keys.push_back(key.c_str());
    lengths.push_back(key.size());
  }

  darts_ac::DoubleArrayAhoCorasick* ac = new darts_ac::DoubleArrayAhoCorasick();
  if (ac->buildAhoCorasick(num_keys, c_keys.data(), lengths.data()) != 0) {
    throw std::runtime_error("Failed to build aho-corasick.");
  }
  return ac;
}

TEST(DoubleArrayAhoCorasick, build_without_failure) {
  auto* ac = build({"a", "ab", "abc", "abcd", "abcde"});
  ac->clear();
}

TEST(DoubleArrayAhoCorasick, find_works_fine) {
  typedef darts_ac::DoubleArrayAhoCorasick::result_pair_type result_pair_type;

  auto* ac = build({"ab", "bab", "bac", "d", "db", "dd"});

  const std::string text = "ababdd";
  const std::size_t maxTrieResults = 1024;
  std::vector<result_pair_type> matches(maxTrieResults + 1);

  auto num_results =
      ac->find(text.c_str(), matches.data(), maxTrieResults, text.size());

  ASSERT_EQ(num_results, 6);
  ASSERT_THAT(std::vector(matches.data(), matches.data() + num_results),
              ::testing::ElementsAre((result_pair_type){0, 2, 0},  // ab
                                     (result_pair_type){1, 3, 1},  // bab
                                     (result_pair_type){0, 2, 2},  // ab
                                     (result_pair_type){3, 1, 4},  // d
                                     (result_pair_type){5, 2, 4},  // dd
                                     (result_pair_type){3, 1, 5}   // d
                                     ));
  ac->clear();
}

TEST(DoubleArrayAhoCorasick, find_single_char) {
  typedef darts_ac::DoubleArrayAhoCorasick::result_pair_type result_pair_type;

  auto* ac = build({"S"});

  const std::string text = "SSS";
  const std::size_t maxTrieResults = 1024;
  std::vector<result_pair_type> matches(maxTrieResults + 1);

  auto num_results =
      ac->find(text.c_str(), matches.data(), maxTrieResults, text.size());

  ASSERT_EQ(num_results, 3);
  ASSERT_THAT(std::vector(matches.data(), matches.data() + num_results),
              ::testing::ElementsAre((result_pair_type){0, 1, 0},  // S
                                     (result_pair_type){0, 1, 1},  // S
                                     (result_pair_type){0, 1, 2}   // S
                                     ));
  ac->clear();
}

TEST(DoubleArrayAhoCorasick, find_test_k) {
  typedef darts_ac::DoubleArrayAhoCorasick::result_pair_type result_pair_type;

  auto* ac = build({"안녕", "요", "하세요"});

  const std::string text = "안녕하세요";
  const std::size_t maxTrieResults = 1024;
  std::vector<result_pair_type> matches(maxTrieResults + 1);

  auto num_results =
      ac->find(text.c_str(), matches.data(), maxTrieResults, text.size());

  ASSERT_EQ(num_results, 3);
  ASSERT_THAT(std::vector(matches.data(), matches.data() + num_results),
              ::testing::ElementsAre((result_pair_type){0, 6, 0},  // 안녕
                                     (result_pair_type){2, 9, 6},  // 하세요
                                     (result_pair_type){1, 3, 12}  // 요
                                     ));
  ac->clear();
}

TEST(DoubleArrayAhoCorasick, find_test_j) {
  typedef darts_ac::DoubleArrayAhoCorasick::result_pair_type result_pair_type;

  auto* ac =
      build({"あ", "ある", "で", "は", "はー", "る", "吾", "吾輩", "猫", "輩"});

  const std::string text = "吾輩は猫である";
  const std::size_t maxTrieResults = 1024;
  std::vector<result_pair_type> matches(maxTrieResults + 1);

  auto num_results =
      ac->find(text.c_str(), matches.data(), maxTrieResults, text.size());

  ASSERT_EQ(num_results, 9);
  ASSERT_THAT(std::vector(matches.data(), matches.data() + num_results),
              ::testing::ElementsAre((result_pair_type){6, 3, 0},   // 吾
                                     (result_pair_type){7, 6, 0},   // 吾輩
                                     (result_pair_type){9, 3, 3},   // 輩
                                     (result_pair_type){3, 3, 6},   // は
                                     (result_pair_type){8, 3, 9},   // 猫
                                     (result_pair_type){2, 3, 12},  // で
                                     (result_pair_type){0, 3, 15},  // あ
                                     (result_pair_type){1, 6, 15},  // ある
                                     (result_pair_type){5, 3, 18}   // る
                                     ));
  ac->clear();
}
