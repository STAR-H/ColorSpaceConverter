#ifndef INPUTCMDPARSER_H
#define INPUTCMDPARSER_H

#include <vector>
#include <string>
#include <algorithm>

class InputCmdParser {
public:
    InputCmdParser(int& argc, const char** argv) {
        for (int i = 1; i < argc; ++i) {
            m_tokens.emplace_back(argv[i]);
        }
    }

    const std::string GetCmdOption(const std::string& option) const {
        auto it = std::find(m_tokens.begin(), m_tokens.end(), option);
        if (it != m_tokens.end() && ++it != m_tokens.end()) {
            return *it;
        }
        return "";
    }

    bool CmdOptionExists(const std::string& option) const {
        return std::find(m_tokens.begin(), m_tokens.end(), option) != m_tokens.end();
    }

private:
    std::vector<std::string> m_tokens;
};

#endif // INPUTCMDPARSER_H
