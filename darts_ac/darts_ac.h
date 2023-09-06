#ifndef __DARTS_AC_H__
#define __DARTS_AC_H__

#include <darts.h>

namespace darts_ac {

// Extended from <Darts::DoubleArrayImpl> to support Aho-Corasick algorithm.
//
// This can be little bit inefficient because this class does not have full
// control over the base class.
template <typename A, typename B, typename T, typename C>
class DoubleArrayAhoCorasickImpl : Darts::DoubleArrayImpl<A, B, T, C> {
 public:
  // Even if this <value_type> is changed, the internal value type is still
  // <Darts::Details::value_type>. Other types, such as 64-bit integer types
  // and floating-point number types, should not be used.
  typedef T value_type;
  // A key is reprenseted by a sequence of <key_type>s. For example,
  // exactMatchSearch() takes a <const key_type *>.
  typedef Darts::Details::char_type key_type;
  // In searching dictionaries, the values associated with the matched keys are
  // stored into or returned as <result_type>s.
  typedef value_type result_type;

  // Redefine the <result_pair_type> to support the position of the matched
  // key.
  struct result_pair_type {
    value_type value;
    std::size_t length;
    std::size_t position;

    // for testing.
    bool operator==(const result_pair_type &rhs) const {
      return this->value == rhs.value && this->length == rhs.length &&
             this->position == rhs.position;
    }
  };

  // Build ahocorasick index. keys should be sorted in lexicographical order,
  // and lengths[i] should be the length of keys[i].
  int buildAhoCorasick(
      std::size_t num_keys, const key_type *const *keys,
      const std::size_t *lengths,
      Darts::Details::progress_func_type dart_progress_func = NULL,
      Darts::Details::progress_func_type failure_progress_func = NULL);

  // find() is the main function of this class. It finds all the
  // matched keys in the given text. The matched keys are stored into
  // <results>.
  //
  // For the simplicity, length shouldn't be 0.
  inline std::size_t find(const key_type *key, result_pair_type *results,
                          std::size_t max_num_results,
                          std::size_t length) const;

  void clear() {
    // Inherit the base class's clear() function to delete the failure_ array.
    Darts::DoubleArrayImpl<A, B, T, C>::clear();

    if (failure_ != NULL) {
      delete[] failure_;
      failure_ = NULL;
    }

    if (depth_ != NULL) {
      delete[] depth_;
      depth_ = NULL;
    }
  }

  using Darts::DoubleArrayImpl<A, B, T, C>::array;

  using Darts::DoubleArrayImpl<A, B, T, C>::set_array;

  using Darts::DoubleArrayImpl<A, B, T, C>::size;

  using Darts::DoubleArrayImpl<A, B, T, C>::total_size;

  using Darts::DoubleArrayImpl<A, B, T, C>::unit_size;

  using Darts::DoubleArrayImpl<A, B, T, C>::exactMatchSearch;

  const void *failure() const { return failure_; }

  std::size_t failure_size() const {
    return sizeof(Darts::Details::id_type) * size();
  }

  void set_failure(const void *failure) {
    failure_ = static_cast<const Darts::Details::id_type *>(failure);
  }

  const void *depth() const { return depth_; }

  std::size_t depth_size() const { return sizeof(unsigned int) * size(); }

  void set_depth(const void *depth) {
    depth_ = static_cast<const unsigned int *>(depth);
  }

  // TODO: implement save and open.

 private:
  const Darts::Details::id_type *failure_;
  const unsigned int *depth_;

  // Build a failure function.
  int buildFailureLinks(
      std::size_t num_keys, const key_type *const *keys,
      const std::size_t *lengths,
      Darts::Details::progress_func_type progress_func = NULL);

  Darts::Details::id_type findFailureLink(Darts::Details::id_type* buf, std::size_t parent_node_pos,
                                          const key_type *key,
                                          std::size_t key_pos) const;

