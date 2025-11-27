# p2ptrans
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


