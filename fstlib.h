//
//  fstlib.h
//
//  Copyright (c) 2020 Yuji Hirose. All rights reserved.
//  MIT License
//

#ifndef CPPFSTLIB_FSTLIB_H_
#define CPPFSTLIB_FSTLIB_H_

#include <algorithm>
#include <any>
#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fst {

//-----------------------------------------------------------------------------
// variable byte encoding
//-----------------------------------------------------------------------------

template <typename Val> inline size_t vb_encode_value_length(Val n) {
  size_t len = 0;
  while (n >= 128) {
    len++;
    n >>= 7;
  }
  len++;
  return len;
}

template <typename Val> inline size_t vb_encode_value(Val n, char *out) {
  size_t len = 0;
  while (n >= 128) {
    out[len] = (char)(n & 0x7f);
    len++;
    n >>= 7;
  }
  out[len] = (char)(n + 128);
  len++;
  return len;
}

template <typename Val, typename Cont> void vb_encode_value(Val n, Cont &out) {
  while (n >= 128) {
    out.push_back((typename Cont::value_type)(n & 0x7f));
    n >>= 7;
  }
  out.push_back((typename Cont::value_type)(n + 128));
}

template <typename Val>
inline size_t vb_encode_value_reverse(Val n, char *out) {
  auto len = vb_encode_value(n, out);
  for (size_t i = 0; i < len / 2; i++) {
    std::swap(out[i], out[len - i - 1]);
  }
  return len;
}

template <typename Val>
inline size_t vb_decode_value_reverse(const char *data, Val &n) {
  auto p = (const uint8_t *)data;
  int i = 0;
  n = 0;
  size_t cnt = 0;
  while (p[i] < 128) {
    n += (p[i--] << (7 * cnt++));
  }
  n += (p[i--] - 128) << (7 * cnt);
  return i * -1;
}

//-----------------------------------------------------------------------------
// MurmurHash64B - 64-bit MurmurHash2 for 32-bit platforms
//
// URL:: https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp
// License: Public Domain
//-----------------------------------------------------------------------------

inline uint64_t MurmurHash64B(const void *key, size_t len, uint64_t seed) {
  const uint32_t m = 0x5bd1e995;
  const size_t r = 24;

  uint32_t h1 = uint32_t(seed) ^ uint32_t(len);
  uint32_t h2 = uint32_t(seed >> 32);

  const uint32_t *data = (const uint32_t *)key;

  while (len >= 8) {
    uint32_t k1 = *data++;
    k1 *= m;
    k1 ^= k1 >> r;
    k1 *= m;
    h1 *= m;
    h1 ^= k1;
    len -= 4;

    uint32_t k2 = *data++;
    k2 *= m;
    k2 ^= k2 >> r;
    k2 *= m;
    h2 *= m;
    h2 ^= k2;
    len -= 4;
  }

  if (len >= 4) {
    uint32_t k1 = *data++;
    k1 *= m;
    k1 ^= k1 >> r;
    k1 *= m;
    h1 *= m;
    h1 ^= k1;
    len -= 4;
  }

  switch (len) {
  case 3: h2 ^= ((unsigned char *)data)[2] << 16;
  case 2: h2 ^= ((unsigned char *)data)[1] << 8;
  case 1: h2 ^= ((unsigned char *)data)[0]; h2 *= m;
  };

  h1 ^= h2 >> 18;
  h1 *= m;
  h2 ^= h1 >> 22;
  h2 *= m;
  h1 ^= h2 >> 17;
  h1 *= m;
  h2 ^= h1 >> 19;
  h2 *= m;

  uint64_t h = h1;

  h = (h << 32) | h2;

  return h;
}

//-----------------------------------------------------------------------------
// get_prefix_length
//-----------------------------------------------------------------------------

inline size_t get_prefix_length(const std::string &s1, const std::string &s2) {
  size_t i = 0;
  while (i < s1.size() && i < s2.size() && s1[i] == s2[i]) {
    i++;
  }
  return i;
}

inline bool get_prefix_length(const std::string &s1, const std::string &s2,
                              size_t &l) {
  l = 0;
  while (l < s1.size() && l < s2.size()) {
    auto ch1 = static_cast<uint8_t>(s1[l]);
    auto ch2 = static_cast<uint8_t>(s2[l]);
    if (ch1 < ch2) { break; }
    if (ch1 > ch2) { return false; }
    l++;
  }
  return true;
}