  void set_result(result_pair_type *result, value_type value,
                  std::size_t length, std::size_t position) const {
    result->value = value;
    result->length = length;
    result->position = position;
  }
};

// <DoubleArrayAhoCorasick> is the typical instance of
// <DoubleArrayAhoCorasickImpl>. It uses <int> as the type of values and it is
// suitable for most cases.
typedef DoubleArrayAhoCorasickImpl<void, void, int, void>
    DoubleArrayAhoCorasick;

//
// Member functions of <DoubleArrayAhoCorasick>.
//

template <typename A, typename B, typename T, typename C>
int DoubleArrayAhoCorasickImpl<A, B, T, C>::buildAhoCorasick(
    std::size_t num_keys, const key_type *const *keys,
    const std::size_t *lengths,
    Darts::Details::progress_func_type dart_progress_func,
    Darts::Details::progress_func_type failure_progress_func) {
  // Build a double-array trie.
  int ret = this->build(num_keys, keys, lengths, nullptr, dart_progress_func);
  if (ret != 0) {
    return ret;
  }

  // Set depth_.
  auto *buf = new unsigned int[this->size()]();
  for (std::size_t i = 0; i < num_keys; ++i) {
    std::size_t node_pos = 0;
    for (std::size_t j = 0; j < lengths[i]; ++j) {
      std::size_t mutable_key_pos = j;
      if (this->traverse(keys[i], node_pos, mutable_key_pos, j + 1) == -2) {
        return -1;
      }
      buf[node_pos] = j + 1;
    }
  }
  this->depth_ = buf;

  // Build a failure function.
  return buildFailureLinks(num_keys, keys, lengths, failure_progress_func);
}

template <typename A, typename B, typename T, typename C>
inline std::size_t DoubleArrayAhoCorasickImpl<A, B, T, C>::find(
    const key_type *key, result_pair_type *results, std::size_t max_num_results,
    std::size_t length) const {
  std::size_t num_results = 0;
  std::size_t node_pos = 0;  // starts from the root node.

  const Darts::Details::DoubleArrayUnit *array =
      static_cast<const Darts::Details::DoubleArrayUnit *>(this->array());

  Darts::Details::DoubleArrayUnit unit = array[node_pos];
  for (std::size_t i = 0; i < length; ++i) {
    while (true) {
      auto next_node_pos = node_pos ^ unit.offset() ^
                           static_cast<Darts::Details::uchar_type>(key[i]);
      auto next_unit = array[next_node_pos];

      if (next_unit.label() ==
          static_cast<Darts::Details::uchar_type>(key[i])) {
        node_pos = next_node_pos;
        unit = next_unit;
        break;
      }

      // root node cannot follow the failure link.
      if (node_pos == 0) {
        break;
      }

      node_pos = this->failure_[node_pos];
      unit = array[node_pos];
    }

    if (unit.has_leaf()) {
      if (num_results < max_num_results) {
        set_result(
            &results[num_results],
            static_cast<value_type>(array[node_pos ^ unit.offset()].value()),
            depth_[node_pos], i + 1 - depth_[node_pos]);
      }
      ++num_results;
    }

    // TODO: this is little bit inefficient because we traverse the failure
    // links again without knowning there's a match or not. So we can improve
    // this by checking the failure links before traversing.
    auto node_pos_for_output = failure_[node_pos];
    while (node_pos_for_output != 0) {
      auto unit_for_output = array[node_pos_for_output];
      if (unit_for_output.has_leaf()) {
        if (num_results < max_num_results) {
          set_result(&results[num_results],
                     static_cast<value_type>(
                         array[node_pos_for_output ^ unit_for_output.offset()]
                             .value()),
                     depth_[node_pos_for_output],
                     i + 1 - depth_[node_pos_for_output]);
        }
        ++num_results;
      }
      node_pos_for_output = failure_[node_pos_for_output];
    }
  }

  return num_results;
}

template <typename A, typename B, typename T, typename C>
int DoubleArrayAhoCorasickImpl<A, B, T, C>::buildFailureLinks(
    std::size_t num_keys, const key_type *const *keys,
    const std::size_t *lengths,
    Darts::Details::progress_func_type progress_func) {
  // Allocate memory for the failure function and fill zeros.
  auto* buf = new Darts::Details::id_type[this->size()]();

  std::size_t max_length = 0;
  for (std::size_t i = 0; i < num_keys; ++i) {
    if (max_length < lengths[i]) {
      max_length = lengths[i];
    }
  }

  for (std::size_t i = 0; i < max_length; ++i) {
    // Find a failure link for each node.
    for (std::size_t key_index = 0; key_index < num_keys; ++key_index) {
      const key_type *const key = keys[key_index];
      const std::size_t length = lengths[key_index];

      if (length <= i) {
        // The key is too short. Skip.
        continue;
      }

      std::size_t parent_node_pos = 0;
      std::size_t key_pos = 0;
      if (i != 0) {
        auto val = this->traverse(key, parent_node_pos, key_pos, i);
        // when the key is not found in the trie. this is not expected to
        // happen.
        if (val == -2) {
          delete[] buf;
          return -1;
        }
      }

      std::size_t node_pos = parent_node_pos;
      auto val = this->traverse(key, node_pos, key_pos, i + 1);
      if (val == -2) {
        delete[] buf;
        return -1;
      }

      if (buf[node_pos] != 0) {
        // The failure link is already set. Skip.
        continue;
      }

      // Find a failure link.
      const auto failure_node_pos = findFailureLink(buf, parent_node_pos, key, i);
      buf[node_pos] = failure_node_pos;
    }

    if (progress_func != NULL) {
      progress_func(i + 1, max_length + 1);
    }
  }

  failure_ = buf;
  return 0;
}

template <typename A, typename B, typename T, typename C>
Darts::Details::id_type DoubleArrayAhoCorasickImpl<A, B, T, C>::findFailureLink(
  Darts::Details::id_type* buf,
    std::size_t parent_node_pos, const key_type *key,
    std::size_t key_pos) const {
  if (parent_node_pos == 0)  // skip.
    return 0;

  while (true) {
    const auto failure_node_pos = buf[parent_node_pos];

    std::size_t mutable_key_pos = 0;
    std::size_t mutable_node_pos = failure_node_pos;
    auto ret =
        this->traverse(key + key_pos, mutable_node_pos, mutable_key_pos, 1);
    if (ret != -2) {
      return mutable_node_pos;
    }

    if (failure_node_pos == 0) {
      return 0;
    }

    parent_node_pos = failure_node_pos;
  }

  return 0;
}

}  // namespace darts_ac

#endif /* __DARTS_AC_H__ */
