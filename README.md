# p2ptrans
g++ -std=c++17 -O2 p2pnode.cpp -o p2pnode -lpthread <br />

사용법:<br />

1) 각 노드에서 컨트롤 서버 실행<br />
   node p2pnode.js --control -p 7000 -h 0.0.0.0 <br />
<br />
2) 원하는 곳에서 전송 명령 (A→B 예시) <br />
   node p2pnode.js --send \\ <br />
     --source-host 192.168.79.9 \\ <br />
     --source-file /root/node-v18.13.0.tar.gz \\ <br />
     --send-port 9000 \\ <br />
     --source-port 7000 \\ <br />
     --target-host 192.168.146.131 \\ <br />
     --target-port 7000 \\ <br />
     --target-save /root/p2phttp/downloads \\ <br />
     -b <br />

옵션 요약: <br />

  --control <br />
    -p, --port          컨트롤 포트 (기본 7000) <br />
    -h, --host          바인딩 IP (기본 0.0.0.0) <br />
<br />
  --send <br />
    --source-host       파일이 있는 노드 IP (이 노드에서 9000 데이터 서버 뜸) <br />
    --source-file, -f   전송할 파일 경로 <br />
    --source-port       source-host의 컨트롤 포트 (기본 7000) <br />
    --send-port         데이터 서버 포트 (기본 9000) <br />
<br />
    --target-host       파일 받을 노드 IP <br />
    --target-port       target-host 컨트롤 포트 (기본 7000) <br />
    --target-save       target-host에서 저장할 디렉토리<br />
<br />
    -b, --progress      다운로드 진행률 표시<br />

<br /><br />
Usage example: <br />
```
root@ubuntu2:~/p2phttp# node p2pnode.js --send        --source-host 192.168.79.9  --source-port 20000      --source-file /root/node-v18.13.0.tar.gz        --send-port 10000        --target-host 192.168.146.131        --target-port 20000        --target-save /root/p2phttp/downloads        -b

[SEND] 전송 명령 시작
  source-host(ctrl) : 192.168.79.9:20000
  target-host(ctrl) : 192.168.146.131:20000
  file              : /root/node-v18.13.0.tar.gz
  sendPort(data)    : 10000
  targetSave        : /root/p2phttp/downloads

[SEND] 완료 응답: {
  status: 'ok',
  detail: {
    status: 'ok',
    saved: '/root/p2phttp/downloads/node-v18.13.0.tar.gz'
  }
}

root@ubuntu2:~/p2phttp# node p2pnode.js --send        --source-host 192.168.79.9  --source-port 7000      --source-file /root/node-v18.13.0.tar.gz        --send-port 10000        --target-host 192.168.146.131        --target-port 20000        --target-save /root/p2phttp/downloads        -b

[SEND] 전송 명령 시작
  source-host(ctrl) : 192.168.79.9:7000
  target-host(ctrl) : 192.168.146.131:20000
  file              : /root/node-v18.13.0.tar.gz
  sendPort(data)    : 10000
  targetSave        : /root/p2phttp/downloads

[SEND] 완료 응답: {
  status: 'ok',
  detail: {
    status: 'ok',
    saved: '/root/p2phttp/downloads/node-v18.13.0.tar.gz'
  }
}

root@ubuntu2:~/p2phttp# node p2pnode.js --send        --source-host 192.168.79.9        --source-file /root/node-v18.13.0.tar.gz        --send-port 10000        --target-host 192.168.146.131        --target-port 7000        --target-save /root/p2phttp/downloads        -b

[SEND] 전송 명령 시작
  source-host(ctrl) : 192.168.79.9:7000
  target-host(ctrl) : 192.168.146.131:7000
  file              : /root/node-v18.13.0.tar.gz
  sendPort(data)    : 10000
  targetSave        : /root/p2phttp/downloads

[SEND] 완료 응답: {
  status: 'ok',
  detail: {
    status: 'ok',
    saved: '/root/p2phttp/downloads/node-v18.13.0.tar.gz'
  }
}
```

사용법:

1) 컨트롤 서버 (양쪽에서 실행)
   node p2pnode.js --control -p 7000 -h 0.0.0.0