//-----------------------------------------------------------------------------
// state output traits
//-----------------------------------------------------------------------------

template <typename output_t> struct OutputTraits {};

template <> struct OutputTraits<uint32_t> {
  using value_type = uint32_t;

  static value_type initial_value() { return 0; }

  static bool empty(value_type val) { return val == 0; }

  static void clear(value_type &val) { val = 0; }

  static std::string to_string(value_type val) { return std::to_string(val); }

  static void prepend_value(value_type &base, value_type val) { base += val; }

  static value_type get_suffix(value_type a, value_type b) { return a - b; }

  static value_type get_common_prefix(value_type a, value_type b) {
    return std::min(a, b);
  }

  static size_t write_value(char *buff, size_t buff_len, value_type val) {
    memcpy(&buff[buff_len], &val, sizeof(val));
    return sizeof(val);
  }

  static size_t get_byte_value_size(value_type val) {
    return vb_encode_value_length(val);
  }

  static void write_byte_value(std::ostream &os, value_type val) {
    char vb[16];
    auto vb_len = vb_encode_value_reverse(val, vb);
    os.write(vb, vb_len);
  }

  static size_t read_byte_value(const char *p, value_type &val) {
    return vb_decode_value_reverse(p, val);
  }
};

template <> struct OutputTraits<std::string> {
  using value_type = std::string;

  static value_type initial_value() { return value_type(); }

  static bool empty(const value_type &val) { return val.empty(); }

  static void clear(value_type &val) { val.clear(); }

  static value_type to_string(const value_type &val) { return val; }

  static void prepend_value(value_type &base, const value_type &val) {
    base.insert(0, val);
  }

  static value_type get_suffix(const value_type &a, const value_type &b) {
    return a.substr(b.size());
  }

  static value_type get_common_prefix(const value_type &a,
                                      const value_type &b) {
    return a.substr(0, get_prefix_length(a, b));
  }

  static size_t write_value(char *buff, size_t buff_len,
                            const value_type &val) {
    memcpy(&buff[buff_len], val.data(), val.size());
    return val.size();
  }

  static size_t get_byte_value_size(const value_type &val) {
    return vb_encode_value_length(val.size()) + val.size();
  }

  static void write_byte_value(std::ostream &os, const value_type &val) {
    os.write(val.data(), val.size());
    OutputTraits<uint32_t>::write_byte_value(os, val.size());
  }

  static size_t read_byte_value(const char *p, value_type &val) {
    uint32_t str_len = 0;
    auto vb_len = OutputTraits<uint32_t>::read_byte_value(p, str_len);

    val.resize(str_len);
    memcpy(val.data(), p - vb_len - str_len + 1, str_len);

    return vb_len + str_len;
  }
};

//-----------------------------------------------------------------------------
// State
//-----------------------------------------------------------------------------

