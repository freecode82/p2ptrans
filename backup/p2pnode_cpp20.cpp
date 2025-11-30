#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <mutex>

#include "httplib.h"
#include "json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// -------------------------------------------
// 유틸
// -------------------------------------------
bool file_exists(const fs::path &p) {
    std::error_code ec;
    return fs::exists(p, ec);
}

void ensure_dir(const fs::path &p) {
    std::error_code ec;
    if (!fs::exists(p, ec)) {
        fs::create_directories(p, ec);
    }
}

int run_command(const std::string &cmd) {
    std::cout << "[CMD] " << cmd << std::endl;
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "[CMD] failed, code=" << ret << std::endl;
    }
    return ret;
}

void draw_progress(uint64_t downloaded, uint64_t total) {
    if (!total) return;
    double ratio = (double)downloaded / (double)total;
    int percent = (int)(ratio * 100.0);
    const int width = 40;
    int filled = (int)(ratio * width);
    std::string bar(filled, '#');
    bar.resize(width, '-');
    std::cout << "\r[" << bar << "] " << percent << "% (" << downloaded << "/" << total << ")";
    if (downloaded >= total) std::cout << std::endl;
    std::cout.flush();
}

bool auto_extract_archive(const fs::path &archive_path) {
    std::string name = archive_path.filename().string();
    fs::path dir = archive_path.parent_path();

    if (name.ends_with(".tar.gz") || name.ends_with(".tgz")) {
        std::string cmd = "tar -xzf \"" + archive_path.string() +
                          "\" -C \"" + dir.string() + "\"";
        return run_command(cmd) == 0 && std::remove(archive_path.string().c_str()) == 0;
    } else if (name.ends_with(".tar")) {
        std::string cmd = "tar -xf \"" + archive_path.string() +
                          "\" -C \"" + dir.string() + "\"";
        return run_command(cmd) == 0 && std::remove(archive_path.string().c_str()) == 0;
    } else if (name.ends_with(".gz")) {
        std::string cmd = "gunzip \"" + archive_path.string() + "\"";
        return run_command(cmd) == 0;
    }
    return true; // 기타 확장자는 놔둠
}

bool download_to_file_with_retry(const std::string &url,
                                 const fs::path &dest,
                                 bool show_progress,
                                 const std::string &label) {
    const int MAX_RETRY = 5;
    for (int attempt = 1; attempt <= MAX_RETRY; ++attempt) {
        try {
            httplib::Client cli(url.c_str());
            // httplib::Client 는 URL 전체가 아니라 host/port 기준이라,
            // 여기서는 아래 방식 대신 다른 방법을 써야 한다.
            // 편의를 위해 전체 URL을 파싱하지 않고,
            // 이 함수는 baseUrl이 아닌 "http://host:port/path" 대신
            // 이미 호스트/포트로 분리해서 쓰는 게 정상이다.
            // 그러나 여기서는 간단화를 위해 다른 인터페이스를 쓰자.
            // -> 이 함수는 실제로 아래에서 직접 구현된 클라이언트를 사용하지 않고,
            //    별도의 래퍼를 쓰는 편이 낫다.
            // 구현 단순화를 위해 아래에서 다시 작성하자.
        } catch (...) {}
        // 여기까지는 템플릿, 실제 구현은 아래에서...
        break;
    }
    return false;
}

// 실제 HTTP GET 다운로드 구현 (host, port, path)
bool http_download_file(const std::string &host,
                        int port,
                        const std::string &path,
                        const fs::path &dest,
                        bool show_progress) {
    httplib::Client cli(host.c_str(), port);
    cli.set_read_timeout(300, 0);
    std::ofstream ofs(dest, std::ios::binary);
    if (!ofs) {
        std::cerr << "[DOWNLOAD] cannot open dest: " << dest << std::endl;
        return false;
    }

    uint64_t total = 0;
    uint64_t downloaded = 0;

    auto res = cli.Get(path.c_str(),
        [&](const httplib::Response &res) {
            if (res.has_header("Content-Length")) {
                total = std::stoull(res.get_header_value("Content-Length"));
            }
            return true;
        },
        [&](const char *data, size_t data_length) {
            ofs.write(data, data_length);
            downloaded += data_length;
            if (show_progress && total) draw_progress(downloaded, total);
            return true;
        });

    ofs.close();

    if (!res || res->status != 200) {
        std::cerr << "[DOWNLOAD] error: " << (res ? res->status : 0) << std::endl;
        return false;
    }
    if (show_progress && total) std::cout << std::endl;
    return true;
}

