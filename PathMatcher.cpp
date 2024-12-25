#include "PathMatcher.hpp"

#include <stack>

void PathMatcher::insert(const std::vector<std::string> &pathSegments) const {
    TrieNode *current = mRoot;
    for (const auto &segment: pathSegments) {
        if (!current->children.contains(segment)) {
            current->children[segment] = new TrieNode();
        }
        current = current->children[segment];
    }
    current->isEndOfPath = true;
}

bool PathMatcher::match(const std::vector<std::string> &pathSegments) const {
    auto stack = std::stack<std::pair<TrieNode *, size_t> >({{mRoot, 0}});

    while (!stack.empty()) {
        auto [node, index] = stack.top();
        stack.pop();

        if (index == pathSegments.size()) {
            if (node->isEndOfPath) {
                return true;
            }

            continue;
        }

        const std::string &segment = pathSegments[index];

        if (node->children.contains(segment)) {
            stack.emplace(node->children[segment], index + 1);
        }

        if (node->children.contains("*")) {
            stack.emplace(node->children["*"], index + 1);
        }
    }

    return false;
}