template <typename output_t> class State {
public:
  struct Transition {
    State<output_t> *state;
    output_t output;
    size_t id;
    bool final;
    output_t state_output;

    bool operator==(const Transition &rhs) const {
      if (this != &rhs) {
        return output == rhs.output && id == rhs.id && final == rhs.final &&
               state_output == rhs.state_output;
      }
      return true;
    }
  };

  class Transitions {
  public:
    std::vector<char> arcs;
    std::vector<Transition> states_and_outputs;

    bool operator==(const Transitions &rhs) const {
      if (this != &rhs) {
        return arcs == rhs.arcs && states_and_outputs == rhs.states_and_outputs;
      }
      return true;
    }

    size_t size() const { return arcs.size(); }

    bool empty() const { return !size(); }

    int get_index(char arc) const {
      for (size_t i = 0; i < arcs.size(); i++) {
        if (arcs[i] == arc) { return static_cast<int>(i); }
      }
      return -1;
    }

    const output_t &output(char arc) const {
      auto idx = get_index(arc);
      assert(idx != -1);
      return states_and_outputs[idx].output;
    }

    template <typename Functor> void for_each(Functor fn) const {
      for (auto i = 0u; i < arcs.size(); i++) {
        fn(arcs[i], states_and_outputs[i], i);
      }
    }

    template <typename Functor> void for_each_reverse(Functor fn) const {
      for (auto i = arcs.size(); i > 0; i--) {
        auto idx = i - 1;
        fn(arcs[idx], states_and_outputs[idx], idx);
      }
    }

    template <typename Functor> void for_each_arc(Functor fn) const {
      for (auto arc : arcs) {
        fn(arc);
      }
    }

  private:
    void clear() {
      arcs.clear();
      states_and_outputs.clear();
    }

    void set_transition(char arc, State<output_t> *state) {
      auto idx = get_index(arc);
      if (idx == -1) {
        idx = static_cast<int>(arcs.size());
        arcs.push_back(arc);
        states_and_outputs.emplace_back(Transition());
      }
      states_and_outputs[idx].state = state;
      states_and_outputs[idx].id = state->id;
      states_and_outputs[idx].final = state->final;
      states_and_outputs[idx].state_output = state->state_output;
    }

    void set_output(char arc, const output_t &val) {
      auto idx = get_index(arc);
      states_and_outputs[idx].output = val;
    }

    void insert_output(char arc, const output_t &val) {
      auto idx = get_index(arc);
      auto &output = states_and_outputs[idx].output;
      OutputTraits<output_t>::prepend_value(output, val);
    }

    friend class State;
  };

  size_t id;
  bool final = false;
  Transitions transitions;
  output_t state_output = OutputTraits<output_t>::initial_value();

  State(size_t id) : id(id) {}

  const output_t &output(char arc) const { return transitions.output(arc); }

  bool operator==(const State &rhs) const {
    if (this != &rhs) {
      return final == rhs.final && transitions == rhs.transitions &&
             state_output == rhs.state_output;
    }
    return true;
  }

  uint64_t hash() const;

  void set_final(bool final) { this->final = final; }

  void set_transition(char arc, State<output_t> *state) {
    transitions.set_transition(arc, state);
  }

  void set_output(char arc, const output_t &output) {
    transitions.set_output(arc, output);
  }

  void prepend_suffix_to_output(char arc, const output_t &suffix) {
    transitions.insert_output(arc, suffix);
  }

  void push_to_state_outputs(const output_t &output) { state_output = output; }

  void prepend_suffix_to_state_outputs(const output_t &suffix) {
    OutputTraits<output_t>::prepend_value(state_output, suffix);
  }

  void reuse(size_t state_id) {
    id = state_id;
    set_final(false);
    transitions.clear();
    OutputTraits<output_t>::clear(state_output);
  }

private:
  State(const State &) = delete;
  State(State &&) = delete;
};

template <typename output_t> inline uint64_t State<output_t>::hash() const {
  char buff[1024]; // TOOD: large enough?
  size_t buff_len = 0;

  transitions.for_each([&](char arc, const State::Transition &t, size_t i) {
    buff[buff_len++] = arc;

    auto val = static_cast<uint32_t>(t.id);
    memcpy(&buff[buff_len], &val, sizeof(val));
    buff_len += sizeof(val);

    if (!OutputTraits<output_t>::empty(t.output)) {
      buff_len += OutputTraits<output_t>::write_value(buff, buff_len, t.output);
    }
    buff[buff_len++] = '\t';
  });

  if (final && !OutputTraits<output_t>::empty(state_output)) {
    buff[buff_len++] = '\0';
    buff_len +=
        OutputTraits<output_t>::write_value(buff, buff_len, state_output);
  }

  return MurmurHash64B(buff, buff_len, 0);
}

//-----------------------------------------------------------------------------
// StatePool
//-----------------------------------------------------------------------------

template <typename output_t> class StatePool {
public:
  ~StatePool() {
    for (auto p : object_pool_) {
      delete p;
    }
  }

  State<output_t> *New(size_t state_id = -1) {
    auto p = new State<output_t>(state_id);
    object_pool_.insert(p);
    return p;
  }

  void Delete(State<output_t> *p) {
    object_pool_.erase(p);
    delete p;
  }

private:
  std::unordered_set<State<output_t> *> object_pool_;
};

//-----------------------------------------------------------------------------
// Dictionary
//-----------------------------------------------------------------------------