// -------------------------------------------
// 압축 준비 (파일/폴더 → archive)
// -------------------------------------------

enum class PackMode { NONE, TAR, GZ, TARGZ };

struct ArchiveInfo {
    fs::path archive_path;
    std::string archive_name;
    bool cleanup = false;
};

ArchiveInfo prepare_archive(const fs::path &input, PackMode mode) {
    ArchiveInfo info;
    info.archive_path = input;
    info.archive_name = input.filename().string();
    info.cleanup = false;

    bool is_dir = fs::is_directory(input);
    bool is_file = fs::is_regular_file(input);

    if (!is_dir && !is_file) {
        throw std::runtime_error("path is neither file nor directory");
    }

    // mode NONE + 폴더이면 tar로 강제 (RAW 폴더는 C++버전에서 구현 X)
    if (is_dir && mode == PackMode::NONE) {
        mode = PackMode::TAR;
    }

    if (mode == PackMode::NONE) {
        return info;
    }

    fs::path tmp = fs::temp_directory_path();
    std::string base = input.filename().string();

    if (is_dir) {
        if (mode == PackMode::TAR) {
            info.archive_path = tmp / (base + ".tar");
            info.archive_name = info.archive_path.filename().string();
            std::string cmd = "cd \"" + input.parent_path().string() + "\" && tar -cf \"" +
                              info.archive_path.string() + "\" \"" + base + "\"";
            if (run_command(cmd) != 0) {
                throw std::runtime_error("tar failed");
            }
        } else { // GZ 또는 TARGZ -> tar.gz
            info.archive_path = tmp / (base + ".tar.gz");
            info.archive_name = info.archive_path.filename().string();
            std::string cmd = "cd \"" + input.parent_path().string() +
                              "\" && tar -czf \"" + info.archive_path.string() + "\" \"" +
                              base + "\"";
            if (run_command(cmd) != 0) {
                throw std::runtime_error("tar.gz failed");
            }
        }
    } else { // file
        if (mode == PackMode::TAR) {
            info.archive_path = tmp / (base + ".tar");
            info.archive_name = info.archive_path.filename().string();
            std::string cmd = "cd \"" + input.parent_path().string() + "\" && tar -cf \"" +
                              info.archive_path.string() + "\" \"" + base + "\"";
            if (run_command(cmd) != 0) {
                throw std::runtime_error("tar failed");
            }
        } else if (mode == PackMode::TARGZ) {
            info.archive_path = tmp / (base + ".tar.gz");
            info.archive_name = info.archive_path.filename().string();
            std::string cmd = "cd \"" + input.parent_path().string() +
                              "\" && tar -czf \"" + info.archive_path.string() + "\" \"" +
                              base + "\"";
            if (run_command(cmd) != 0) {
                throw std::runtime_error("tar.gz failed");
            }
        } else if (mode == PackMode::GZ) {
            info.archive_path = tmp / (base + ".gz");
            info.archive_name = info.archive_path.filename().string();
            std::string cmd = "gzip -c \"" + input.string() + "\" > \"" +
                              info.archive_path.string() + "\"";
            if (run_command(cmd) != 0) {
                throw std::runtime_error("gzip failed");
            }
        } else {
            return info;
        }
    }

    info.cleanup = true;
    return info;
}

// -------------------------------------------
// 마스터용 노드 정보
// -------------------------------------------
struct NodeInfo {
    std::string host;
    int ctrl_port;
    std::string name;
    uint64_t last_seen;
};

