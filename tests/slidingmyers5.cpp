#include <vector>
#include <string>
#include <deque>
#include <memory>
#include <algorithm>
#include <cassert>
#include <sstream>
#include <iostream>
#include <iomanip>

// Debug trace class to capture algorithm state
class DiffTrace {
public:
    static bool enabled;
    static std::stringstream trace;
    
    static void clear() {
        trace.str("");
        trace.clear();
    }
    
    template<typename T>
    static void printSequences(const std::deque<T>& seq1, const std::deque<T>& seq2) {
        if (!enabled) return;
        trace << "\nSequences state:\n";
        trace << "Seq1 [" << seq1.size() << "]: ";
        for (const auto& elem : seq1) trace << elem << " ";
        trace << "\nSeq2 [" << seq2.size() << "]: ";
        for (const auto& elem : seq2) trace << elem << " ";
        trace << "\n";
    }
    
    static void printVector(const std::vector<int>& v, const std::string& name) {
        if (!enabled) return;
        trace << name << " [" << v.size() << "]: ";
        for (int x : v) trace << std::setw(3) << x << " ";
        trace << "\n";
    }
    
    static void log(const std::string& msg) {
        if (!enabled) return;
        trace << msg << "\n";
    }
};

bool DiffTrace::enabled = true;
std::stringstream DiffTrace::trace;

template<typename T>
class SlidingMyersDiff {
private:
    // === CLASS INVARIANTS ===
    // 1. sequence1_.size() <= window_size_
    // 2. sequence2_.size() <= window_size_
    // 3. lcs_path_ represents valid diff state whenever either sequence has content
    // 4. prev_endpoints_ size matches forward_vectors_ size when both are non-empty
    // 5. Diff is always maintained for current sequence state, regardless of window fullness

    struct Snake {
        int x, y, len;
        Snake(int x = 0, int y = 0, int len = 0) : x(x), y(y), len(len) {}
    };

    size_t window_size_;     // Must be > 0
    std::deque<T> sequence1_; // Size <= window_size_
    std::deque<T> sequence2_; // Size <= window_size_
    std::vector<int> forward_vectors_; // Size = 2 * (max_size + 1) + 1 when diff is calculated
    std::vector<Snake> lcs_path_; // Contains valid snake paths after diff calculation
    std::vector<int> prev_endpoints_; // Size matches forward_vectors_ when both non-empty

    // === METHOD CONTRACTS ===

    /**
     * @brief Calculates diff between current sequences
     * @pre At least one sequence has content
     * @post forward_vectors_ contains valid endpoint data
     * @post lcs_path_ contains valid snake paths
     * @post Size of forward_vectors_ is 2 * (max_size + 1) + 1 where max_size is max(N,M)
     */
    void calculateDiff() {
        DiffTrace::log("\n=== Starting calculateDiff() ===");
        DiffTrace::printSequences(sequence1_, sequence2_);
        
        assert((!sequence1_.empty() || !sequence2_.empty()) && "At least one sequence must have content");
        
        const int N = sequence1_.size();
        const int M = sequence2_.size();
        const int MAX_D = std::max(N, M);  // Changed from N + M to handle partial sequences better
        
        forward_vectors_.resize(2 * MAX_D + 1, 0);
        lcs_path_.clear();
        
        DiffTrace::log("Initial state:");
        DiffTrace::printVector(forward_vectors_, "forward_vectors");
        
        // Special case: one sequence empty
        if (N == 0 || M == 0) {
            // All elements will be insertions or deletions
            DiffTrace::log("Special case: one sequence empty");
            return;
        }
        
        // Normal Myers diff algorithm
        for (int d = 0; d <= MAX_D; d++) {
            DiffTrace::log("\nProcessing d = " + std::to_string(d));
            
            for (int k = -d; k <= d; k += 2) {
                std::stringstream ss;
                ss << "  Processing k = " << k << ": ";
                
                int x = (k == -d || (k != d && 
                    forward_vectors_[k - 1 + MAX_D] < forward_vectors_[k + 1 + MAX_D])) 
                    ? forward_vectors_[k + 1 + MAX_D] 
                    : forward_vectors_[k - 1 + MAX_D] + 1;
                
                int y = x - k;
                
                // Bounds check for partial sequences
                if (y < 0) y = 0;
                if (x < 0) x = 0;
                
                ss << "Initial (x,y) = (" << x << "," << y << ")";
                
                int start_x = x;
                int start_y = y;
                
                // Follow diagonal while in bounds and matching
                while (x < N && y < M && sequence1_[x] == sequence2_[y]) {
                    x++;
                    y++;
                }
                
                ss << ", Final (x,y) = (" << x << "," << y << ")";
                DiffTrace::log(ss.str());
                
                forward_vectors_[k + MAX_D] = x;
                
                if (x - start_x > 0) {
                    lcs_path_.emplace_back(start_x, start_y, x - start_x);
                    DiffTrace::log("    Added snake: (" + std::to_string(start_x) + "," + 
                                 std::to_string(start_y) + ") len=" + std::to_string(x - start_x));
                }
                
                DiffTrace::printVector(forward_vectors_, "    forward_vectors");
                
                if (x >= N && y >= M) {
                    DiffTrace::log("Reached end of both sequences");
                    return;
                }
            }
        }
    }