2) 전송 명령 (파일 또는 폴더)

   # A(192.168.79.9) → B(192.168.146.131) 로 전송
   # (명령은 A든 B든 아무 곳에서나 실행 가능)

   ## 1) 파일/폴더를 tar.gz 로 보내고, 받는 쪽에서 자동 해제
   node p2pnode.js --send \
     --source-host 192.168.79.9 \
     --source-file /root/testdir \
     --send-port 9000 \
     --source-port 7000 \
     --target-host 192.168.146.131 \
     --target-port 7000 \
     --target-save /root/p2phttp/downloads \
     -tg -b

   ## 2) 압축 없이 폴더 RAW 전송 (최상위 폴더 포함)
   node p2pnode.js --send \
     --source-host 192.168.79.9 \
     --source-file /root/testdir \
     --send-port 9000 \
     --source-port 7000 \
     --target-host 192.168.146.131 \
     --target-port 7000 \
     --target-save /root/p2phttp/downloads

   ## 3) 압축은 하지만 해제는 하지 않기 (-norelease)
   node p2pnode.js --send \
     --source-host 192.168.79.9 \
     --source-file /root/testdir \
     --send-port 9000 \
     --source-port 7000 \
     --target-host 192.168.146.131 \
     --target-port 7000 \
     --target-save /root/p2phttp/downloads \
     -tg -norelease -b

옵션 정리:

  --control
    -p, --port          컨트롤 포트 (기본 7000)
    -h, --host          바인딩 IP (기본 0.0.0.0)

  --send
    --source-host       파일/폴더가 있는 노드 IP
    --source-file, -f   전송할 파일 또는 폴더 경로
    --source-port       source-host 컨트롤 포트 (기본 7000)
    --send-port         데이터 서버 포트 (기본 9000)

    --target-host       받을 노드 IP
    --target-port       target-host 컨트롤 포트 (기본 7000)
    --target-save       target-host가 저장할 디렉토리

    -t                  tar 로 묶어서 전송
    -g                  gzip 압축 (파일: .gz, 폴더: .tar.gz)
    -tg                 tar.gz 로 전송
    -norelease          받은 뒤 압축 해제하지 않음
    -b, --progress      단일 파일/아카이브 전송 시 진행률 표시







node p2pnode.js --send \
     --source-host 192.168.79.9 \
     --source-file /root/openssl-1.1.1w \
     --send-port 9000 \
     --source-port 7000 \
     --target-host 192.168.146.131 \
     --target-port 7000 \
     --target-save /root/p2phttp/downloads \
     -tg -b

node p2pnode.js --send \
     --source-host 192.168.79.9 \
     --source-file /root/openssl-1.1.1w \
     --send-port 9000 \
     --source-port 7000 \
     --target-host 192.168.146.131 \
     --target-port 7000 \
     --target-save /root/p2phttp/downloads \
     -b




1) 마스터 컨트롤 서버 실행 (예: A)
   node p2pnode.js --control --master -p 7000 -h 0.0.0.0

2) 워커 컨트롤 서버 실행 (예: B, C, D)
   node p2pnode.js --control \
     --master-host 192.168.79.9 \
     --master-port 7000 \
     --node-host 192.168.79.11 \
     -p 7000 -h 0.0.0.0

3) 1:1 전송 (기존 방식)
   node p2pnode.js --send \
     --source-host 192.168.146.131 \
     --source-file /root/quickbuild-13.0.39.tar.gz \
     --send-port 9000 \
     --source-port 7000 \
     --target-host 192.168.79.9 \
     --target-port 7000 \
     --target-save /root/p2phttp/downloads \
     -tg -b

4) 브로드캐스트 전송 (send-all)
   node p2pnode.js --send-all \
     --master-host 192.168.79.9 \
     --master-port 7000 \
     --source-host 192.168.79.9 \
     --source-file /root/openssl-1.1.1w \
     --send-port 9000 \
     --source-port 7000 \
     --target-save /root/p2phttp/downloads \
     -tg -b -norelease

옵션 정리:

  --control
    -p, --port          컨트롤 포트 (기본 7000)
    -h, --host          바인딩 IP (기본 0.0.0.0)
    --master            마스터 모드 (노드 목록 관리 + send-all)
    --master-host       워커 모드일 때 마스터의 IP
    --master-port       마스터 컨트롤 포트 (기본 7000)
    --node-host         다른 노드가 이 노드를 접근할 때 사용할 공개 IP/호스트
    --node-name         노드 이름 표시용

  --send (1:1)
    --source-host       파일/폴더가 있는 노드 IP
    --source-file, -f   전송할 파일 또는 폴더 경로
    --source-port       source-host 컨트롤 포트 (기본 7000)
    --send-port         데이터 서버 포트 (기본 9000)
    --target-host       받을 노드 IP
    --target-port       target-host 컨트롤 포트 (기본 7000)
    --target-save       target-host가 저장할 디렉토리
    -t                  tar 로 묶어서 전송
    -g                  gzip 압축 (파일: .gz, 폴더: .tar.gz)
    -tg                 tar.gz 로 전송
    -norelease          받은 뒤 압축 해제하지 않음
    -b, --progress      단일 파일/아카이브 전송 시 진행률 표시

  --send-all (마스터 사용)
    --master-host       마스터 IP
    --master-port       마스터 컨트롤 포트 (기본 7000)
    --source-host       파일/폴더가 있는 노드 IP
    --source-file, -f   전송할 파일 또는 폴더 경로
    --source-port       source-host 컨트롤 포트 (기본 7000)
    --send-port         데이터 서버 포트 (기본 9000)
    --target-save       모든 대상 노드에서의 저장 디렉토리
    -t, -g, -tg, -norelease, -b   (의미 동일)