template <typename output_t> class Dictionary {
public:
  Dictionary(StatePool<output_t> &state_pool) : state_pool_(state_pool) {}

  State<output_t> *get(uint64_t key, State<output_t> *state) {
    auto id = bucket_id(key);
    auto [first, second, third] = buckets_[id];
    if (first && *first == *state) { return first; }
    if (second && *second == *state) {
      buckets_[id] = std::make_tuple(second, first, third);
      return second;
    }
    if (third && *third == *state) {
      buckets_[id] = std::make_tuple(third, first, second);
      return third;
    }
    return nullptr;
  }

  void put(uint64_t key, State<output_t> *state) {
    auto id = bucket_id(key);
    auto [first, second, third] = buckets_[id];
    if (third) { state_pool_.Delete(third); }
    buckets_[id] = std::make_tuple(state, first, second);
  }

private:
  StatePool<output_t> &state_pool_;
  static const size_t kBucketCount = 10000;
  size_t bucket_id(uint64_t key) const { return key % kBucketCount; }
  std::tuple<State<output_t> *, State<output_t> *, State<output_t> *>
      buckets_[kBucketCount] = {{nullptr, nullptr, nullptr}};
};

//-----------------------------------------------------------------------------
// find_minimized
//-----------------------------------------------------------------------------

template <typename output_t>
inline std::pair<bool, State<output_t> *>
find_minimized(State<output_t> *state, Dictionary<output_t> &dictionary) {
  auto h = state->hash();

  auto st = dictionary.get(h, state);
  if (st) { return std::make_pair(true, st); }

  // NOTE: COPY_STATE is very expensive...
  dictionary.put(h, state);
  return std::make_pair(false, state);
};

//-----------------------------------------------------------------------------
// get_common_prefix_and_word_suffix
//-----------------------------------------------------------------------------

template <typename output_t>
inline void get_common_prefix_and_word_suffix(const output_t &current_output,
                                              const output_t &output,
                                              output_t &common_prefix,
                                              output_t &word_suffix) {
  common_prefix =
      OutputTraits<output_t>::get_common_prefix(output, current_output);
  word_suffix = OutputTraits<output_t>::get_suffix(output, common_prefix);
}

//-----------------------------------------------------------------------------
// build_fst
//-----------------------------------------------------------------------------

enum class Result { Success, EmptyKey, UnsortedKey, DuplicateKey };

template <typename output_t, typename Input, typename Writer>
inline std::pair<Result, size_t> build_fst(const Input &input, Writer &writer) {

  StatePool<output_t> state_pool;

  Dictionary<output_t> dictionary(state_pool);
  size_t next_state_id = 0;
  size_t line = 1;
  Result result = Result::Success;

  // Main algorithm ported from the technical paper
  std::vector<State<output_t> *> temp_states;
  std::string previous_word;
  temp_states.push_back(state_pool.New(next_state_id++));

  for (const auto &[current_word, _current_output] : input) {
    auto current_output = _current_output;

    if (current_word.empty()) {
      result = Result::EmptyKey;
      return std::make_pair(result, line);
    }

    // The following loop caluculates the length of the longest common
    // prefix of 'current_word' and 'previous_word'
    // auto prefix_length = get_prefix_length(previous_word, current_word);
    size_t prefix_length;
    if (!get_prefix_length(previous_word, current_word, prefix_length)) {
      result = Result::UnsortedKey;
      return std::make_pair(result, line);
    }

    if (previous_word.size() == current_word.size() &&
        previous_word == current_word) {
      result = Result::DuplicateKey;
      return std::make_pair(result, line);
    }

    // We minimize the states from the suffix of the previous word
    for (auto i = previous_word.size(); i > prefix_length; i--) {
      auto [found, state] =
          find_minimized<output_t>(temp_states[i], dictionary);

      if (found) {
        next_state_id--;
      } else {
        writer.write(*state);

        // Ownership of the object in temp_states[i] has been moved to the
        // dictionary...
        temp_states[i] = state_pool.New();
      }

      auto arc = previous_word[i - 1];
      temp_states[i - 1]->set_transition(arc, state);
    }

    // This loop initializes the tail states for the current word
    for (auto i = prefix_length + 1; i <= current_word.size(); i++) {
      assert(i <= temp_states.size());
      if (i == temp_states.size()) {
        temp_states.push_back(state_pool.New(next_state_id++));
      } else {
        temp_states[i]->reuse(next_state_id++);
      }
      auto arc = current_word[i - 1];
      temp_states[i - 1]->set_transition(arc, temp_states[i]);
    }

    if (current_word != previous_word) {
      auto state = temp_states[current_word.size()];
      state->set_final(true);
      // NOTE: The following code causes bad performance...
      // state->push_to_state_outputs("");
    }

    for (auto j = 1u; j <= prefix_length; j++) {
      auto prev_state = temp_states[j - 1];
      auto arc = current_word[j - 1];

      const auto &output = prev_state->output(arc);

      auto common_prefix = OutputTraits<output_t>::initial_value();
      auto word_suffix = OutputTraits<output_t>::initial_value();
      get_common_prefix_and_word_suffix(current_output, output, common_prefix,
                                        word_suffix);

      prev_state->set_output(arc, common_prefix);

      if (!OutputTraits<output_t>::empty(word_suffix)) {
        auto state = temp_states[j];
        state->transitions.for_each_arc([&](char arc) {
          state->prepend_suffix_to_output(arc, word_suffix);
        });
        if (state->final) {
          state->prepend_suffix_to_state_outputs(word_suffix);
        }
      }

      current_output =
          OutputTraits<output_t>::get_suffix(current_output, common_prefix);
    }

    if (current_word == previous_word) {
      auto state = temp_states[current_word.size()];
      state->push_to_state_outputs(current_output);
    } else {
      auto state = temp_states[prefix_length];
      auto arc = current_word[prefix_length];
      state->set_output(arc, current_output);
    }

    previous_word = current_word;
    line++;
  }

  // Here we are minimizing the states of the last word
  for (auto i = static_cast<int>(previous_word.size()); i >= 0; i--) {
    auto [found, state] = find_minimized<output_t>(temp_states[i], dictionary);

    if (found) {
      next_state_id--;
    } else {
      writer.write(*state);
    }

    if (i > 0) {
      auto arc = previous_word[i - 1];
      temp_states[i - 1]->set_transition(arc, state);
    }
  }

  return std::make_pair(Result::Success, line);
}

