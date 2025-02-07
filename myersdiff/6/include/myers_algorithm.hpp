// myers_algorithm.hpp
#pragma once
template<typename T>
class MyersAlgorithm {
public:
    static void calculateDiff(
        const std::deque<T>& seq1,
        const std::deque<T>& seq2,
        DiffState& state,
        bool debug = false)
    {
        const int N = seq1.size();
        const int M = seq2.size();
        const int MAX_D = std::max(N, M);
        
        state.resetVectors();
        state.clearSnakes();
        
        if (debug) {
            DiffTrace::log("Starting Myers algorithm:");
            DiffTrace::log("  Sequence 1 [" + std::to_string(N) + "]: " + dumpSequence(seq1));
            DiffTrace::log("  Sequence 2 [" + std::to_string(M) + "]: " + dumpSequence(seq2));
        }
        
        // Special case: one sequence empty
        if (N == 0 || M == 0) {
            if (debug) DiffTrace::log("Special case: empty sequence");
            return;
        }
        
        for (int d = 0; d <= MAX_D; d++) {
            if (debug) DiffTrace::log("Processing d = " + std::to_string(d));
            
            for (int k = -d; k <= d; k += 2) {
                processK(k, d, seq1, seq2, state, debug);
            }
        }
    }

private:
    template<typename Container>
    static std::string dumpSequence(const Container& seq) {
        std::stringstream ss;
        for (const auto& elem : seq) ss << elem << " ";
        return ss.str();
    }
    
    static void processK(int k, int d, 
        const std::deque<T>& seq1,
        const std::deque<T>& seq2,
        DiffState& state,
        bool debug)
    {
        const int MAX_D = std::max(seq1.size(), seq2.size());
        
        // Choose the furthest reaching path
        int x = (k == -d || (k != d && 
            state[k - 1 + MAX_D] < state[k + 1 + MAX_D])) 
            ? state[k + 1 + MAX_D] 
            : state[k - 1 + MAX_D] + 1;
            
        int y = x - k;
        
        if (debug) {
            DiffTrace::log("  k=" + std::to_string(k) + 
                          " initial (x,y)=(" + std::to_string(x) + "," + 
                          std::to_string(y) + ")");
        }
        
        // Record the starting point
        int start_x = x;
        int start_y = y;
        
        // Follow diagonal
        while (x < seq1.size() && y < seq2.size() && 
               seq1[x] == seq2[y]) {
            x++;
            y++;
        }
        
        if (debug && x > start_x) {
            DiffTrace::log("    Found snake from (" + 
                          std::to_string(start_x) + "," + 
                          std::to_string(start_y) + ") len=" + 
                          std::to_string(x - start_x));
        }
        
        // Save endpoint
        state[k + MAX_D] = x;
        
        // Record snake if we found one
        if (x > start_x) {
            state.addSnake(start_x, start_y, x - start_x);
        }
    }
};

