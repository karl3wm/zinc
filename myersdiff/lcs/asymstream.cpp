#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

class LCSStreamer {
public:
    // Constructor initializes the LCS streamer with the offline string and epsilon value.
    LCSStreamer(const std::string& offlineString, double epsilon)
        : offlineString(offlineString), epsilon(epsilon),
          windowSize(static_cast<int>(std::sqrt(offlineString.size()))) {
        currentWindow = 0;
        windowLCS.resize(windowSize, std::vector<int>(offlineString.size() + 1, 0));
    }

    // Processes each character of the online string.
    void processCharacter(char c) {
        if (currentWindow >= windowSize) {
            updateLCS();
            currentWindow = 0;
        }

        for (int j = 1; j <= static_cast<int>(offlineString.size()); ++j) {
            if (offlineString[j - 1] == c) {
                windowLCS[currentWindow][j] = previousLCS[j - 1] + 1;
            } else {
                windowLCS[currentWindow][j] = std::max(previousLCS[j], windowLCS[currentWindow][j - 1]);
            }
        }
        currentWindow++;
    }

    // Returns the approximate length of the LCS.
    int getLCSLength() {
        updateLCS();
        return overallLCS;
    }

private:
    std::string offlineString;
    double epsilon;
    int windowSize;
    int currentWindow;
    std::vector<std::vector<int>> windowLCS;
    std::vector<int> previousLCS;
    int overallLCS = 0;

    // Updates the overall LCS length and resets the window for the next set of characters.
    void updateLCS() {
        int maxLCS = 0;
        for (int j = 0; j <= static_cast<int>(offlineString.size()); ++j) {
            maxLCS = std::max(maxLCS, windowLCS[windowSize - 1][j]);
        }
        overallLCS += maxLCS;
        previousLCS = windowLCS[windowSize - 1];
        windowLCS.clear();
        windowLCS.resize(windowSize, std::vector<int>(offlineString.size() + 1, 0));
    }
};

int main() {
    std::string offlineString = "AGGTAB";
    double epsilon = 0.1;
    LCSStreamer lcsStreamer(offlineString, epsilon);

    std::string onlineString = "GXTXAYB";
    for (char c : onlineString) {
        lcsStreamer.processCharacter(c);
    }

    std::cout << "Approximate LCS Length: " << lcsStreamer.getLCSLength() << std::endl;
    return 0;
}