//-----------------------------------------------------------------------------
// compile
//-----------------------------------------------------------------------------

enum class ContainerType { Set, Map };
enum class ValueType { Uint32, String };

template <typename output_t> struct FstTraits {};

template <> struct FstTraits<uint32_t> {
  static ValueType get_value_type() { return ValueType::Uint32; }
};

template <> struct FstTraits<std::string> {
  static ValueType get_value_type() { return ValueType::String; }
};

struct FstHeader {
  static const size_t CHAR_INDEX_SIZE = 8;

  uint8_t container_type = 0;
  uint8_t value_type = 0;
  uint8_t reserved = 0;
  uint32_t start_address = 0;
  char char_index[CHAR_INDEX_SIZE] = {0};

  FstHeader() {}

  FstHeader(ContainerType container_type, ValueType value_type,
            size_t start_address, const std::vector<size_t> &char_index_table)
      : container_type(static_cast<uint8_t>(container_type)),
        value_type(static_cast<uint8_t>(value_type)),
        start_address(start_address) {
    for (size_t ch = 0; ch < 256; ch++) {
      auto index = char_index_table[ch];
      if (0 < index && index < CHAR_INDEX_SIZE) {
        char_index[index] = static_cast<char>(ch);
      }
    }
  }

  void write(std::ostream &os) {
    os.write(reinterpret_cast<const char *>(this), sizeof(*this));
  }
};

union FstFlags {
  struct {
    unsigned no_address : 1;
    unsigned last_transition : 1;
    unsigned final : 1;
    unsigned has_output : 1;
    unsigned has_state_output : 1;
    unsigned label_index : 3;
  } data;
  uint8_t byte;
};

template <typename output_t> struct FstRecord {
  FstFlags flags;

  char label = 0;
  size_t delta = 0;
  const output_t *output = nullptr;
  const output_t *state_output = nullptr;

  size_t byte_size() const {
    size_t sz = 1;
    if (flags.data.label_index == 0) { sz += 1; }
    if (!flags.data.no_address) { sz += vb_encode_value_length(delta); }
    if (flags.data.has_output) {
      sz += OutputTraits<output_t>::get_byte_value_size(*output);
    }
    if (flags.data.has_state_output) {
      sz += OutputTraits<output_t>::get_byte_value_size(*state_output);
    }
    return sz;
  }

  void write(std::ostream &os) {
    if (flags.data.has_state_output) {
      OutputTraits<output_t>::write_byte_value(os, *state_output);
    }
    if (flags.data.has_output) {
      OutputTraits<output_t>::write_byte_value(os, *output);
    }
    if (!flags.data.no_address) {
      OutputTraits<uint32_t>::write_byte_value(os, delta);
    }
    if (flags.data.label_index == 0) { os << label; }
    os.write(reinterpret_cast<const char *>(&flags.byte), sizeof(flags.byte));
  }
};