std::mutex g_nodes_mutex;
std::vector<NodeInfo> g_nodes; // master에서만 사용

// -------------------------------------------
// CONTROL SERVER
// -------------------------------------------

struct ControlConfig {
    std::string bind_host;
    int bind_port;
    bool is_master = false;
    std::string master_host;
    int master_port = 7000;
    std::string public_host;  // 다른 노드가 접근할 때 사용할 IP
    std::string node_name;
};

void start_control_server(const ControlConfig &cfg);

// -------------------------------------------
// SEND (1:1)
// -------------------------------------------
struct SendConfig {
    std::string source_host;
    int source_ctrl_port;
    std::string source_file;

    std::string target_host;
    int target_ctrl_port;
    std::string target_save;

    int send_port;
    PackMode pack_mode;
    bool auto_extract;
    bool progress;
};

void start_send(const SendConfig &cfg);

// -------------------------------------------
// SEND-ALL (master에게 브로드캐스트 명령)
// -------------------------------------------
struct SendAllConfig {
    std::string master_host;
    int master_port;

    std::string source_host;
    int source_ctrl_port;
    std::string source_file;

    int send_port;
    std::string target_save;

    PackMode pack_mode;
    bool auto_extract;
    bool progress;
};

void start_send_all(const SendAllConfig &cfg);

