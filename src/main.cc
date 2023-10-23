#include <iomanip>
#include <iostream>
using namespace std;

class Cache {
  private:
    bool lru;
    bool wbwa;
    int block;
    int assoc;
    int sets;
    int read_count = 0;
    int read_miss = 0;
    int write_count = 0;
    int write_miss = 0;
    int write_back = 0;
    struct CacheLine {
        bool dirty = false;
        bool valid = false;
        int count = 0;
        int tag = 0;
    } **cache;

  public:
    Cache(bool lru, bool wbwa, int block, int assoc, int sets)
        : lru(lru), wbwa(wbwa), block(block), assoc(assoc), sets(sets),
          cache(new CacheLine *[sets]) {
        for (int i = 0; i < sets; i++) { cache[i] = new CacheLine[assoc]; }
    }

    ~Cache() {
        for (int i = 0; i < sets; i++) { delete[] cache[i]; }
        delete[] cache;
    }

    void updateLineCount(int set, int index) {
        if (!lru) {
            cache[set][index].count++;
            return;
        }
        if (cache[set][index].valid) {
            int count = cache[set][index].count;
            for (int i = 0; i < assoc; i++) {
                if (cache[set][i].count < count) { cache[set][i].count++; }
            }
        } else {
            for (int i = 0; i < assoc; i++) { cache[set][i].count++; }
        }
        cache[set][index].count = 0;
    }

    int replace(int set) {
        if (lru) {
            for (int i = 0; i < assoc; i++) {
                if (cache[set][i].count == assoc - 1) { return i; }
            }
        }
        int index = 0;
        int min_count = cache[set][0].count;
        for (int i = 0; i < assoc; i++) {
            if (cache[set][i].count < min_count) {
                index = i;
                min_count = cache[set][i].count;
            }
        }
        return index;
    }

    void outputResult() {
        double miss_rate =
            double(read_miss + write_miss) / (read_count + write_count);
        double ht_l1 = 0.25 + 2.5 * (sets * block * assoc / (512.0 * 1024)) +
                       0.025 * (block / 16.0) + 0.025 * (assoc);
        double miss_pen_l1 = 20 + 0.5 * (block / 16.0);
        cout << fixed << setprecision(4) << endl
             << "===== L1 contents =====" << endl;
        for (int i = 0; i < sets; i++) {
            cout << "set" << setw(4) << i << ":";
            cout << hex;
            for (int j = 0; j < assoc; j++) {
                if (cache[i][j].valid) {
                    cout << setw(8) << cache[i][j].tag
                         << (cache[i][j].dirty ? " D" : "  ");
                } else {
                    cout << "    -     ";
                }
            }
            cout << dec << endl;
        }
        cout << endl
             << "  ====== Simulation results (raw) ======" << endl
             << "  a. number of L1 reads:" << setw(16) << read_count << endl
             << "  b. number of L1 read misses:" << setw(10) << read_miss
             << endl
             << "  c. number of L1 writes:" << setw(15) << write_count << endl
             << "  d. number of L1 write misses:" << setw(9) << write_miss
             << endl
             << "  e. L1 miss rate:" << setw(22) << miss_rate << endl
             << "  f. number of writebacks from L1:" << setw(6) << write_back
             << endl
             << "  g. total memory traffic:" << setw(14)
             << (wbwa ? read_miss + write_miss + write_back :
                        read_miss + write_count)
             << endl
             << endl
             << "  ==== Simulation results (performance) ====" << endl
             << "  1. average access time:         "
             << ht_l1 + miss_rate * miss_pen_l1 << " ns";
    }

    void readFromAddress(unsigned int addr) {
        int blk = addr / block;
        int tag = blk / sets;
        int set = blk % sets;
        read_count++;
        for (int i = 0; i < assoc; i++) {
            if (cache[set][i].tag == tag && cache[set][i].valid) {
                updateLineCount(set, i);
                return;
            }
        }
        read_miss++;
        for (int i = 0; i < assoc; i++) {
            if (!cache[set][i].valid) {
                updateLineCount(set, i);
                cache[set][i].tag = tag;
                cache[set][i].valid = true;
                cache[set][i].dirty = false;
                return;
            }
        }
        int index = replace(set);
        if (cache[set][index].dirty) { write_back++; }
        updateLineCount(set, index);
        cache[set][index].tag = tag;
        cache[set][index].dirty = false;
    }

    void writeToAddress(unsigned int addr) {
        int blk = addr / block;
        int tag = blk / sets;
        int set = blk % sets;
        write_count++;
        for (int i = 0; i < assoc; i++) {
            if (cache[set][i].tag == tag && cache[set][i].valid) {
                updateLineCount(set, i);
                if (wbwa) { cache[set][i].dirty = true; }
                return;
            }
        }
        write_miss++;
        if (!wbwa) { return; }
        for (int i = 0; i < assoc; i++) {
            if (!cache[set][i].valid) {
                updateLineCount(set, i);
                cache[set][i].tag = tag;
                cache[set][i].valid = true;
                cache[set][i].dirty = true;
                return;
            }
        }
        int index = replace(set);
        if (cache[set][index].dirty) { write_back++; }
        updateLineCount(set, index);
        cache[set][index].tag = tag;
        cache[set][index].dirty = true;
    }
};

int main(int argc, char *argv[]) {
    if (argc != 7) {
        cout << "Usage: " << argv[0] << " <L1_BLOCKSIZE> <L1_SIZE> <L1_ASSOC>"
             << " <L1_REPLACEMENT_POLICY> <L1_WRITE_POLICY> <trace_file>"
             << endl;
        return 1;
    }
    cout << "  ===== Simulator configuration =====" << endl
         << "  L1_BLOCKSIZE:" << setw(22) << argv[1] << endl
         << "  L1_SIZE:" << setw(27) << argv[2] << endl
         << "  L1_ASSOC:" << setw(26) << argv[3] << endl
         << "  L1_REPLACEMENT_POLICY:" << setw(13) << argv[4] << endl
         << "  L1_WRITE_POLICY:" << setw(19) << argv[5] << endl
         << "  trace_file:" << setw(24) << argv[6] << endl
         << "  ===================================" << endl;

    Cache cache(!atoi(argv[4]), !atoi(argv[5]), atoi(argv[1]), atoi(argv[3]),
                atoi(argv[2]) / atoi(argv[1]) / atoi(argv[3]));
    FILE *fp = fopen(argv[6], "r");
    if (fp == NULL) {
        perror(argv[6]);
        return 1;
    }
    char line[12];
    while (fgets(line, 12, fp) != NULL) {
        unsigned int addr = strtol(line + 2, NULL, 16);
        (line[0] == 'r') ? cache.readFromAddress(addr) :
                           cache.writeToAddress(addr);
    }
    cache.outputResult();
    return 0;
}
