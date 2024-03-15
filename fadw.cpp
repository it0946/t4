#include <cstdio>
#include <cctype>

#include <unordered_set>
#include <string_view>
#include <stdexcept>
#include <iostream>

struct filebuf {
    filebuf(const char * fp) {
        FILE * f = fopen(fp, "rb");
        if (!f) {
            throw std::runtime_error{"failed to open file"};
        }

        fseek(f, 0l, SEEK_END);
        size = ftell(f);
        fseek(f, 0l, SEEK_SET);

        buf = new char[size];

        size_t res = fread(buf, 1, size, f);
        if (res == 0) {
            throw std::runtime_error{"failed to read file"};
        }

        fclose(f);
    }
    
    ~filebuf() {
        delete[] buf;
    }

    char * buf;
    size_t size;
};

int main(int argc, const char * argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [filename]", argv[0]);
    }

    filebuf inf{argv[1]};
    filebuf enf{"sorted.bin"};

    std::unordered_set<std::string_view> eng;
    eng.reserve(200'000);

    {
        const char * start = enf.buf;
        for (size_t i = 0; i < enf.size; i++) {
            const char * c = enf.buf + i;
            if (*c == '\0') {
                size_t length = c - start;
                if (length != 0) {
                    std::string_view v{start, length};
                    eng.insert(v);
                }                
                start = c + 1;
            }
        }
    }

    std::unordered_set<std::string_view> in;
    in.reserve(10'000);
    
    uint64_t non_english = 0;
    uint64_t num_unique = 0;
    uint64_t num_total = 0;

    {
        char * start = inf.buf;
        for (size_t i = 0; i < inf.size; i++) {
            char * c = inf.buf + i;
            if (!isalpha(*c)) {
                size_t length = c - start;
                if (length != 0) {
                    std::string_view v{start, length};
                    if (in.insert(v).second) {
                        num_unique += 1;
                        
                        auto it = eng.find(v);
                        if (it == eng.end()) {
                            std::cout << v << "\n";
                            non_english += 1;
                        }
                    }
                    num_total += 1;
                }
                start = c + 1;
            } else {
                *c = tolower(*c);
            }
        }
    }

    std::cout << "\nTotal words: " << num_total << "\n";
    std::cout << "Unique words: " << num_unique << "\n";
    std::cout << "Number of non-english: " << non_english << "\n";

    return 0;
}