// -------------------------------------------
// CONTROL SERVER 구현
// -------------------------------------------
void start_control_server(const ControlConfig &cfg) {
    httplib::Server svr;

    std::cout << "\n[CONTROL] 서버 시작"
              << "\n  bind: " << cfg.bind_host << ":" << cfg.bind_port
              << "\n  mode: " << (cfg.is_master ? "MASTER" :
                                  (cfg.master_host.empty() ? "STANDALONE" : "WORKER"))
              << std::endl;

    // health
    svr.Get("/api/health", [](const httplib::Request&, httplib::Response &res) {
        json j;
        j["status"] = "ok";
        res.set_content(j.dump(), "application/json");
    });

    // ----- MASTER 모드 노드 관리 -----
    if (cfg.is_master) {
        // 자기 자신도 노드로 등록
        {
            NodeInfo self;
            self.host = cfg.public_host.empty() ? cfg.bind_host : cfg.public_host;
            self.ctrl_port = cfg.bind_port;
            self.name = cfg.node_name.empty() ? "master" : cfg.node_name;
            self.last_seen = std::time(nullptr);
            std::lock_guard<std::mutex> lk(g_nodes_mutex);
            g_nodes.push_back(self);
            std::cout << "[MASTER] 자기 자신 등록: " << self.host << ":" << self.ctrl_port << "\n";
        }

        svr.Post("/api/register-node", [](const httplib::Request &req, httplib::Response &res) {
            try {
                auto j = json::parse(req.body);
                std::string host = j.value("host", "");
                int ctrl_port = j.value("ctrlPort", 0);
                std::string name = j.value("name", "");
                if (host.empty() || ctrl_port == 0) {
                    res.status = 400;
                    res.set_content("{\"error\":\"host, ctrlPort required\"}", "application/json");
                    return;
                }

                uint64_t now = std::time(nullptr);
                std::lock_guard<std::mutex> lk(g_nodes_mutex);
                bool found = false;
                for (auto &n : g_nodes) {
                    if (n.host == host && n.ctrl_port == ctrl_port) {
                        n.last_seen = now;
                        if (!name.empty()) n.name = name;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    NodeInfo ni{host, ctrl_port, name, now};
                    g_nodes.push_back(ni);
                    std::cout << "[MASTER] 노드 등록: " << host << ":" << ctrl_port
                              << " (" << name << ")\n";
                }

                json r;
                r["status"] = "ok";
                res.set_content(r.dump(), "application/json");
            } catch (...) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid json\"}", "application/json");
            }
        });

        svr.Get("/api/nodes", [](const httplib::Request&, httplib::Response &res) {
            json j;
            {
                std::lock_guard<std::mutex> lk(g_nodes_mutex);
                json arr = json::array();
                for (auto &n : g_nodes) {
                    json nj;
                    nj["host"] = n.host;
                    nj["ctrlPort"] = n.ctrl_port;
                    nj["name"] = n.name;
                    nj["lastSeen"] = n.last_seen;
                    arr.push_back(nj);
                }
                j["nodes"] = arr;
            }
            res.set_content(j.dump(2), "application/json");
        });

        // /api/send-all: source-host → 모든 다른 노드로 브로드캐스트
        svr.Post("/api/send-all", [cfg](const httplib::Request &req, httplib::Response &res) {
            try {
                auto j = json::parse(req.body);
                std::string source_host = j.value("sourceHost", "");
                std::string source_file = j.value("sourceFile", "");
                int source_ctrl_port = j.value("sourceCtrlPort", cfg.bind_port);
                int send_port = j.value("sendPort", 9000);
                std::string target_save = j.value("targetSave", "");
                std::string pack_mode_str = j.value("packMode", "none");
                bool auto_extract = j.value("autoExtract", false);
                bool progress = j.value("progress", false);

                if (source_host.empty() || source_file.empty()) {
                    res.status = 400;
                    res.set_content("{\"error\":\"sourceHost, sourceFile required\"}",
                                    "application/json");
                    return;
                }

                PackMode pm = PackMode::NONE;
                if (pack_mode_str == "tar") pm = PackMode::TAR;
                else if (pack_mode_str == "gz") pm = PackMode::GZ;
                else if (pack_mode_str == "targz") pm = PackMode::TARGZ;

                std::cout << "\n[MASTER] send-all 요청"
                          << "\n  source : " << source_host << ":" << source_ctrl_port
                          << "\n  file   : " << source_file
                          << "\n  sendPort: " << send_port
                          << "\n  packMode: " << pack_mode_str
                          << "\n";

                std::vector<NodeInfo> targets;
                {
                    std::lock_guard<std::mutex> lk(g_nodes_mutex);
                    for (auto &n : g_nodes) {
                        if (n.host == source_host && n.ctrl_port == source_ctrl_port) continue;
                        targets.push_back(n);
                    }
                }

                json result;
                result["sourceHost"] = source_host;
                result["targets"] = json::array();

                for (auto &t : targets) {
                    std::cout << "[MASTER] 대상 → " << t.host << ":" << t.ctrl_port << "\n";
                    json tj;
                    tj["host"] = t.host;
                    tj["ctrlPort"] = t.ctrl_port;
                    tj["ok"] = false;

                    httplib::Client cli(source_host.c_str(), source_ctrl_port);
                    cli.set_read_timeout(300, 0);

                    json body;
                    body["filePath"] = source_file;
                    body["dataPort"] = send_port;
                    body["sourceHost"] = source_host;
                    body["targetHost"] = t.host;
                    body["targetCtrlPort"] = t.ctrl_port;
                    body["targetSave"] = target_save;
                    body["progress"] = progress;
                    body["autoExtract"] = auto_extract;
                    if (pm == PackMode::TAR) body["packMode"] = "tar";
                    else if (pm == PackMode::GZ) body["packMode"] = "gz";
                    else if (pm == PackMode::TARGZ) body["packMode"] = "targz";
                    else body["packMode"] = "none";

                    auto res2 = cli.Post("/api/send-file", body.dump(), "application/json");
                    if (res2 && res2->status == 200) {
                        tj["ok"] = true;
                        try {
                            tj["detail"] = json::parse(res2->body);
                        } catch (...) {
                            tj["detail"] = res2->body;
                        }
                    } else {
                        tj["error"] = res2 ? std::to_string(res2->status) : "no response";
                    }
                    result["targets"].push_back(tj);
                }

                res.set_content(result.dump(2), "application/json");
            } catch (...) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid json\"}", "application/json");
            }
        });
    }

    // /api/download-file : 단일 파일/아카이브 다운로드
    svr.Post("/api/download-file", [](const httplib::Request &req, httplib::Response &res) {
        try {
            auto j = json::parse(req.body);
            std::string url = j.value("url", "");
            std::string file_name = j.value("fileName", "");
            std::string save_dir = j.value("saveDir", "");
            bool progress = j.value("progress", false);
            bool auto_extract = j.value("autoExtract", false);

            if (url.empty() || file_name.empty()) {
                res.status = 400;
                res.set_content("{\"error\":\"url, fileName required\"}", "application/json");
                return;
            }

            fs::path dest_dir = save_dir.empty() ? fs::current_path() : fs::path(save_dir);
            ensure_dir(dest_dir);
            fs::path dest_path = dest_dir / file_name;

            // URL 파싱 (매우 단순: http://host:port/path 만 처리)
            if (!url.starts_with("http://")) {
                res.status = 400;
                res.set_content("{\"error\":\"only http:// supported\"}", "application/json");
                return;
            }
            std::string rest = url.substr(7);
            auto pos = rest.find('/');
            if (pos == std::string::npos) {
                res.status = 400;
                res.set_content("{\"error\":\"invalid url\"}", "application/json");
                return;
            }
            std::string hostport = rest.substr(0, pos);
            std::string path = rest.substr(pos);
            std::string host = hostport;
            int port = 80;
            auto colon = hostport.find(':');
            if (colon != std::string::npos) {
                host = hostport.substr(0, colon);
                port = std::stoi(hostport.substr(colon + 1));
            }

            std::cout << "\n[CONTROL:DOWNLOAD] " << url << " → " << dest_path << "\n";
            bool ok = http_download_file(host, port, path, dest_path, progress);
            if (!ok) {
                res.status = 500;
                res.set_content("{\"error\":\"download failed\"}", "application/json");
                return;
            }

            if (auto_extract) {
                if (!auto_extract_archive(dest_path)) {
                    std::cerr << "[CONTROL:DOWNLOAD] extract failed\n";
                }
            }

            json r;
            r["status"] = "ok";
            r["saved"] = dest_path.string();
            res.set_content(r.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"invalid json\"}", "application/json");
        }
    });

    // /api/send-file : 이 노드(source)에서 data 서버 띄우고 target에 다운로드 명령
    svr.Post("/api/send-file", [cfg](const httplib::Request &req, httplib::Response &res) {
        try {
            auto j = json::parse(req.body);
            std::string file_path = j.value("filePath", "");
            int data_port = j.value("dataPort", 9000);
            std::string source_host = j.value("sourceHost", "");
            std::string target_host = j.value("targetHost", "");
            int target_ctrl_port = j.value("targetCtrlPort", cfg.bind_port);
            std::string target_save = j.value("targetSave", "");
            bool progress = j.value("progress", false);
            bool auto_extract = j.value("autoExtract", false);
            std::string pack_mode_str = j.value("packMode", "none");

            if (file_path.empty() || source_host.empty() || target_host.empty()) {
                res.status = 400;
                res.set_content("{\"error\":\"filePath, sourceHost, targetHost required\"}",
                                "application/json");
                return;
            }

            PackMode pm = PackMode::NONE;
            if (pack_mode_str == "tar") pm = PackMode::TAR;
            else if (pack_mode_str == "gz") pm = PackMode::GZ;
            else if (pack_mode_str == "targz") pm = PackMode::TARGZ;

            fs::path p(file_path);
            if (!file_exists(p)) {
                res.status = 400;
                res.set_content("{\"error\":\"filePath not found\"}", "application/json");
                return;
            }

            std::cout << "\n[CONTROL:SEND] 요청"
                      << "\n  path  : " << p
                      << "\n  mode  : " << pack_mode_str
                      << "\n  data  : " << source_host << ":" << data_port
                      << "\n  target: " << target_host << ":" << target_ctrl_port
                      << "\n";

            ArchiveInfo ai = prepare_archive(p, pm);
            auto size = fs::file_size(ai.archive_path);

            // data 서버
            auto svr_data = std::make_shared<httplib::Server>();
            svr_data->Get("/download", [ai, size](const httplib::Request&, httplib::Response &res2) {
                std::ifstream ifs(ai.archive_path, std::ios::binary);
                if (!ifs) {
                    res2.status = 500;
                    return;
                }
                res2.set_header("Content-Type", "application/octet-stream");
                res2.set_header("Content-Disposition",
                                "attachment; filename=\"" + ai.archive_name + "\"");
                res2.set_header("Content-Length", std::to_string(size));
                res2.set_content_provider(size, "application/octet-stream",
                    [ifs = std::move(ifs)](size_t offset, size_t length,
                                           httplib::DataSink &sink) mutable {
                        ifs.seekg(offset, std::ios::beg);
                        std::vector<char> buf(length);
                        ifs.read(buf.data(), length);
                        sink.write(buf.data(), (size_t)ifs.gcount());
                        return true;
                    });
            });

            std::thread th([svr_data, data_port]() {
                std::cout << "[DATA] listen 0.0.0.0:" << data_port << "/download\n";
                svr_data->listen("0.0.0.0", data_port);
            });

            // 대상에게 다운로드 명령
            httplib::Client cli(target_host.c_str(), target_ctrl_port);
            cli.set_read_timeout(300, 0);

            std::string url = "http://" + source_host + ":" +
                              std::to_string(data_port) + "/download";

            json body2;
            body2["url"] = url;
            body2["fileName"] = ai.archive_name;
            body2["saveDir"] = target_save;
            body2["progress"] = progress;
            body2["autoExtract"] = auto_extract;

            auto res2 = cli.Post("/api/download-file", body2.dump(), "application/json");
            svr_data->stop();
            if (th.joinable()) th.join();

            if (ai.cleanup) {
                std::error_code ec;
                fs::remove(ai.archive_path, ec);
            }

            if (!res2 || res2->status != 200) {
                res.status = 500;
                res.set_content("{\"error\":\"target download failed\"}", "application/json");
                return;
            }

            json r;
            r["status"] = "ok";
            try {
                r["detail"] = json::parse(res2->body);
            } catch (...) {
                r["detail_raw"] = res2->body;
            }
            res.set_content(r.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"invalid json\"}", "application/json");
        }
    });

    // 서버 시작
    svr.listen(cfg.bind_host.c_str(), cfg.bind_port);
}

