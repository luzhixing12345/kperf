

#pragma once

#include <algorithm>
#include <unordered_map>
#include <string>
#include <vector>

struct TNode {
    int c = 0;
    std::unordered_map<std::string, TNode *> s;
    struct TNode *add(std::string n) {
        c++;
        if (s[n] == nullptr)
            s[n] = new TNode();
        return s[n];
    }
    int printit(FILE *fp, int k);
};


