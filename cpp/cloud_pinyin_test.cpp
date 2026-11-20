// cloud_pinyin_test.cpp — 云拼音客户端自检
//
// 离线用例（不需要网络）：
//   1. parse_cloud_response：正常响应、缺字段、坏 JSON、limit 截断
//   2. 缓存写读 + TTL 过期
//   3. merge 去重
// 联网用例（给了 endpoint 才跑）：
//   ./cloud_pinyin_test http://127.0.0.1:8080/pinyin [pinyin]
#include "cloud_pinyin.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

using namespace pinyinime;

static int g_pass = 0, g_fail = 0;

static void check(const char *name, bool ok) {
    if (ok) {
        ++g_pass;
        std::printf("  ok   %s\n", name);
    } else {
        ++g_fail;
        std::printf("  FAIL %s\n", name);
    }
}

int main(int argc, char **argv) {
    std::printf("=== parse ===\n");
    {
        const char *body =
            "{\"candidates\":[{\"chinese\":\"\xe4\xbd\xa0\xe5\xa5\xbd\",\"pinyin\":\"ni hao\",\"confidence\":95},"
            "{\"chinese\":\"\xe4\xbd\xa0\xe5\x8f\xb7\",\"confidence\":40}]}";
        auto c = parse_cloud_response(body, 9);
        check("two candidates", c.size() == 2);
        check("chinese ok", c.size() == 2 && c[0].chinese == "\xe4\xbd\xa0\xe5\xa5\xbd");
        check("pinyin ok", c.size() == 2 && c[0].pinyin == "ni hao");
        check("confidence ok", c.size() == 2 && c[0].confidence == 95);
        check("missing field tolerated", c.size() == 2 && c[1].pinyin.empty());
    }
    {
        auto c = parse_cloud_response("{\"candidates\":[]}", 9);
        check("empty array", c.empty());
        c = parse_cloud_response("not json at all", 9);
        check("garbage -> empty", c.empty());
        c = parse_cloud_response("{\"error\":\"quota\"}", 9);
        check("error body -> empty", c.empty());
    }
    {
        const char *body =
            "{\"candidates\":[{\"chinese\":\"a\"},{\"chinese\":\"b\"},{\"chinese\":\"c\"}]}";
        auto c = parse_cloud_response(body, 2);
        check("limit respected", c.size() == 2);
    }
    {
        // 转义与 unicode 转义
        auto c = parse_cloud_response("{\"candidates\":[{\"chinese\":\"\\u4f60\\u597d\"}]}", 9);
        check("unicode escape", c.size() == 1 && c[0].chinese == "\xe4\xbd\xa0\xe5\xa5\xbd");
        c = parse_cloud_response("{\"candidates\":[{\"chinese\":\"a\\\"b\"}]}", 9);
        check("escaped quote", c.size() == 1 && c[0].chinese == "a\"b");
    }

    std::printf("=== cache ===\n");
    {
        CloudConfig cfg;
        cfg.enabled = true;
        cfg.endpoint = "http://127.0.0.1:1/never";
        cfg.cache_dir = "/tmp/pinyinime_cloud_test_cache";
        cfg.cache_ttl_s = 3600;
        CloudPinyinClient client(cfg);
        // 直接写缓存（private 方法不可用，走 query 之外的路径：手工造文件）
        std::system("mkdir -p /tmp/pinyinime_cloud_test_cache");
        FILE *f = std::fopen("/tmp/pinyinime_cloud_test_cache/nihao.cache", "w");
        std::fprintf(f, "#%ld\n95\tni hao\t\xe4\xbd\xa0\xe5\xa5\xbd\n", (long)time(nullptr));
        std::fclose(f);
        auto cached = client.query_cache_only("nihao");
        check("cache hit", cached.size() == 1 && cached[0].chinese == "\xe4\xbd\xa0\xe5\xa5\xbd");
        check("cache source tag", cached.size() == 1 && cached[0].source == "cloud-cache");
        check("cache pinyin", cached.size() == 1 && cached[0].pinyin == "ni hao");

        // TTL 过期
        CloudConfig cfg2 = cfg;
        cfg2.cache_ttl_s = 1;
        CloudPinyinClient client2(cfg2);
        FILE *f2 = std::fopen("/tmp/pinyinime_cloud_test_cache/nihao.cache", "w");
        std::fprintf(f2, "#%ld\n95\tni hao\t\xe4\xbd\xa0\xe5\xa5\xbd\n", (long)time(nullptr) - 10);
        std::fclose(f2);
        check("ttl expiry", client2.query_cache_only("nihao").empty());

        // 缓存关闭
        CloudConfig cfg3 = cfg;
        cfg3.use_cache = false;
        CloudPinyinClient client3(cfg3);
        check("cache disabled", client3.query_cache_only("nihao").empty());
    }

    std::printf("=== merge ===\n");
    {
        std::vector<std::string> local{"\xe4\xbd\xa0\xe5\xa5\xbd", "x"};
        std::vector<CloudCandidate> cloud{{"\xe4\xbd\xa0\xe5\xa5\xbd", "", 90, "cloud"},
                                          {"y", "", 50, "cloud"}};
        auto merged = CloudPinyinClient::merge(local, cloud);
        check("dedupe", merged.size() == 3);
        auto capped = CloudPinyinClient::merge(local, cloud, 2);
        check("cap respected", capped.size() == 2);
    }

    std::printf("=== disabled ===\n");
    {
        CloudPinyinClient off{CloudConfig{}};
        check("disabled -> empty", off.query("nihao").empty());
    }

    if (argc >= 2) {
        std::printf("=== live: %s ===\n", argv[1]);
        CloudConfig cfg;
        cfg.enabled = true;
        cfg.endpoint = argv[1];
        const char *cache_env = std::getenv("PINYINIME_CLOUD_TEST_CACHE");
        cfg.cache_dir = cache_env && *cache_env ? cache_env
                                                : "/tmp/pinyinime_cloud_test_cache_live";
        cfg.timeout_ms = 3000;
        CloudPinyinClient client(cfg);
        const char *q = argc >= 3 ? argv[2] : "nihao";
        auto r = client.query(q);
        std::printf("query(%s) -> %zu candidates\n", q, r.size());
        for (auto &c : r) {
            std::printf("  %d\t%s\t%s\n", c.confidence, c.pinyin.c_str(), c.chinese.c_str());
        }
        check("live candidates", !r.empty());
        if (r.empty()) std::printf("  last_error: %s\n", client.last_error().c_str());

        // 第二次应命中缓存（断网也能返回）
        auto again = client.query_cache_only(q);
        check("live cache written", !again.empty());
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