// -------------------------------------------
// SEND (1:1)
// -------------------------------------------
void start_send(const SendConfig &cfg) {
    std::cout << "\n[SEND] 1:1 전송 명령"
              << "\n  src : " << cfg.source_host << ":" << cfg.source_ctrl_port
              << "\n  dst : " << cfg.target_host << ":" << cfg.target_ctrl_port
              << "\n  file: " << cfg.source_file
              << "\n";

    httplib::Client cli(cfg.source_host.c_str(), cfg.source_ctrl_port);
    cli.set_read_timeout(300, 0);

    json body;
    body["filePath"] = cfg.source_file;
    body["dataPort"] = cfg.send_port;
    body["sourceHost"] = cfg.source_host;
    body["targetHost"] = cfg.target_host;
    body["targetCtrlPort"] = cfg.target_ctrl_port;
    body["targetSave"] = cfg.target_save;
    body["progress"] = cfg.progress;
    body["autoExtract"] = cfg.auto_extract;

    if (cfg.pack_mode == PackMode::TAR) body["packMode"] = "tar";
    else if (cfg.pack_mode == PackMode::GZ) body["packMode"] = "gz";
    else if (cfg.pack_mode == PackMode::TARGZ) body["packMode"] = "targz";
    else body["packMode"] = "none";

    auto res = cli.Post("/api/send-file", body.dump(), "application/json");
    if (!res) {
        std::cerr << "[SEND] no response\n";
        return;
    }
    std::cout << "[SEND] status: " << res->status << "\n";
    std::cout << res->body << std::endl;
}

