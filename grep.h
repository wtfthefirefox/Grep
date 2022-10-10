#pragma once

#include <optional>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "utf8.h" // default utf8 libary
#include <cstddef>

using std::optional;

struct GrepOptions {
    optional<size_t> look_ahead_length;
    size_t max_matches_per_line;

    GrepOptions() {
        max_matches_per_line = 10;
    }

    GrepOptions(size_t look_ahead_length) : GrepOptions() {
        this->look_ahead_length = look_ahead_length;
    }

    GrepOptions(optional<size_t> look_ahead_length, size_t max_matches_per_line) {
        this->look_ahead_length = look_ahead_length;
        this->max_matches_per_line = max_matches_per_line;
    }
};

static bool IsFileValid(const std::string& file_name) {
    std::ifstream file(file_name);

    if (!file.is_open()) {
        return false;
    }

    if (std::filesystem::status(file_name).type() == std::filesystem::file_type::symlink) {
        return false;
    }

    std::istreambuf_iterator<char> it(file.rdbuf());

    if (*it == '\0') {  // file is empty
        return false;
    }

    std::istreambuf_iterator<char> eos;

    return utf8::is_valid(it, eos);
}

static std::string ConverterToUTF(std::string::iterator it, std::string::iterator end,
                                  int patter_size, int len) {
    while (patter_size > 0) {
        ++it;
        --patter_size;
    }

    std::string::iterator end_it;

    if (len == -1) {
        end_it = end;
    } else {
        end_it = it;

        for (size_t i = 0; i < len; ++i) {
            ++end_it;
            while (end_it != end && (*end_it & 0xC0) == 0x80) {
                ++end_it;
            }
        }
    }

    std::string res(it, end_it);
    return res;
}

template <class Visitor>
static void ParseFile(const std::string& path, const std::string& pattern, Visitor visitor,
                      const GrepOptions& options) {

    std::ifstream file(path);

    if (!file.is_open()) {
        visitor.OnError("i do not have enough rights for open file");
        return;
    }

    if (!IsFileValid(path)) {
        visitor.OnError("file " + path + " is not UTF-8 format");
        return;
    }

    size_t cur_line = 1;
    std::string line;
    size_t max_count = options.max_matches_per_line;
    int count_after =
        options.look_ahead_length == std::nullopt ? -1 : options.look_ahead_length.value();
    size_t pattern_size = pattern.size();

    while (std::getline(file, line)) {
        size_t founded_count = 0;
        auto it = std::search(line.begin(), line.end(),
                              std::default_searcher(pattern.begin(), pattern.end()));

        while (it != line.end() && founded_count < max_count) {
            ++founded_count;
            if (count_after < 0) {
                visitor.OnMatch(path, cur_line, utf8::distance(line.begin(), it) + 1, std::nullopt);
            } else {
                if (count_after <= utf8::distance(it, line.end()) - 2) {
                    visitor.OnMatch(path, cur_line, utf8::distance(line.begin(), it) + 1,
                                    ConverterToUTF(it, line.end(), pattern_size, count_after));
                } else {
                    visitor.OnMatch(path, cur_line, utf8::distance(line.begin(), it) + 1,
                                    ConverterToUTF(it, line.end(), pattern_size, -1));
                }
            }

            ++it;
            it = std::search(it, line.end(), std::default_searcher(pattern.begin(), pattern.end()));
        }

        ++cur_line;
    }
}

template <class Visitor>
static void GrepSearcher(const std::string& path, const std::string& pattern, Visitor visitor,
                         const GrepOptions& options) {
    if (std::filesystem::is_directory(path)) {
        for (auto const& dir_entry : std::filesystem::directory_iterator(path)) {
            if (std::filesystem::is_directory(dir_entry.path())) {
                std::cout << dir_entry.path() << " is directory" << std::endl;
                GrepSearcher(dir_entry.path(), pattern, visitor, options);
            } else {
                ParseFile(dir_entry.path(), pattern, visitor, options);
            }
        }
    } else {
        ParseFile(path, pattern, visitor, options);
    }
}

template <class Visitor>
void Grep(const std::string& path, const std::string& pattern, Visitor visitor,
          const GrepOptions& options) {
    GrepSearcher(path, pattern, visitor, options);
}
