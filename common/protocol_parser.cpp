#include "common/protocol_parser.hpp"

#include <cctype>
#include <charconv>
#include <sstream>

namespace rssi_game::protocol {
namespace {

bool startsWith(const std::string& s, const char* prefix) {
    const std::size_t n = std::char_traits<char>::length(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

void trimInPlace(std::string& s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
}

bool parseInt(const std::string& tok, int& out) {
    const char* begin = tok.data();
    const char* end = tok.data() + tok.size();
    int v = 0;
    auto result = std::from_chars(begin, end, v);
    if (result.ec != std::errc{} || result.ptr != end) {
        return false;
    }
    out = v;
    return true;
}

bool parseDouble(const std::string& tok, double& out) {
    std::istringstream is(tok);
    is >> out;
    return !is.fail();
}

std::vector<std::pair<int, int>> parseCellList(const std::string& inner) {
    std::vector<std::pair<int, int>> out;
    std::string chunk = inner;
    std::size_t start = 0;
    while (start < chunk.size()) {
        std::size_t semi = chunk.find(';', start);
        std::string part = (semi == std::string::npos) ? chunk.substr(start) : chunk.substr(start, semi - start);
        auto comma = part.find(',');
        if (comma == std::string::npos) {
            break;
        }
        int x = 0;
        int y = 0;
        if (!parseInt(part.substr(0, comma), x) || !parseInt(part.substr(comma + 1), y)) {
            break;
        }
        out.push_back({x, y});
        if (semi == std::string::npos) {
            break;
        }
        start = semi + 1;
    }
    return out;
}

} // namespace

void parseServerLine(const std::string& line, ParsedServerLine& out) {
    out = ParsedServerLine{};

    if (startsWith(line, "ROLE ")) {
        out.kind = ServerLineKind::Role;
        out.role_raw = line.substr(std::string("ROLE ").size());
        trimInPlace(out.role_raw);
        return;
    }
    if (startsWith(line, "INVITE ")) {
        out.kind = ServerLineKind::Invite;
        InviteData d;
        d.code = line.substr(std::string("INVITE ").size());
        trimInPlace(d.code);
        out.invite = std::move(d);
        return;
    }
    if (startsWith(line, "GAME_START ")) {
        out.kind = ServerLineKind::GameStart;
        std::istringstream is(line.substr(std::string("GAME_START ").size()));
        GameStartData d;
        if (!(is >> d.grid_w >> d.grid_h >> d.placement_sec)) {
            out.kind = ServerLineKind::Unknown;
            return;
        }
        out.game_start = d;
        return;
    }
    if (startsWith(line, "STATE ")) {
        out.kind = ServerLineKind::State;
        StateData st;
        std::string rest = line.substr(std::string("STATE ").size());

        auto takeUntilSpace = [&rest]() -> std::string {
            auto sp = rest.find(' ');
            std::string tok = (sp == std::string::npos) ? rest : rest.substr(0, sp);
            if (sp != std::string::npos) {
                rest = rest.substr(sp + 1);
            } else {
                rest.clear();
            }
            return tok;
        };

        while (!rest.empty()) {
            std::string tok = takeUntilSpace();
            if (tok.rfind("turn=", 0) == 0) {
                auto slash = tok.find('/');
                if (slash != std::string::npos) {
                    int t = 0;
                    int m = 0;
                    if (parseInt(tok.substr(5, slash - 5), t) && parseInt(tok.substr(slash + 1), m)) {
                        st.turn = t;
                        st.max_turns = m;
                    }
                }
            } else if (tok.rfind("rxpos=", 0) == 0) {
                auto lt = tok.find('<');
                auto gt = tok.rfind('>');
                if (lt != std::string::npos && gt != std::string::npos && gt > lt) {
                    st.receiver_positions = parseCellList(tok.substr(lt + 1, gt - lt - 1));
                }
            } else if (tok.rfind("txest=", 0) == 0) {
                auto lt = tok.find('<');
                auto gt = tok.rfind('>');
                if (lt != std::string::npos && gt != std::string::npos && gt > lt) {
                    st.tx_estimated = parseCellList(tok.substr(lt + 1, gt - lt - 1));
                }
            } else if (tok.rfind("rssi=", 0) == 0) {
                auto lt = tok.find('<');
                auto gt = tok.rfind('>');
                if (lt == std::string::npos || gt == std::string::npos || gt <= lt) {
                    break;
                }
                std::string inner = tok.substr(lt + 1, gt - lt - 1);
                std::vector<std::vector<double>> rows;
                std::size_t pos = 0;
                while (pos <= inner.size()) {
                    std::size_t bar = inner.find('|', pos);
                    std::string row = (bar == std::string::npos) ? inner.substr(pos) : inner.substr(pos, bar - pos);
                    std::vector<double> vals;
                    std::size_t rp = 0;
                    while (rp < row.size()) {
                        std::size_t comma = row.find(',', rp);
                        std::string num = (comma == std::string::npos) ? row.substr(rp) : row.substr(rp, comma - rp);
                        double v = 0;
                        if (parseDouble(num, v)) {
                            vals.push_back(v);
                        }
                        if (comma == std::string::npos) {
                            break;
                        }
                        rp = comma + 1;
                    }
                    rows.push_back(std::move(vals));
                    if (bar == std::string::npos) {
                        break;
                    }
                    pos = bar + 1;
                }
                st.rssi = std::move(rows);
            }
        }
        out.state = std::move(st);
        return;
    }
    if (startsWith(line, "PLACEMENT_DONE ")) {
        out.kind = ServerLineKind::PlacementDone;
        PlacementDoneData d;
        d.tx_real_raw = line.substr(std::string("PLACEMENT_DONE ").size());
        trimInPlace(d.tx_real_raw);
        out.placement_done = std::move(d);
        return;
    }
    if (startsWith(line, "GAME_FINISHED ")) {
        out.kind = ServerLineKind::GameFinished;
        std::string rest = line.substr(std::string("GAME_FINISHED ").size());
        GameFinishedData d;
        auto posTxReal = rest.find("txReal=");
        auto posTxEst = rest.find("txEst=");
        auto posMean = rest.find("mean_error_cells=");
        if (posTxReal != std::string::npos) {
            std::string sub = rest.substr(posTxReal + std::string("txReal=").size());
            auto lt = sub.find('<');
            auto gt = sub.find('>');
            if (lt != std::string::npos && gt != std::string::npos && gt > lt) {
                d.tx_real_raw = sub.substr(lt, gt - lt + 1);
            }
        }
        if (posTxEst != std::string::npos) {
            std::string sub = rest.substr(posTxEst + std::string("txEst=").size());
            auto lt = sub.find('<');
            auto gt = sub.find('>');
            if (lt != std::string::npos && gt != std::string::npos && gt > lt) {
                d.tx_est_raw = sub.substr(lt, gt - lt + 1);
            }
        }
        if (posMean != std::string::npos) {
            std::string tail = rest.substr(posMean + std::string("mean_error_cells=").size());
            trimInPlace(tail);
            (void)parseDouble(tail, d.mean_error_cells);
        }
        out.game_finished = std::move(d);
        return;
    }
    if (startsWith(line, "ERROR ")) {
        out.kind = ServerLineKind::Error;
        ErrorData e;
        e.message = line.substr(std::string("ERROR ").size());
        trimInPlace(e.message);
        out.error = std::move(e);
        return;
    }
}

} // namespace rssi_game::protocol
