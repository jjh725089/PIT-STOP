# PIT-STOP

**실시간 낙상 감지 및 안전 이송 경로 최적화 시스템**  

## 프로젝트 개요

**목표**:  
실시간 낙상 감지와 군중 혼잡도 기반의 최적 이송 경로 안내 시스템 구축

**핵심 기술**:  
YOLO 기반 AI 모델, CCTV 실시간 영상 분석, 경로 탐색 알고리즘, 실시간 UI 대시보드

**동작 원리**:  
낙상 발생 -> 프레임 캡처 및 분석 -> 혼잡도 계산 -> 경로 산출 -> 알림 전송 및 시각화

## 시스템 명칭 및 의미

> **PIT-STOP**  
> _People Instant Tracking & Safe Transit Optimization Platform_  

F1의 피트스탑처럼, 신속하고 정밀한 구조 대응을 지향하는 시스템

## 구조

![System Architecture](images/system%20architecture.png)

**Main Server**  
낙상 위치 계산, 전체 혼잡도 및 경로 산출, 경로 추천 결과 송출

**Sub Server (x3)**  
각 출입구 외부 모니터링, 인원 수 탐지, 혼잡도 계산 후 Main Server로 전송

**Client Dashboard**  
실시간 이벤트 알림, 영상 스트리밍, 혼잡도 시각화, 음성 안내 전송, CSV 로그 export

**통신 프로토콜**  
MQTT (이벤트/명령 송수신), RTSP (영상 스트리밍), FTP (프레임 이미지 전송), TCP/IP (음성 송수신)

## 주요 기능

**낙상 감지**:
YOLOv8 기반 객체 탐지 -> Aspect Ratio로 정상/낙상 분류

**군중 탐지 및 혼잡도 분석**:
YOLOv5 기반 객체 수 추출 -> 지수 감쇠 함수로 가중치 반영한 grid 기반 밀집도 맵 생성

**경로 탐색**:
낙상 위치, 혼잡도, 출구까지 거리 및 외부 인원 수 -> A* 휴리스틱 기반 경로 점수 계산 -> 최적 경로 선택

**알림**:
LED 점등, 마이크 방송, 팝업 알림, 이벤트 로그 기록

**Dashboard**:
로그인/회원관리, 이벤트 로그 검색 및 Export, 실시간 그래프 및 영상 스트리밍 통합 표시

## 기술 스택

**AI**: YOLOv8n (낙상 감지), YOLOv5s (군중 탐지), ONNX Runtime

**통신**: MQTT over TLS, RTSP over TLS, FTP, TCP/IP

**영상 처리**: GStreamer, OpenCV

**보안**: OpenSSL

**UI**: Qt

**DB**: SQLite

**하드웨어**: Raspberry Pi 4B, Hanwha Vision PNO-A9081R, Pi Camera Module V2, LED, 마이크/스피커

## 팀 소개

**이현지(팀장)**: Client Dashboard 구현

**목경민**: Main Server, Sub Server 구현

**조정현**: 실시간 스트리밍 구현, 왜곡 보정 구현, AWB 구현

**황주호**: 낙상 감지, 군중 탐지, 혼잡도 분석, 경로 탐색, 시각화, 음성 안내 모듈 구현

**김대환 책임(멘토)**: 기술 자문

## 프로젝트 회고

**잘한 점**

MQTT + TLS 기반 통신 설계 및 안정화

멀티스레드 서버 아키텍처 구현

통합 테스트 및 자동화 도구 설계 경험

**아쉬운 점**

Git 브랜치 전략 및 커밋 관리 미흡

협업 도구(Jira 등) 활용 부족

**한계 및 개선 방향**

장애물 없는 실내 환경 기준으로 동작 -> 장애물 위치 반영 경로 탐색 알고리즘 도입

사람 외 객체 인식 불가 -> 레이더, 센서 등 외부 장치 연동