template <typename output_t, typename Input> class ByteCodeWriter {
public:
  ByteCodeWriter(const Input &input, std::ostream &os, bool need_output,
                 bool dump, bool verbose)
      : os_(os), need_output_(need_output), dump_(dump), verbose_(verbose) {

    intialize_char_index_table_(input);

    if (dump_) {
      std::cout << "Address\tArc\tN F L\tNxtAddr";
      if (need_output_) { std::cout << "\tOutput\tStOuts\tSize"; }
      std::cout << std::endl;
      std::cout << "-------\t---\t-----\t-------";
      if (need_output_) { std::cout << "\t------\t------\t----"; }
      std::cout << std::endl;
    }
  }

  ~ByteCodeWriter() {
    if (record_index_ == 0) { return; }

    auto container_type =
        need_output_ ? ContainerType::Map : ContainerType::Set;

    auto start_byte_adress = address_table_[record_index_ - 1];

    FstHeader header(container_type, FstTraits<output_t>::get_value_type(),
                     start_byte_adress, char_index_table_);

    if (!dump_) { header.write(os_); }

    if (verbose_) {
      std::cerr << "# unique char count: " << char_count_.size() << std::endl;
      std::cerr << "# state output count: " << state_output_count_ << std::endl;
      std::cerr << "# record count: " << record_index_ << std::endl;
      std::cerr << "# total size: " << total_size_ + sizeof(header)
                << std::endl;
    }
  }

  void write(const State<output_t> &state) {
    auto transition_count = state.transitions.size();

    state.transitions.for_each_reverse([&](char arc, const auto &t, size_t i) {
      auto recored_index_iter = record_index_map_.find(t.id);
      auto has_address = recored_index_iter != record_index_map_.end();
      auto last_transition = transition_count - 1 == i;
      auto no_address = last_transition && has_address &&
                        record_index_map_[t.id] == record_index_ - 1;

      FstRecord<output_t> rec;
      rec.flags.data.no_address = no_address;
      rec.flags.data.last_transition = last_transition;
      rec.flags.data.final = t.final;

      auto index = char_index_table_[static_cast<uint8_t>(arc)];
      if (index < FstHeader::CHAR_INDEX_SIZE) {
        rec.flags.data.label_index = index;
      } else {
        rec.flags.data.label_index = 0;
        rec.label = arc;
      }

      rec.delta = 0;
      if (!no_address) {
        if (has_address) {
          rec.delta = address_ - address_table_[recored_index_iter->second];
        }
      }

      rec.flags.data.has_output = false;
      if (need_output_) {
        if (!OutputTraits<output_t>::empty(t.output)) {
          rec.flags.data.has_output = true;
          rec.output = &t.output;
        }
      }

      rec.flags.data.has_state_output = false;
      if (need_output_) {
        if (!OutputTraits<output_t>::empty(t.state_output)) {
          rec.flags.data.has_state_output = true;
          rec.state_output = &t.state_output;
        }
      }

      auto byte_size = rec.byte_size();
      auto accessible_address = address_ + byte_size - 1;

      address_table_.push_back(accessible_address);

      if (!dump_) {
        rec.write(os_);
      } else {
        // Byte address
        std::cout << address_table_[record_index_] << "\t";

        // Arc
        if (arc < 0x20) {
          std::cout << std::hex << (int)(uint8_t)arc << std::dec;
        } else {
          std::cout << arc;
        }
        std::cout << "\t";

        // Flags
        std::cout << (no_address ? "↑" : " ") << ' ' << (t.final ? '*' : ' ')
                  << ' ' << (last_transition ? "‾" : " ") << "\t";

        // Next Address
        if (!no_address) {
          if (rec.delta > 0) {
            std::cout << address_ - rec.delta;
          } else {
            std::cout << "x";
          }
        }
        std::cout << "\t";

        // Output
        if (need_output_) {
          if (!OutputTraits<output_t>::empty(t.output)) {
            std::cout << t.output;
          }
        }
        std::cout << "\t";

        // State Output
        if (need_output_) {
          if (!OutputTraits<output_t>::empty(t.state_output)) {
            std::cout << t.state_output;
            state_output_count_++;
          }
        }

        std::cout << "\t" << byte_size;
        std::cout << std::endl;
      }

      total_size_ += byte_size;

      record_index_ += 1;
      address_ += byte_size;
    });

    if (!state.transitions.empty()) {
      record_index_map_[state.id] = record_index_ - 1;
    }
  }

private:
  void intialize_char_index_table_(const Input &input) {
    char_index_table_.assign(256, 0);

    for (const auto &[word, _] : input) {
      for (auto ch : word) {
        char_count_[ch]++;
      }
    }

    struct second_order {
      bool operator()(const std::pair<char, size_t> &x,
                      const std::pair<char, size_t> &y) const {
        return x.second < y.second;
      }
    };

    std::priority_queue<std::pair<char, size_t>,
                        std::vector<std::pair<char, size_t>>, second_order>
        que;

    for (auto x : char_count_) {
      que.push(x);
    }

    size_t index = 1;
    while (!que.empty()) {
      auto [ch, count] = que.top();
      char_index_table_[static_cast<uint8_t>(ch)] = index++;
      que.pop();
    }
  }

  std::ostream &os_;
  bool need_output_ = true;
  size_t dump_ = true;
  size_t verbose_ = true;

  std::map<char, size_t> char_count_;
  std::vector<size_t> char_index_table_;

  size_t record_index_ = 0;
  std::unordered_map<size_t, size_t> record_index_map_;

  size_t address_ = 0;
  std::vector<size_t> address_table_;

  size_t total_size_ = 0;
  size_t state_output_count_ = 0;
};

