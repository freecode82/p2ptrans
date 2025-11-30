#!/usr/bin/env node

/**
 * P2P 2.2 — 완전 양방향 전송 시스템
 * - 컨트롤 서버는 양쪽에서 항상 실행
 * - 어느 노드에서 명령 내리든 A→B, B→A 전송 가능
 * - 데이터 서버는 항상 "source-host"에서만 뜬다
 * - 다운로드는 항상 target-host의 컨트롤 서버가 수행
 */

const express = require("express");
const axios = require("axios");
const fs = require("fs");
const path = require("path");

// -------------------------------------------
// Utils
// -------------------------------------------
function fileExists(p) {
    try { fs.accessSync(p); return true; }
    catch { return false; }
}

function ensureDir(dir) {
    if (!fileExists(dir)) fs.mkdirSync(dir, { recursive: true });
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function drawProgress(downloaded, total) {
    if (!total) return;
    const ratio = downloaded / total;
    const percent = Math.floor(ratio * 100);
    const width = 40;
    const filled = Math.floor(ratio * width);
    const bar = "█".repeat(filled) + "-".repeat(width - filled);
    process.stdout.write(`\r[${bar}] ${percent}% (${downloaded}/${total})`);
    if (downloaded >= total) process.stdout.write("\n");
}

// -------------------------------------------
// CONTROL SERVER (항상 켜져 있음)
// -------------------------------------------
async function startControlServer(port, host) {
    const app = express();
    app.use(express.json());

    console.log(`\n������ CONTROL SERVER RUNNING`);
    console.log(`  Host: ${host}`);
    console.log(`  Port: ${port}`);
    console.log(`  /api/download-file`);
    console.log(`  /api/send-file\n`);

    // health check
    app.get("/api/health", (req, res) => {
        res.json({ status: "ok" });
    });

    /**
     * 대상 노드에서 파일을 "다운로드"하는 API
     * (실제 데이터 전송은 항상 GET으로 이 노드에서 수행)
     *
     * body:
     *  {
     *    url: "http://sourceHost:sendPort/download",
     *    fileName: "xxx.tar.gz",
     *    saveDir: "/path/to/save",
     *    progress: true
     *  }
     */
    app.post("/api/download-file", async (req, res) => {
        const { url, fileName, saveDir, progress } = req.body || {};

        if (!url || !fileName) {
            return res.status(400).json({ error: "url, fileName 필수" });
        }

        const destDir = saveDir ? path.resolve(saveDir) : process.cwd();
        ensureDir(destDir);
        const destPath = path.join(destDir, fileName);

        console.log(`\n[CONTROL:DOWNLOAD] 요청`);
        console.log(`  URL  : ${url}`);
        console.log(`  Dest : ${destPath}`);

        const MAX_RETRY = 5;
        let attempt = 0;

        while (attempt < MAX_RETRY) {
            try {
                const resp = await axios.get(url, { responseType: "stream" });
                const total = parseInt(resp.headers["content-length"] || "0", 10);
                let downloaded = 0;

                const ws = fs.createWriteStream(destPath);

                await new Promise((resolve, reject) => {
                    resp.data.on("data", chunk => {
                        downloaded += chunk.length;
                        if (progress && total) drawProgress(downloaded, total);
                    });
                    resp.data.on("error", reject);
                    ws.on("error", reject);
                    ws.on("finish", resolve);

                    resp.data.pipe(ws);
                });

                console.log(`\n[CONTROL:DOWNLOAD] 완료: ${destPath}`);
                return res.status(200).json({ status: "ok", saved: destPath });

            } catch (err) {
                attempt++;
                console.warn(`[CONTROL:DOWNLOAD] 실패(${attempt}/${MAX_RETRY}): ${err.message}`);
                if (attempt >= MAX_RETRY) {
                    console.warn(`[CONTROL:DOWNLOAD] 재시도 한계 도달. 중단.`);
                    return res.status(500).json({ error: err.message });
                }
                await sleep(500);
            }
        }
    });

    /**
     * 이 노드(source-host)에서 파일을 전송하기 위한 API
     *  - 임시 데이터 서버를 이 노드에서 띄우고
     *  - target-host의 /api/download-file 을 호출한다.
     *
     * body:
     *  {
     *    filePath: "/root/file.tar.gz",
     *    dataPort: 9000,
     *    sourceHost: "192.168.79.9", // 상대가 접속할 IP
     *    targetHost: "192.168.146.131",
     *    targetCtrlPort: 7000,
     *    targetSave: "/root/p2phttp/downloads",
     *    progress: true
     *  }
     */
    app.post("/api/send-file", async (req, res) => {
        const {
            filePath,
            dataPort,
            sourceHost,
            targetHost,
            targetCtrlPort,
            targetSave,
            progress
        } = req.body || {};

        if (!filePath || !sourceHost || !targetHost) {
            return res.status(400).json({ error: "filePath, sourceHost, targetHost 필수" });
        }

        const resolvedPath = path.resolve(filePath);
        if (!fileExists(resolvedPath)) {
            return res.status(400).json({ error: `file not found: ${resolvedPath}` });
        }

        const stat = fs.statSync(resolvedPath);
        if (!stat.isFile()) {
            return res.status(400).json({ error: "현재 버전은 파일만 지원 (폴더 X)" });
        }

        const fileName = path.basename(resolvedPath);
        const size = stat.size;
        const portData = dataPort || 9000;
        const targetPort = targetCtrlPort || port; // targetPort 미지정 시 이 서버와 동일한 control 포트로 가정

        console.log(`\n[CONTROL:SEND] 전송 요청`);
        console.log(`  filePath : ${resolvedPath}`);
        console.log(`  size     : ${size} bytes`);
        console.log(`  source   : ${sourceHost}:${portData}`);
        console.log(`  target   : ${targetHost}:${targetPort}`);
        console.log(`  saveDir  : ${targetSave || "(target current dir)"}`);

        // 임시 데이터 서버 생성
        const dataApp = express();

        dataApp.get("/download", (req2, res2) => {
            console.log(`[DATA] /download 요청 수신 → 파일 스트리밍 시작`);
            res2.setHeader("Content-Type", "application/octet-stream");
            res2.setHeader("Content-Disposition", `attachment; filename="${fileName}"`);
            res2.setHeader("Content-Length", size);

            const rs = fs.createReadStream(resolvedPath);
            rs.on("error", err => {
                console.error("[DATA] ReadStream 오류:", err.message);
                res2.destroy(err);
            });
            rs.pipe(res2);
        });

        const dataServer = dataApp.listen(portData, "0.0.0.0", async () => {
            console.log(`[DATA] 임시 데이터 서버 ON → http://${sourceHost}:${portData}/download`);

            const downloadUrl = `http://${sourceHost}:${portData}/download`;
            const targetCtrlUrl = `http://${targetHost}:${targetPort}/api/download-file`;

            try {
                const resp = await axios.post(targetCtrlUrl, {
                    url: downloadUrl,
                    fileName,
                    saveDir: targetSave,
                    progress
                }, { timeout: 0 });

                console.log(`[CONTROL:SEND] 대상 응답:`, resp.data);
                res.status(200).json({ status: "ok", detail: resp.data });

            } catch (err) {
                console.error(`[CONTROL:SEND] 대상 다운로드 호출 실패: ${err.message}`);
                res.status(500).json({ error: err.message });

            } finally {
                console.log(`[DATA] 임시 데이터 서버 종료`);
                dataServer.close();
            }
        });
    });

    app.listen(port, host);
}

// -------------------------------------------
// SEND MODE (오케스트레이터)
// -------------------------------------------
async function startSendMode(opt) {
    const {
        sourceHost,
        sourceFile,
        sourceCtrlPort,
        sendPort,
        targetHost,
        targetCtrlPort,
        targetSave,
        progress
    } = opt;

    const controlUrl = `http://${sourceHost}:${sourceCtrlPort}/api/send-file`;

    console.log(`\n[SEND] 전송 명령 시작`);
    console.log(`  source-host(ctrl) : ${sourceHost}:${sourceCtrlPort}`);
    console.log(`  target-host(ctrl) : ${targetHost}:${targetCtrlPort}`);
    console.log(`  file              : ${sourceFile}`);
    console.log(`  sendPort(data)    : ${sendPort}`);
    console.log(`  targetSave        : ${targetSave || "(target current dir)"}`);

    try {
        const resp = await axios.post(controlUrl, {
            filePath: sourceFile,
            dataPort: sendPort,
            sourceHost,
            targetHost,
            targetCtrlPort,
            targetSave,
            progress
        }, { timeout: 0 });

        console.log(`\n[SEND] 완료 응답:`, resp.data);
    } catch (err) {
        console.error(`\n[SEND] 실패: ${err.message}`);
    }
}

// -------------------------------------------
// CLI Parser
// -------------------------------------------
function parseArgs(argv) {
    const out = {};
    for (let i = 0; i < argv.length; i++) {
        const a = argv[i];
        if (a.startsWith("--")) {
            const key = a.slice(2);
            const next = argv[i + 1];
            if (!next || next.startsWith("-")) out[key] = true;
            else { out[key] = next; i++; }
        } else if (a.startsWith("-")) {
            const key = a.slice(1);
            const next = argv[i + 1];
            if (!next || next.startsWith("-")) out[key] = true;
            else { out[key] = next; i++; }
        }
    }
    return out;
}

// -------------------------------------------
// ENTRY
// -------------------------------------------
(async () => {
    const args = parseArgs(process.argv.slice(2));

    // CONTROL MODE
    if (args.control) {
        const host = args.h || args.host || "0.0.0.0";
        const port = parseInt(args.p || args.port || 7000, 10);
        await startControlServer(port, host);
        return;
    }

    // SEND MODE
    if (args.send) {
        const sourceHost      = args["source-host"] || args["send-host"] || "127.0.0.1";
        const sourceFile      = args["source-file"] || args.f;
        const sourceCtrlPort  = parseInt(args["source-port"] || 7000, 10); // 이 호스트의 control 포트
        const sendPort        = parseInt(args["send-port"] || 9000, 10);   // 데이터 포트

        const targetHost      = args["target-host"] || args["client-host"];
        const targetCtrlPort  = parseInt(args["target-port"] || args["client-port"] || 7000, 10);
        const targetSave      = args["target-save"] || args["client-save"];
        const progress        = !!(args.b || args.progress);

        if (!sourceFile) {
            console.error("Error: --source-file 또는 -f <파일 경로> 가 필요합니다.");
            process.exit(1);
        }
        if (!targetHost) {
            console.error("Error: --target-host <IP> 가 필요합니다.");
            process.exit(1);
        }

        await startSendMode({
            sourceHost,
            sourceFile,
            sourceCtrlPort,
            sendPort,
            targetHost,
            targetCtrlPort,
            targetSave,
            progress
        });
        return;
    }

    // Usage
    console.log(`
사용법:

1) 각 노드에서 컨트롤 서버 실행
   node p2pnode.js --control -p 7000 -h 0.0.0.0

2) 원하는 곳에서 전송 명령 (A→B 예시)
   node p2pnode.js --send \\
     --source-host 192.168.79.9 \\
     --source-file /root/node-v18.13.0.tar.gz \\
     --send-port 9000 \\
     --source-port 7000 \\
     --target-host 192.168.146.131 \\
     --target-port 7000 \\
     --target-save /root/p2phttp/downloads \\
     -b

옵션 요약:

  --control
    -p, --port          컨트롤 포트 (기본 7000)
    -h, --host          바인딩 IP (기본 0.0.0.0)

  --send
    --source-host       파일이 있는 노드 IP (이 노드에서 9000 데이터 서버 뜸)
    --source-file, -f   전송할 파일 경로
    --source-port       source-host의 컨트롤 포트 (기본 7000)
    --send-port         데이터 서버 포트 (기본 9000)

    --target-host       파일 받을 노드 IP
    --target-port       target-host 컨트롤 포트 (기본 7000)
    --target-save       target-host에서 저장할 디렉토리

    -b, --progress      다운로드 진행률 표시
`);
})();

