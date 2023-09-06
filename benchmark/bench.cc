#include <sys/types.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "darts_ac/darts_ac.h"

ABSL_FLAG(std::string, keys, "benchmark/data/key.sort.txt", "Keys");
ABSL_FLAG(std::string, text, "benchmark/data/wagahaiwa_nekodearu.txt", "Text");

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage("Benchmark nori tokenizer");
  absl::ParseCommandLine(argc, argv);

  std::cout << "start benchmark" << std::endl;
  std::cout << "Reading keys from " << absl::GetFlag(FLAGS_keys) << std::endl;
  std::vector<std::string> keyLines;
  {
    std::ifstream ifs(absl::GetFlag(FLAGS_keys));
    CHECK(ifs.good()) << "Cannot open " << absl::GetFlag(FLAGS_keys);
    std::string line;
    while (std::getline(ifs, line)) {
      keyLines.push_back(line);
    }
  }

  std::stable_sort(
      keyLines.begin(), keyLines.end(),
      [](const std::string& a, const std::string& b) { return a < b; });

  std::cout << "Reading text from " << absl::GetFlag(FLAGS_text) << std::endl;
  std::vector<std::string> textLines;
  {
    std::ifstream ifs(absl::GetFlag(FLAGS_text));
    CHECK(ifs.good()) << "Cannot open " << absl::GetFlag(FLAGS_text);
    std::string line;
    while (std::getline(ifs, line)) {
      textLines.push_back(line);
    }
  }

  std::size_t total_key_bytes = 0;
  for (const auto& line : keyLines) {
    total_key_bytes += line.size();
  }

  std::cout << "Total key bytes\t: " << total_key_bytes << std::endl;
  std::cout << "Total key lines\t: " << keyLines.size() << std::endl;
  std::cout << "#bytes/key\t: " << total_key_bytes / (float)keyLines.size() << std::endl;

  std::cout << "Total text lines: " << textLines.size() << std::endl;

  std::vector<const char*> keys;
  std::vector<std::size_t> lengths;
  for (const auto& key : keyLines) {
    keys.push_back(key.c_str());
    lengths.push_back(key.size());
  }

  auto* ac = new darts_ac::DoubleArrayAhoCorasick();

  std::chrono::steady_clock::time_point begin =
      std::chrono::steady_clock::now();
  CHECK(ac->buildAhoCorasick(keys.size(), keys.data(), lengths.data()) == 0)
      << "Failed to build aho-corasick.";
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  auto construction_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - begin)
          .count();

  std::cout << "construction\t: " << construction_time << " ms." << std::endl;

  begin = std::chrono::steady_clock::now();
  int total_matches = 0;
  for (const auto& text : textLines) {
    const std::size_t maxTrieResults = 1024;
    std::vector<darts_ac::DoubleArrayAhoCorasick::result_pair_type> matches(
        maxTrieResults + 1);

    auto num_results =
        ac->find(text.c_str(), matches.data(), maxTrieResults, text.size());
    total_matches += num_results;
  }
  end = std::chrono::steady_clock::now();
  auto total_nanos =
      std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
          .count();

  std::cout << "total matches\t: " << total_matches << std::endl;
  std::cout << "matching\t: " << total_nanos << " us." << std::endl;
  std::cout << "matching/line\t: " << total_nanos / (float)textLines.size()
            << " us." << std::endl;

  ac->clear();

  return 0;
}
