
#include "node.h"

TNode *gnode = NULL;

int TNode::printit(FILE *fp, int k) {
    if (s.size()) {
        using tt = std::tuple<int, std::string, TNode *>;
        std::vector<tt> xx;
        for (auto x : s) xx.push_back(make_tuple(x.second->c, x.first, x.second));
        std::sort(begin(xx), end(xx), std::greater<tt>());
        for (auto x : xx) {
            auto count = std::get<0>(x);
            if (100.0 * count / c < 1)
                continue;
            auto name = std::get<1>(x);
            auto nx = std::get<2>(x);
            fprintf(fp, "<li>\n");
            fprintf(fp, "<input type=\"checkbox\" id=\"c%d\" />\n", k);
            fprintf(fp,
                    "<label class=\"tree_label\" for=\"c%d\">%s(%.3f%% "
                    "%d/%d)</label>\n",
                    k,
                    name.c_str(),
                    100.0 * count / c,
                    count,
                    c);
            fprintf(fp, "<ul>\n");
            // printf("%s(%.3f%% %d/%d)\n", name.c_str(), 100.0*count/c, count, c);
            k = nx->printit(fp, k + 1);
            fprintf(fp, "</ul>\n");
            fprintf(fp, "</li>\n");
        }
    }
    return k;
}
