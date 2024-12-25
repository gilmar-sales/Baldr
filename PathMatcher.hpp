#pragma once

#include <unordered_map>
#include <vector>
#include <string>

struct TrieNode {
    std::unordered_map<std::string, TrieNode *> children;
    bool isEndOfPath = false;
};

class PathMatcher {
public:
    PathMatcher() {
        mRoot = new TrieNode();
    }

    void insert(const std::vector<std::string> &pathSegments) const;

    [[nodiscard]] bool match(const std::vector<std::string> &pathSegments) const;

private:
    TrieNode *mRoot;
};
