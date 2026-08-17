# E2E 테스트 (실제 브라우저)

```bash
npm i -D puppeteer@23
# 헤드리스 크롬 공유 라이브러리가 없는 환경:
#   apt-get download libnss3 libnspr4 ... && dpkg-deb -x *.deb root/
#   export LD_LIBRARY_PATH=$PWD/root/usr/lib/x86_64-linux-gnu
python3 -m http.server 7000 --directory ../ &
node e2e.js
```

28건 검증: 렌더링·한글조합·멀티탭·롱프레스·겹받침·▶확정·레이아웃전환·
E.161영어·숫자·기호팔레트·한자변환·M1가드·물리키보드·콘솔에러
