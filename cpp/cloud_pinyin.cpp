// cloud_pinyin.cpp — libcurl 实现的云拼音客户端
#include "cloud_pinyin.h"

#include <curl/curl.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace pinyinime {
namespace {

constexpr char kClientName[] = "pinyin-ime";
constexpr char kClientVersion[] = "0.9.175";

void ensure_dir(const std::string &path) {
    if (path.empty()) return;
    std::string cur;
    for (char c : path) {
        cur.push_back(c);
        if (c == '/') ::mkdir(cur.c_str(), 0700);
    }
    ::mkdir(path.c_str(), 0700);
}

std::string json_escape(const std::string &s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    return out;
}

// 读取紧随 key 之后的双引号字符串；失败返回 false。
bool read_json_string(const std::string &s, size_t pos, std::string &out,
                      size_t &end_pos) {
    // pos 指向开引号
    if (pos >= s.size() || s[pos] != '"') return false;
    out.clear();
    size_t i = pos + 1;
    while (i < s.size()) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size()) {
            char n = s[i + 1];
            switch (n) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'u': {
                // \uXXXX：只处理 ASCII 区间，其余原样跳过（中文一般直接是 UTF-8）
                if (i + 5 < s.size()) {
                    unsigned code = 0;
                    bool ok = true;
                    for (int k = 2; k <= 5; ++k) {
                        char h = s[i + k];
                        unsigned v;
                        if (h >= '0' && h <= '9') v = h - '0';
                        else if (h >= 'a' && h <= 'f') v = h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') v = h - 'A' + 10;
                        else { ok = false; break; }
                        code = code * 16 + v;
                    }
                    if (ok) {
                        if (code < 0x80) {
                            out.push_back(static_cast<char>(code));
                        } else if (code < 0x800) {
                            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        } else {
                            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                        }
                        i += 6;
                        continue;
                    }
                }
                out.push_back(n);
                break;
            }
            default: out.push_back(n);
            }
            i += 2;
            continue;
        }
        if (c == '"') {
            end_pos = i + 1;
            return true;
        }
        out.push_back(c);
        ++i;
    }
    return false;
}

size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *out = static_cast<std::string *>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

long now_s() {
    return static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count());
}

}  // namespace

// ---------------------------------------------------------------- JSON 解析

std::vector<CloudCandidate> parse_cloud_response(const std::string &json,
                                                 size_t max_candidates) {
    std::vector<CloudCandidate> out;
    size_t key = json.find("\"candidates\"");
    if (key == std::string::npos) return out;
    size_t arr = json.find('[', key);
    if (arr == std::string::npos) return out;

    size_t i = arr + 1;
    while (i < json.size() && out.size() < max_candidates) {
        size_t obj = json.find('{', i);
        if (obj == std::string::npos) break;
        size_t obj_end = json.find('}', obj);
        if (obj_end == std::string::npos) break;

        std::string block = json.substr(obj, obj_end - obj + 1);
        CloudCandidate c;

        for (const char *field : {"chinese", "pinyin"}) {
            size_t k = block.find(std::string("\"") + field + "\"");
            if (k == std::string::npos) continue;
            size_t colon = block.find(':', k);
            if (colon == std::string::npos) continue;
            size_t q = block.find_first_not_of(" \t\r\n", colon + 1);
            std::string val;
            size_t next = 0;
            if (q != std::string::npos && read_json_string(block, q, val, next)) {
                if (std::strcmp(field, "chinese") == 0) c.chinese = val;
                else c.pinyin = val;
            }
        }
        size_t ck = block.find("\"confidence\"");
        if (ck != std::string::npos) {
            size_t colon = block.find(':', ck);
            if (colon != std::string::npos) {
                c.confidence = std::atoi(block.c_str() + colon + 1);
                if (c.confidence < 0) c.confidence = 0;
                if (c.confidence > 100) c.confidence = 100;
            }
        }
        if (!c.chinese.empty()) {
            c.source = "cloud";
            out.push_back(std::move(c));
        }
        i = obj_end + 1;
    }
    return out;
}

// ---------------------------------------------------------------- client

CloudPinyinClient::CloudPinyinClient(CloudConfig cfg) : cfg_(std::move(cfg)) {
    if (cfg_.cache_dir.empty()) cfg_.cache_dir = default_cache_dir();
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

std::string CloudPinyinClient::default_cache_dir() {
    const char *xdg = std::getenv("XDG_CACHE_HOME");
    const char *home = std::getenv("HOME");
    std::string base;
    if (xdg && *xdg) base = xdg;
    else if (home && *home) base = std::string(home) + "/.cache";
    else base = "/tmp";
    return base + "/pinyin-ime/cloud";
}

std::string CloudPinyinClient::cache_path(const std::string &pinyin) const {
    if (!cfg_.use_cache || cfg_.cache_dir.empty()) return {};
    // 文件名只用 [a-z0-9] ，避免拼音里的引号/斜杠落到路径上
    std::string safe;
    safe.reserve(pinyin.size());
    for (char c : pinyin) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) safe.push_back(c);
        else safe.push_back('_');
    }
    return cfg_.cache_dir + "/" + safe + ".cache";
}