template <typename output_t, typename Input>
inline std::pair<Result, size_t> compile(const Input &input, std::ostream &os,
                                         bool need_output, bool verbose) {

  ByteCodeWriter<output_t, Input> writer(input, os, need_output, false,
                                         verbose);
  return build_fst<output_t>(input, writer);
}

template <typename output_t, typename Input>
inline std::pair<Result, size_t> dump(const Input &input, std::ostream &os,
                                      bool verbose) {

  ByteCodeWriter<output_t, Input> writer(input, os, true, true, verbose);
  return build_fst<output_t>(input, writer);
}

//-----------------------------------------------------------------------------
// dot
//-----------------------------------------------------------------------------

template <typename output_t> class DotWriter {
public:
  DotWriter(std::ostream &os) : os_(os) {
    os_ << "digraph{" << std::endl;
    os_ << "  rankdir = LR;" << std::endl;
  }

  ~DotWriter() { os_ << "}" << std::endl; }

  void write(const State<output_t> &state) {
    if (state.final) {
      output_t state_output;
      if (!OutputTraits<output_t>::empty(state.state_output)) {
        state_output = state.state_output;
      }
      os_ << "  s" << state.id << " [ shape = doublecircle, xlabel = \""
          << state_output << "\" ];" << std::endl;
    } else {
      os_ << "  s" << state.id << " [ shape = circle ];" << std::endl;
    }

    state.transitions.for_each(
        [&](char arc, const typename State<output_t>::Transition &t, size_t i) {
          std::string label;
          label += arc;
          os_ << "  s" << state.id << "->s" << t.id << " [ label = \"" << label;
          if (!OutputTraits<output_t>::empty(t.output)) {
            os_ << "/" << t.output;
          }
          os_ << "\" ];" << std::endl;
        });
  }

private:
  std::ostream &os_;
};

template <typename output_t, typename Input>
inline std::pair<Result, size_t> dot(const Input &input, std::ostream &os) {

  DotWriter<output_t> writer(os);
  return build_fst<output_t>(input, writer);
}

//-----------------------------------------------------------------------------
// Matcher
//-----------------------------------------------------------------------------