[root@fedora2 p2phttp]# ./p2pnode --control --master -p 7000 -h 0.0.0.0 --node-host 192.168.79.9

[CONTROL] 서버 시작
  bind: 0.0.0.0:7000
  mode: MASTER
[MASTER] 자기 자신 등록: 192.168.79.9:7000
[MASTER] 노드 등록: 192.168.79.128:7000 (192.168.79.128)
[MASTER] 노드 등록: 192.168.79.11:7000 (192.168.79.11)
[MASTER] 노드 등록: 192.168.79.12:7000 (192.168.79.12)



root@ubuntu2:~/p2phttp# ./p2pnode --control      --master-host 192.168.79.9 --master-port 7000      --node-host 192.168.79.128      -p 7000 -h 0.0.0.0

[CONTROL] 서버 시작
  bind: 0.0.0.0:7000
  mode: WORKER
[WORKER] 마스터 등록 시도 → 192.168.79.9:7000 as 192.168.79.128:7000
[WORKER] 마스터 등록 성공


[root@fedora p2phttp]# ./p2pnode --control      --master-host 192.168.79.9 --master-port 7000      --node-host 192.168.79.11      -p 7000 -h 0.0.0.0

[CONTROL] 서버 시작
  bind: 0.0.0.0:7000
  mode: WORKER
[WORKER] 마스터 등록 시도 → 192.168.79.9:7000 as 192.168.79.11:7000
[WORKER] 마스터 등록 성공



root@free-VMware-Virtual-Platform:~/p2phttp# ./p2pnode --control      --master-host 192.168.79.9 --master-port 7000      --node-host 192.168.79.12      -p 7000 -h 0.0.0.0

[CONTROL] 서버 시작
  bind: 0.0.0.0:7000
  mode: WORKER
[WORKER] 마스터 등록 시도 → 192.168.79.9:7000 as 192.168.79.12:7000
[WORKER] 마스터 등록 성공


root@ubuntu2:~/p2phttp# ./p2pnode --send-all \
      --master-host 192.168.79.9 --master-port 7000 \
      --source-host 192.168.79.9 --source-port 7000 \
      --source-file /root/openssl-1.1.1w \
      --send-port 9000 \
      --target-save /root/p2phttp/downloads \
      -tg -b

root@ubuntu2:~/p2phttp# ./p2pnode --send-all       --master-host 192.168.79.9 --master-port 7000       --source-host 192.168.79.9 --source-port 7000       --source-file /root/openssl-1.1.1w       --send-port 9000       --target-save /root/p2phttp/downloads  -tg -b -norelease


root@ubuntu2:~/p2phttp# ./p2pnode --send      --source-host 192.168.79.9 --source-port 7000      --source-file /root/buildagent.tar.gz      --send-port 9000      --target-host 192.168.79.12 --target-save /root/p2phttp/downloads      -b

[SEND] 1:1 전송 명령
  src : 192.168.79.9:7000
  dst : 192.168.79.12:7000
  file: /root/buildagent.tar.gz
[SEND] status: 200
{"detail":{"saved":"/root/p2phttp/downloads/buildagent.tar.gz","status":"ok"},"status":"ok"}
root@ubuntu2:~/p2phttp# ./p2pnode --send      --source-host 192.168.79.9 --source-port 7000      --source-file /root/buildagent.tar.gz      --send-port 9000      --target-host 192.168.79.12 --target-save /root/p2phttp/downloads      -tg -b

[SEND] 1:1 전송 명령
  src : 192.168.79.9:7000
  dst : 192.168.79.12:7000
  file: /root/buildagent.tar.gz
[SEND] status: 200
{"detail":{"saved":"/root/p2phttp/downloads/buildagent.tar.gz.tar.gz","status":"ok"},"status":"ok"}
root@ubuntu2:~/p2phttp# ./p2pnode --send      --source-host 192.168.79.9 --source-port 7000      --source-file /root/buildagent.tar.gz      --send-port 9000      --target-host 192.168.79.12 --target-save /root/p2phttp/downloads      -tg -b -norelease