std::vector<CloudCandidate>
CloudPinyinClient::load_cache(const std::string &pinyin) const {
    std::vector<CloudCandidate> out;
    std::string path = cache_path(pinyin);
    if (path.empty()) return out;
    std::ifstream in(path);
    if (!in) return out;

    long ts = 0;
    std::string line;
    if (!std::getline(in, line)) return out;
    // 第一行：#<unix ts>
    if (line.size() > 1 && line[0] == '#') ts = std::atol(line.c_str() + 1);
    if (cfg_.cache_ttl_s > 0 && now_s() - ts > cfg_.cache_ttl_s) return out;

    while (std::getline(in, line)) {
        // confidence \t pinyin \t chinese
        size_t t1 = line.find('\t');
        if (t1 == std::string::npos) continue;
        size_t t2 = line.find('\t', t1 + 1);
        if (t2 == std::string::npos) continue;
        CloudCandidate c;
        c.confidence = std::atoi(line.c_str());
        c.pinyin = line.substr(t1 + 1, t2 - t1 - 1);
        c.chinese = line.substr(t2 + 1);
        c.source = "cloud-cache";
        if (!c.chinese.empty()) out.push_back(std::move(c));
        if (out.size() >= cfg_.max_candidates) break;
    }
    return out;
}

void CloudPinyinClient::save_cache(const std::string &pinyin,
                                   const std::vector<CloudCandidate> &cands) const {
    std::string path = cache_path(pinyin);
    if (path.empty() || cands.empty()) return;
    ensure_dir(cfg_.cache_dir);
    std::ofstream out(path, std::ios::trunc);
    if (!out) return;
    out << '#' << now_s() << '\n';
    for (const auto &c : cands) {
        out << c.confidence << '\t' << c.pinyin << '\t' << c.chinese << '\n';
    }
}

bool CloudPinyinClient::http_post(const std::string &body, std::string &response) {
    CURL *h = curl_easy_init();
    if (!h) {
        last_error_ = "curl_easy_init failed";
        return false;
    }

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    std::string auth;
    if (!cfg_.api_key.empty()) {
        auth = "Authorization: Bearer " + cfg_.api_key;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(h, CURLOPT_URL, cfg_.endpoint.c_str());
    curl_easy_setopt(h, CURLOPT_POST, 1L);
    curl_easy_setopt(h, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(h, CURLOPT_TIMEOUT_MS, static_cast<long>(cfg_.timeout_ms));
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS,
                     static_cast<long>(cfg_.connect_timeout_ms));
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_USERAGENT, "pinyin-ime/0.9.175 (+fcitx5)");
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);

    CURLcode rc = curl_easy_perform(h);
    long code = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &code);

    bool ok = false;
    if (rc != CURLE_OK) {
        last_error_ = std::string("curl: ") + curl_easy_strerror(rc);
    } else if (code != 200) {
        last_error_ = "http status " + std::to_string(code);
    } else {
        last_error_.clear();
        ok = true;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(h);
    return ok;
}

std::vector<CloudCandidate> CloudPinyinClient::query_cache_only(
    const std::string &pinyin) {
    if (!cfg_.enabled || pinyin.empty()) return {};
    return load_cache(pinyin);
}

std::vector<CloudCandidate> CloudPinyinClient::query(const std::string &pinyin) {
    if (!cfg_.enabled || pinyin.empty()) return {};
    if (cfg_.endpoint.empty()) {
        last_error_ = "endpoint empty";
        return {};
    }

    auto cached = load_cache(pinyin);
    if (!cached.empty()) return cached;

    std::ostringstream body;
    body << "{\"pinyin\":\"" << json_escape(pinyin) << "\",\"limit\":"
         << cfg_.max_candidates << ",\"client\":\"" << kClientName
         << "\",\"version\":\"" << kClientVersion << "\"}";

    std::string resp;
    if (!http_post(body.str(), resp)) return {};

    auto cands = parse_cloud_response(resp, cfg_.max_candidates);
    if (!cands.empty()) save_cache(pinyin, cands);
    return cands;
}

std::vector<std::string> CloudPinyinClient::merge(
    const std::vector<std::string> &local,
    const std::vector<CloudCandidate> &cloud, size_t max_total) {
    std::vector<std::string> out = local;
    for (const auto &c : cloud) {
        if (max_total && out.size() >= max_total) break;
        bool dup = false;
        for (const auto &r : out) {
            if (r == c.chinese) { dup = true; break; }
        }
        if (!dup) out.push_back(c.chinese);
    }
    return out;
}

}  // namespace pinyinime