    /**
     * @brief Updates diff state when sequences change
     * @pre At least one sequence has content
     * @post forward_vectors_ and lcs_path_ represent current diff state
     * @post prev_endpoints_ matches forward_vectors_ if window is full
     */
    void updateDiffIncremental() {
        DiffTrace::log("\n=== Starting updateDiffIncremental() ===");
        
        // Always calculate if sequences aren't full
        if (sequence1_.size() != window_size_ || sequence2_.size() != window_size_) {
            DiffTrace::log("Partial sequences - calculating diff");
            calculateDiff();
            return;
        }
        
        // First time calculation needs to be done regardless
        if (prev_endpoints_.empty()) {
            DiffTrace::log("Initial calculation needed");
            calculateDiff();
            prev_endpoints_ = forward_vectors_;
            return;
        }
        
        // Force recalc after window slide to ensure accuracy
        if (sequence1_.size() == window_size_ && sequence2_.size() == window_size_) {
            DiffTrace::log("Window full after slide - recalculating");
            calculateDiff();
            prev_endpoints_ = forward_vectors_;
            return;
        }
    }

public:
    /**
     * @brief Constructs a new SlidingMyersDiff
     * @param window_size Must be > 0
     * @post All internal structures are empty
     */
    explicit SlidingMyersDiff(size_t window_size) 
        : window_size_(window_size) {
        assert(window_size > 0 && "Window size must be positive");
        DiffTrace::log("Created SlidingMyersDiff with window size " + std::to_string(window_size));
    }

    /**
     * @brief Adds element to first sequence
     * @param elem Element to add
     * @post sequence1_.size() <= window_size_
     * @post If both sequences at window_size_, diff is updated
     */
    void addElement1(const T& elem) {
        DiffTrace::log("\nAdding element to sequence1: " + std::to_string(elem));
        
        if (sequence1_.size() >= window_size_) {
            DiffTrace::log("Removing oldest element from sequence1: " + std::to_string(sequence1_.front()));
            sequence1_.pop_front();
        }
        sequence1_.push_back(elem);
        
        DiffTrace::printSequences(sequence1_, sequence2_);
        
        updateDiffIncremental();
        
        assert(sequence1_.size() <= window_size_ && "Sequence1 exceeded window size");
    }

    /**
     * @brief Adds element to second sequence
     * @param elem Element to add
     * @post sequence2_.size() <= window_size_
     * @post If both sequences at window_size_, diff is updated
     */
    void addElement2(const T& elem) {
        DiffTrace::log("\nAdding element to sequence2: " + std::to_string(elem));
        
        if (sequence2_.size() >= window_size_) {
            DiffTrace::log("Removing oldest element from sequence2: " + std::to_string(sequence2_.front()));
            sequence2_.pop_front();
        }
        sequence2_.push_back(elem);
        
        DiffTrace::printSequences(sequence1_, sequence2_);
        
        updateDiffIncremental();
        
        assert(sequence2_.size() <= window_size_ && "Sequence2 exceeded window size");
    }

    /**
     * @brief Returns the longest common subsequence (LCS) of the two sequences
     * @return Vector of pairs representing the LCS
     */
    std::vector<std::pair<size_t, size_t>> getLCS() const {
        DiffTrace::log("\n=== Getting LCS ===");
        std::vector<std::pair<size_t, size_t>> result;
        for (const auto& snake : lcs_path_) {
            for (int i = 0; i < snake.len; ++i) {
                result.emplace_back(snake.x + i, snake.y + i);
                DiffTrace::log("LCS pair: (" + std::to_string(snake.x + i) + "," + 
                             std::to_string(snake.y + i) + ")");
            }
        }
        return result;
    }

