// cloud_pinyin.h — 云拼音候选客户端（libcurl 实现）
//
// 背景：v0.9.174 之前 quick_input.h 里的 CloudPinyinClient 只是空壳
// （query() 直接 return {}，注释写着"实际实现需要 libcurl"），
// 测试也只能验到"离线降级返回空"。本文件是第一个真正发 HTTP 请求的实现。
//
// 协议（服务端只需实现这一个接口）：
//   POST <endpoint>
//   Content-Type: application/json
//   Authorization: Bearer <api_key>      （api_key 为空则不带该头）
//   {
//     "pinyin": "nihao",
//     "limit": 9,
//     "client": "pinyin-ime",
//     "version": "0.9.175"
//   }
//   ->
//   200 OK
//   {
//     "candidates": [
//       {"chinese": "你好", "pinyin": "ni hao", "confidence": 95},
//       ...
//     ]
//   }
//
// 设计约束：
//   * 输入法路径上不能阻塞：调用方应把 query() 放到后台线程，配合 cache 使用；
//   * 永远不因网络问题影响候选：任何失败都返回空 vector（离线降级）；
//   * 缓存只存拼音与候选词，不落任何语料上下文（安全纪律）。
#pragma once

#include <string>
#include <vector>

namespace pinyinime {

struct CloudCandidate {
    std::string chinese;
    std::string pinyin;
    int confidence = 0;   // 0-100
    std::string source;   // "cloud" / "cloud-cache"
};

struct CloudConfig {
    bool enabled = false;
    std::string endpoint;            // 完整 URL，含路径
    std::string api_key;             // Bearer token，可空
    int timeout_ms = 1500;           // 超时上限，输入法路径要短
    int connect_timeout_ms = 800;
    size_t max_candidates = 9;
    std::string cache_dir;           // 空则用 $XDG_CACHE_HOME/pinyin-ime/cloud
    long cache_ttl_s = 604800;       // 7 天
    bool use_cache = true;
};

class CloudPinyinClient {
public:
    explicit CloudPinyinClient(CloudConfig cfg = {});

    // 同步查询：命中缓存直接返回，否则发 HTTP 请求并写缓存。
    // 任何失败（超时/非 200/解析失败）都返回空 vector。
    std::vector<CloudCandidate> query(const std::string &pinyin);

    // 只读缓存，不发请求（输入法路径首选用这个，避免阻塞）
    std::vector<CloudCandidate> query_cache_only(const std::string &pinyin);

    // 把云候选合并进本地候选（去重、保持在尾部附近）
    static std::vector<std::string>
    merge(const std::vector<std::string> &local,
          const std::vector<CloudCandidate> &cloud,
          size_t max_total = 0);

    const CloudConfig &config() const { return cfg_; }
    // 最近一次失败原因（调试用，不含请求内容以外的敏感信息）
    const std::string &last_error() const { return last_error_; }

    // 生成默认缓存目录
    static std::string default_cache_dir();

private:
    CloudConfig cfg_;
    std::string last_error_;

    std::string cache_path(const std::string &pinyin) const;
    std::vector<CloudCandidate> load_cache(const std::string &pinyin) const;
    void save_cache(const std::string &pinyin,
                    const std::vector<CloudCandidate> &cands) const;
    bool http_post(const std::string &body, std::string &response);
};

// 极简 JSON 解析：只认 {"candidates":[{...}]} 这一种结构，
// 单层、无嵌套对象，键名固定。故意不引入 nlohmann 之类依赖。
std::vector<CloudCandidate> parse_cloud_response(const std::string &json,
                                                 size_t max_candidates);

}  // namespace pinyinime