// -------------------------------------------
// SEND-ALL
// -------------------------------------------
void start_send_all(const SendAllConfig &cfg) {
    std::cout << "\n[SEND-ALL] 브로드캐스트 전송"
              << "\n  master: " << cfg.master_host << ":" << cfg.master_port
              << "\n  source: " << cfg.source_host << ":" << cfg.source_ctrl_port
              << "\n  file  : " << cfg.source_file
              << "\n";

    httplib::Client cli(cfg.master_host.c_str(), cfg.master_port);
    cli.set_read_timeout(300, 0);

    json body;
    body["sourceHost"] = cfg.source_host;
    body["sourceFile"] = cfg.source_file;
    body["sourceCtrlPort"] = cfg.source_ctrl_port;
    body["sendPort"] = cfg.send_port;
    body["targetSave"] = cfg.target_save;
    body["progress"] = cfg.progress;
    body["autoExtract"] = cfg.auto_extract;

    if (cfg.pack_mode == PackMode::TAR) body["packMode"] = "tar";
    else if (cfg.pack_mode == PackMode::GZ) body["packMode"] = "gz";
    else if (cfg.pack_mode == PackMode::TARGZ) body["packMode"] = "targz";
    else body["packMode"] = "none";

    auto res = cli.Post("/api/send-all", body.dump(), "application/json");
    if (!res) {
        std::cerr << "[SEND-ALL] no response\n";
        return;
    }
    std::cout << "[SEND-ALL] status: " << res->status << "\n";
    std::cout << res->body << std::endl;
}