    /**
     * @brief Returns the diff between the two sequences
     * @return Vector of pairs representing the diff
     */
    std::vector<std::pair<char, T>> getDiff() const {
        DiffTrace::log("\n=== Getting Diff ===");
        std::vector<std::pair<char, T>> result;
        
        if (sequence1_.empty() && sequence2_.empty()) {
            return result;
        }
        
        // Special case: one sequence empty
        if (sequence1_.empty()) {
            for (const auto& elem : sequence2_) {
                result.emplace_back('+', elem);
            }
            return result;
        }
        if (sequence2_.empty()) {
            for (const auto& elem : sequence1_) {
                result.emplace_back('-', elem);
            }
            return result;
        }
        
        size_t x = 0, y = 0;
        size_t snake_idx = 0;
        
        while (x < sequence1_.size() || y < sequence2_.size()) {
            // If we have more snakes to process
            if (snake_idx < lcs_path_.size()) {
                const auto& snake = lcs_path_[snake_idx];
                
                // Process differences up to the start of this snake
                while (x < snake.x || y < snake.y) {
                    if (x < snake.x && (y >= snake.y || forward_vectors_[x - y + sequence1_.size()] < x)) {
                        result.emplace_back('-', sequence1_[x++]);
                    } else {
                        result.emplace_back('+', sequence2_[y++]);
                    }
                }
                
                // Process the matching section (snake)
                for (int i = 0; i < snake.len; ++i) {
                    result.emplace_back('=', sequence1_[x]);
                    x++;
                    y++;
                }
                
                snake_idx++;
            } else {
                // Process remaining differences after last snake
                if (x < sequence1_.size()) {
                    result.emplace_back('-', sequence1_[x++]);
                } else if (y < sequence2_.size()) {
                    result.emplace_back('+', sequence2_[y++]);
                }
            }
        }
        
        return result;
    }

    /**
     * @brief Clears all internal data structures
     * @post All internal structures are empty
     */
    void clear() {
        DiffTrace::log("\n=== Clearing all data structures ===");
        sequence1_.clear();
        sequence2_.clear();
        forward_vectors_.clear();
        lcs_path_.clear();
        prev_endpoints_.clear();
    }

    // Helper method to get the trace
    static std::string getTrace() {
        return DiffTrace::trace.str();
    }
    
    static void clearTrace() {
        DiffTrace::clear();
    }
    
    static void enableTracing(bool enable) {
        DiffTrace::enabled = enable;
    }
};

// Test suite
void runTests() {
    // Test 1: Basic sliding window functionality
    {
        std::cout << "\n=== Test 1: Basic sliding window functionality ===" << std::endl;
        SlidingMyersDiff<char>::clearTrace();
        
        SlidingMyersDiff<char> diff(3);
        diff.addElement1('A');
        diff.addElement1('B');
        diff.addElement1('C');
        diff.addElement2('A');
        diff.addElement2('D');
        diff.addElement2('C');
        
        auto result = diff.getDiff();
        std::cout << SlidingMyersDiff<char>::getTrace() << std::endl;
        
        assert(result.size() == 4);
        assert(result[0] == std::make_pair('=', 'A'));
        assert(result[1] == std::make_pair('-', 'B'));
        assert(result[2] == std::make_pair('+', 'D'));
        assert(result[3] == std::make_pair('=', 'C'));
    }

    // Test 2: Sliding window update
    {
        std::cout << "\n=== Test 2: Sliding window update ===" << std::endl;
        SlidingMyersDiff<char>::clearTrace();
        
        SlidingMyersDiff<char> diff(3);
        diff.addElement1('A');
        diff.addElement1('B');
        diff.addElement1('C');
        diff.addElement2('A');
        diff.addElement2('B');
        diff.addElement2('C');
        
        diff.addElement1('D');
        diff.addElement2('D');
        
        auto result = diff.getDiff();
        std::cout << SlidingMyersDiff<char>::getTrace() << std::endl;
        
        assert(result.size() == 3);
        assert(result[0] == std::make_pair('=', 'B'));
        assert(result[1] == std::make_pair('=', 'C'));
        assert(result[2] == std::make_pair('=', 'D'));
    }

    // Test 3: Empty sequences
    {
        std::cout << "\n=== Test 3: Empty sequences ===" << std::endl;
        SlidingMyersDiff<char>::clearTrace();
        
        SlidingMyersDiff<char> diff(3);
        auto result = diff.getDiff();
        std::cout << SlidingMyersDiff<char>::getTrace() << std::endl;
        
        assert(result.empty());
    }

    // Test 4: Different sequence lengths
    {
        std::cout << "\n=== Test 4: Different sequence lengths ===" << std::endl;
        SlidingMyersDiff<char>::clearTrace();
        
        SlidingMyersDiff<char> diff(4);
        diff.addElement1('A');
        diff.addElement1('B');
        diff.addElement2('A');
        
        auto result = diff.getDiff();
        std::cout << SlidingMyersDiff<char>::getTrace() << std::endl;
        
        assert(result.size() == 2);
        assert(result[0] == std::make_pair('=', 'A'));
        assert(result[1] == std::make_pair('-', 'B'));
    }
}

int main() {
    runTests();
    return 0;
}