template <typename output_t> class Matcher {
public:
  Matcher(const char *byte_code, size_t byte_code_size, bool need_output)
      : byte_code_(byte_code), byte_code_size_(byte_code_size),
        need_output_(need_output) {

    if (byte_code_size < sizeof(header_)) { return; }

    auto p = byte_code_ + (byte_code_size - sizeof(header_));
    memcpy(reinterpret_cast<char *>(&header_), p, sizeof(header_));

    if (static_cast<ContainerType>(header_.container_type) !=
        (need_output ? ContainerType::Map : ContainerType::Set)) {
      return;
    }

    if (static_cast<ValueType>(header_.value_type) !=
        FstTraits<output_t>::get_value_type()) {
      return;
    }

    is_valid_ = true;
  }

  operator bool() { return is_valid_; }

  void set_trace(bool on) { trace_ = on; }

  bool exact_match_search(const char *str, size_t len, output_t &output) const {
    return match(str, len, [&](const auto &_) { output = _; });
  }

  bool common_prefix_search(
      const char *str, size_t len,
      std::function<void(size_t, const output_t &)> prefixes) const {
    return match(str, len, nullptr, prefixes);
  }

  bool match(
      const char *str, size_t len,
      std::function<void(const output_t &)> outputs = nullptr,
      std::function<void(size_t, const output_t &)> prefixes = nullptr) const {

    if (trace_) {
      std::cout << "Char\tAddress\tArc\tN F L\tNxtAddr\tOutput\tStOuts"
                << std::endl;
      std::cout << "----\t-------\t---\t-----\t-------\t------\t------"
                << std::endl;
    }

    auto ret = false;
    auto output = OutputTraits<output_t>::initial_value();
    auto state_output = OutputTraits<output_t>::initial_value();

    size_t address = header_.start_address;
    size_t i = 0;
    while (i < len) {
      auto ch = str[i];
      OutputTraits<output_t>::clear(state_output);

      // TODO:
      // if (i == 0) {
      //   if (auto it = jump_table_.find(ch); it != jump_table_.end()) {
      //     address = header_.start_address - it->second;
      //   }
      // }

      auto end = byte_code_ + address;
      auto p = end;

      FstFlags flags;
      flags.byte = *p--;

      auto index = flags.data.label_index;
      char arc = 0;
      if (index == 0) {
        arc = *p--;
      } else {
        arc = header_.char_index[index];
      }

      // TODO:
      // if (i == 0) { jump_table_[arc] = header_.start_address - address; }

      size_t delta = 0;
      if (!flags.data.no_address) { p -= vb_decode_value_reverse(p, delta); }

      auto output_suffix = OutputTraits<output_t>::initial_value();
      if (flags.data.has_output) {
        p -= OutputTraits<output_t>::read_byte_value(p, output_suffix);
      }

      if (flags.data.has_state_output) {
        p -= OutputTraits<output_t>::read_byte_value(p, state_output);
      }

      auto byte_size = std::distance(p, end);

      size_t next_address = 0;
      if (!flags.data.no_address) {
        if (delta) { next_address = address - byte_size - delta + 1; }
      } else {
        next_address = address - byte_size;
      }

      if (trace_) {
        std::cout << ch << "\t";
        std::cout << address << "\t";
        std::cout << arc << "\t";
        std::cout << (flags.data.no_address ? "↑" : " ") << ' '
                  << (flags.data.final ? '*' : ' ') << ' '
                  << (flags.data.last_transition ? "‾" : " ") << "\t";

        // Next Address
        if (next_address) {
          std::cout << next_address;
        } else {
          std::cout << "x";
        }
        std::cout << "\t";

        if (flags.data.has_output) { std::cout << output_suffix; }
        std::cout << "\t";

        if (flags.data.has_state_output) { std::cout << state_output; }
        std::cout << "\t";

        std::cout << byte_size;
        std::cout << std::endl;
      }

      if (ch == arc) {
        output += output_suffix;
        i++;
        if (flags.data.final) {
          if (i == len) {
            if (outputs) {
              if (OutputTraits<output_t>::empty(state_output)) {
                outputs(output);
              } else {
                outputs(output + state_output);
              }
            }
            ret = true;
            break;
          }
          if (prefixes) {
            if (OutputTraits<output_t>::empty(state_output)) {
              prefixes(i, output);
            } else {
              prefixes(i, output + state_output);
            }
          }
        }
        if (!next_address) { break; }
        address = next_address;
      } else {
        if (flags.data.last_transition) { break; }
        address = address - byte_size;
      }
    }

    return ret;
  }

private:
  const char *byte_code_;
  size_t byte_code_size_;
  bool need_output_ = false;

  FstHeader header_;
  bool is_valid_ = false;
  bool trace_ = false;

  mutable std::unordered_map<char, uint16_t> jump_table_;
};

} // namespace fst

#endif // CPPFSTLIB_FSTLIB_H_