// -------------------------------------------
// CLI 파서
// -------------------------------------------
std::map<std::string, std::string> parse_args(int argc, char **argv) {
    std::map<std::string, std::string> o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.starts_with("--")) {
            std::string key = a.substr(2);
            if (i + 1 < argc && argv[i+1][0] != '-') {
                o[key] = argv[++i];
            } else {
                o[key] = "true";
            }
        } else if (a.starts_with("-")) {
            std::string key = a.substr(1);
            if (i + 1 < argc && argv[i+1][0] != '-') {
                o[key] = argv[++i];
            } else {
                o[key] = "true";
            }
        }
    }
    return o;
}

// -------------------------------------------
// main
// -------------------------------------------
int main(int argc, char **argv) {
    auto args = parse_args(argc, argv);

    auto has = [&](const std::string &k) {
        auto it = args.find(k);
        return it != args.end() && it->second == "true";
    };
    auto get = [&](const std::string &k, const std::string &def="") {
        auto it = args.find(k);
        return it == args.end() ? def : it->second;
    };

    if (has("control")) {
        ControlConfig cfg;
        cfg.bind_host = get("h", get("host", "0.0.0.0"));
        cfg.bind_port = std::stoi(get("p", get("port", "7000")));
        cfg.is_master = has("master");
        cfg.master_host = get("master-host", "");
        cfg.master_port = std::stoi(get("master-port", "7000"));
        cfg.public_host = get("node-host", get("public-host", ""));
        cfg.node_name = get("node-name", "");
        start_control_server(cfg);
        return 0;
    }

    bool is_send = has("send");
    bool is_send_all = has("send-all");

    // 공통 packMode
    bool opt_t = has("t");
    bool opt_g = has("g");
    bool opt_tg = has("tg");
    PackMode pm = PackMode::NONE;
    if (opt_tg || (opt_t && opt_g)) pm = PackMode::TARGZ;
    else if (opt_t) pm = PackMode::TAR;
    else if (opt_g) pm = PackMode::GZ;

    bool norelease = has("norelease");
    bool progress = has("b") || has("progress");

    if (is_send) {
        SendConfig cfg;
        cfg.source_host = get("source-host", get("send-host", "127.0.0.1"));
        cfg.source_ctrl_port = std::stoi(get("source-port", "7000"));
        cfg.source_file = get("source-file", get("f", ""));
        cfg.target_host = get("target-host", get("client-host", ""));
        cfg.target_ctrl_port = std::stoi(get("target-port", get("client-port", "7000")));
        cfg.target_save = get("target-save", get("client-save", ""));
        cfg.send_port = std::stoi(get("send-port", "9000"));
        cfg.pack_mode = pm;
        cfg.auto_extract = (!norelease && pm != PackMode::NONE);
        cfg.progress = progress;

        if (cfg.source_file.empty()) {
            std::cerr << "Error: --source-file 또는 -f 필요\n";
            return 1;
        }
        if (cfg.target_host.empty()) {
            std::cerr << "Error: --target-host 필요\n";
            return 1;
        }

        start_send(cfg);
        return 0;
    }

    if (is_send_all) {
        SendAllConfig cfg;
        cfg.master_host = get("master-host", "");
        cfg.master_port = std::stoi(get("master-port", "7000"));
        cfg.source_host = get("source-host", "127.0.0.1");
        cfg.source_ctrl_port = std::stoi(get("source-port", "7000"));
        cfg.source_file = get("source-file", get("f", ""));
        cfg.send_port = std::stoi(get("send-port", "9000"));
        cfg.target_save = get("target-save", get("client-save", ""));
        cfg.pack_mode = pm;
        cfg.auto_extract = (!norelease && pm != PackMode::NONE);
        cfg.progress = progress;

        if (cfg.master_host.empty()) {
            std::cerr << "Error: --master-host 필요\n";
            return 1;
        }
        if (cfg.source_file.empty()) {
            std::cerr << "Error: --source-file 또는 -f 필요\n";
            return 1;
        }

        start_send_all(cfg);
        return 0;
    }

    std::cout << R"(사용법:

1) 마스터 컨트롤 서버 실행
   ./p2pnode --control --master -p 7000 -h 0.0.0.0 --node-host 192.168.0.10

2) 워커 컨트롤 서버 실행
   ./p2pnode --control \
     --master-host 192.168.0.10 --master-port 7000 \
     --node-host 192.168.0.11 \
     -p 7000 -h 0.0.0.0

3) 1:1 전송
   ./p2pnode --send \
     --source-host 192.168.0.10 --source-port 7000 \
     --source-file /root/testdir \
     --send-port 9000 \
     --target-host 192.168.0.11 --target-port 7000 \
     --target-save /root/downloads \
     -tg -b

4) 브로드캐스트 전송 (send-all)
   ./p2pnode --send-all \
     --master-host 192.168.0.10 --master-port 7000 \
     --source-host 192.168.0.10 --source-port 7000 \
     --source-file /root/testdir \
     --send-port 9000 \
     --target-save /root/downloads \
     -tg -b

옵션:
  --control            컨트롤 서버 실행
    --master           마스터 모드
    --master-host      워커 모드일 때 마스터 IP
    --master-port      마스터 컨트롤 포트 (기본 7000)
    --node-host        다른 노드가 접근할 IP (공개 IP)
    -p, --port         컨트롤 포트 (기본 7000)
    -h, --host         바인딩 IP (기본 0.0.0.0)

  --send               1:1 전송
    --source-host      소스 컨트롤 호스트
    --source-port      소스 컨트롤 포트
    --source-file, -f  전송할 파일/폴더
    --send-port        데이터 서버 포트 (기본 9000)
    --target-host      대상 컨트롤 호스트
    --target-port      대상 컨트롤 포트
    --target-save      대상 저장 디렉토리

  --send-all           마스터를 통한 브로드캐스트
    --master-host      마스터 호스트
    --master-port      마스터 컨트롤 포트
    --source-host      소스 컨트롤 호스트
    --source-port      소스 컨트롤 포트
    --source-file, -f  전송할 파일/폴더
    --send-port        데이터 서버 포트
    --target-save      대상들이 저장할 디렉토리

  -t                   tar
  -g                   gz (파일: .gz, 폴더: tar.gz)
  -tg                  tar.gz
  -norelease           수신측 압축 해제 안 함
  -b, --progress       진행률 표시
)";
    return 0;
}

