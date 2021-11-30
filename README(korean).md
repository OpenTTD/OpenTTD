# OpenTTD

## 목차

- 1.0) [정보](#10-정보)
    - 1.1) [OpenTTD 다운로드](#11-openttd-다운로드)
    - 1.2) [OpenTTD 게임 플레이 매뉴얼](#12-OpenTTD-게임-플레이-매뉴얼)
    - 1.3) [지원 플랫폼](#13-지원-플랫폼)
    - 1.4) [OpenTTD 설치 및 실행](#14-openttd-설치-및-실행)
    - 1.5) [추가 컨텐츠 / 모드](#15-추가-컨텐츠--모드)
    - 1.6) [OpenTTD 디렉터리](#16-openttd-디렉터리)
    - 1.7) [OpenTTD 컴파일](#17-openttd-컴파일)
- 2.0) [연락 및 커뮤니티](#20-연락-및-커뮤니티)
    - 2.1) [OpenTTD에 기여하기](#21-openttd에-기여하기)
    - 2.2) [버그 리포팅](#22-버그-리포팅)
    - 2.3) [번역](#23-번역)
- 3.0) [라이센스](#30-라이센스)
- 4.0) [크레딧](#40-크레딧)

## 1.0) 정보


OpenTTD는 Chris Sawyer가 만든 Transport Tycoon Deluxe라는 유명한 게임을 기반으로 한 수송 시뮬레이션 게임입니다.
OpenTTD는 새로운 기능을 확장하면서, 원작 게임을 최대한 가깝게 모방한 게임입니다.

OpenTTD는 GNU General Public License 2.0 버전에 기반하지만, 일부 다른 라이선스들 또한 포함합니다.
자세한 내용은 아래의 ["Licensing"](#30-licensing) 를 참조하세요.

## 1.1) OpenTTD 다운로드

OpenTTD는 [공식 OpenTTD 웹사이트](https://www.openttd.org/)에서 다운받을 수 있습니다.

'안정'버전과 'nightly'버전 모두 다운 가능합니다.

- 대부분의 사람들은 '안정'버전이 더 광범위하게 테스트되었기 때문에 이를 선택해야 합니다.
- 'nightly'버전에는 최신 변경 사항과 기능이 포함되어 있지만 가끔 불안정적일 수 있습니다.

OpenTTD는 또한 [Steam](https://store.steampowered.com/app/1536610/OpenTTD/), [GOG.com](https://www.gog.com/game/openttd), 및 [Microsoft Store](https://www.microsoft.com/p/openttd-official/9ncjg5rvrr1c) 에서도 무료로 사용할 수 있습니다. 일부 플랫폼에서는 OS패키지 관리자 또는 유사한 서비스를 통해 OpenTTD를 사용할 수 있습니다.

## 1.2) OpenTTD 게임 플레이 매뉴얼

OpenTTD에는 게임 플레이 매뉴얼과 팁이 있는 [커뮤니티 관리 wiki](https://wiki.openttd.org/)가 있습니다.


## 1.3) 지원 플랫폼

OpenTTD는 여러 플랫폼과 운영 체제를 지원합니다.

현재 지원하는 플랫폼은 다음과 같습니다.

- Linux (SDL (OpenGL and non-OpenGL))
- macOS (universal) (Cocoa)
- Windows (Win32 GDI / OpenGL)

다른 플랫폼들(특히 다양한 BSD 시스템들) 또한 작동할 수 있지만, 우리는 이를 적극적으로 테스트하거나 유지 관리하지 않습니다.

### 1.3.1) 이전 플랫폼 지원

플랫폼들, 언어들 및 컴파일러들이 변경됩니다.
우리는 누군가가 이전 플랫폼에 관심이 있는 한 이전 플랫폼에 대한 지원을 계속할 것입니다. 하지만, 언어 및 컴파일러 기능을 유지하기 위해 프로젝트를 진행할 수 없는 경우는 예외입니다.

OpenTTD의 모든 개정판은 이전 모든 개정판의 저장 게임을 로드할 수 있음을 보장합니다 (저장 게임이 손상된 경우는 제외).
로드되지 않는 저장 게임을 발견한다면 버그 리포팅을 부탁드립니다.

## 1.4) OpenTTD 설치 및 실행

OpenTTD는 일반적으로 설치가 간단하지만, 더 많은 도움이 필요하다면 wiki [설치 가이드를 포함](https://wiki.openttd.org/en/Manual/Installation)가 있습니다.

OpenTTD를 실행하려면 몇 가지 추가 그래픽과 사운드 파일이 필요합니다.

일부 플랫폼에서는 필요한 경우에 설치 과정 도중 다운로드됩니다.

일부 플랫폼에서는 [설치 가이드](https://wiki.openttd.org/en/Manual/Installation)를 참조해야 합니다.

### 1.4.1) 무료 그래픽 및 사운드 파일

그래픽용 OpenGFX, 사운드용 OpenSFX 및 음악용 OpenMSX로 분리된 무료 데이터 파일을 아래에서 찾을 수 있습니다.

- https://www.openttd.org/downloads/opengfx-releases/ OpenGFX
- https://www.openttd.org/downloads/opensfx-releases/ OpenSFX
- https://www.openttd.org/downloads/openmsx-releases/ OpenMSX

이 패키지들의 README 파일의 설치 절차를 따라주세요.
Windows installer는 선택적으로 위의 패키지들을 다운로드할 수 있습니다.

### 1.4.2) 원본 Transport Tycoon Deluxe 그래픽 및 사운드 파일

원본 Transport Tycoon Deluxe  데이터 파일로 플레이하려면 CD-ROM에서 baseset/ 디렉터리로 데이터 파일을 복사해야 합니다.
Transport Tycoon Deluxe의 DOS 버전에서 복사하든지 Windows 버전에서 복사하든지 상관 없습니다.
Windows 설치는 선택적으로 이 파일들을 복사할 수 있습니다.

당신은 아래의 파일들을 복사해야 합니다.
- sample.cat
- trg1r.grf 또는 TRG1.GRF
- trgcr.grf 또는 TRGC.GRF
- trghr.grf 또는 TRGH.GRF
- trgir.grf 또는 TRGI.GRF
- trgtr.grf 또는 TRGT.GRF

### 1.4.3) 원본 Transport Tycoon Deluxe 음악

Transport Tycoon Deluxe 음악을 원하시면, 원본 게임의 해당 파일을 baseset 폴더에 복사하세요.
- Windows용 TTD: gm/ 폴더의 모든 파일 (gm_tt00.gm ~ gm_tt21.gm)
- DOS용 TTD: GM.CAT 파일
- Transport Tycoon Original: GM.CAT 파일을 GM-TTO.CAT 으로 이름을 바꿔서 저장하세요.

## 1.5) 추가 컨텐츠 / 모드

OpenTTD는 게임플레이를 다른 방향으로 수정할 수 있는 추가 콘텐츠를 다양한 타입으로 제공합니다.

대부분의 추가 콘텐츠는 OpenTTD 내에서 메인 메뉴의 'Check Online Content' 버튼을 통해 다운로드 할 수 있습니다.

추가 콘텐츠를 수동으로 설치할 수도 있지만, 이는 더 복잡합니다. [OpenTTD wiki](https://wiki.openttd.org/) 또는 [OpenTTD 디렉터리 구조 가이드](./docs/directory_structure.md)에서 도움을 제공할 수 있습니다.




### 1.5.1) AI 상대

OpenTTD는 AI 상대들 없이 제공되므로 AI들과 함께 플레이를 원한다면 다운로드해야 합니다.

가장 쉬운 방법은 메인 메뉴의 'Check Online Content' 버튼을 이용하는 것입니다.

자신의 플레이 스타일에 맞는 AI들을 선택할 수 있습니다.

AI 도움말 및 토론들은 [포럼의 AI 섹션](https://www.tt-forums.net/viewforum.php?f=65)에서 찾을 수 있습니다.

### 1.5.2) 시나리오 및 하이트맵

시나리오들 및 하이트맵들은 메인 메뉴의 'Check Online Content' 버튼을 통해 추가할 수 있습니다.

### 1.5.3) NewGRFs

차량, 산업, 정거장, 조경 개체, 도시 이름 등을 포함한 광범위한 추가 콘텐츠들을 NewGRF로 사용할 수 있습니다.

NewGRF는 메인 메뉴의 'Check Online Content' 버튼을 통해 추가할 수 있습니다.

[NewGRFs 가이드](https://wiki.openttd.org/en/Manual/NewGRF) 와 [포럼 그래픽 개발 섹션](https://www.tt-forums.net/viewforum.php?f=66) 또한 참고하세요.

### 1.5.4) 게임 스크립트

게임 스크립트는 표준 OpenTTD 게임 플레이의 추가 과제 또는 변경 사항을 제공할 수 있습니다. ( ex) 운송 목표 설정, 도시 성장 행동 변경)

게임 스크립트는 메인 메뉴의 'Check Online Content' 버튼을 통해 추가할 수 있습니다.

[게임 스크립트 가이드](https://wiki.openttd.org/en/Manual/Game%20script) 와 [포럼 그래픽 게임 스크립트 섹션](https://www.tt-forums.net/viewforum.php?f=65)또한 참고하세요.
 
### 1.6) OpenTTD 디렉터리

OpenTTD는 자체 디렉터리 구조를 사용하여 게임 데이터, 추가 콘텐츠 등을 저장합니다.

자세한 내용은 [디렉터리 구조 가이드](./docs/directory_structure.md)를 참고하세요.

### 1.7) OpenTTD 컴파일

소스에서 OpenTTD를 컴파일하려면, [COMPILING.md](./COMPILING.md)에서 지침을 찾을 수 있습니다.

## 2.0) 연락 및 커뮤니티

'공식' 채널들

- [OpenTTD 홈페이지](https://www.openttd.org)
- irc.oftc.net의 #openttd를 통한 IRC 채팅 [irc channel의 추가 정보](https://wiki.openttd.org/en/Development/IRC%20channel)
- 코드 저장소와 이슈 보고를 위한 [Github의 OpenTTD](https://github.com/openTTD/)
- [forum.openttd.org](https://forum.openttd.org/) - OpenTTD 및 관련 게임 토론을 위한 주된 커뮤니티 포럼 사이트
- [OpenTTD 위키](https://wiki.openttd.org/) - 게임 플레이 가이드, 일부 게임 메커니즘에 대한 자세한 설명, 추가 콘텐츠 사용 방법 등을 포함한 커뮤니티에서 관리하는 위키 

'비공식' 채널들

- OpenTTD 위키에는 영어 이외의 단어를 포함해 [OpenTTD 커뮤니티들을 수집한 페이지](https://wiki.openttd.org/en/Community/Community)가 있습니다.


### 2.1) OpenTTD에 기여하기

저희는 OpenTTD에 기여할 분들을 환영합니다. 기여자들을 위한 자세한 정보는 [CONTRIBUTING.md](./CONTRIBUTING.md)에서 확인할 수 있습니다.

### 2.2) 버그 리포팅

좋은 버그 보고들은 매우 도움이 됩니다. 이에 도움이 되는 [버그 보고 가이드](./CONTRIBUTING.md#bug-reports)가 있습니다.

멀티플레이어의 비동기화는 디버그 및 보고가 복잡합니다(일부 소프트웨어 개발 기술이 요구됨).
지침들은 [비동기화 디버그 및 보고](./docs/debugging_desyncs.md)에서 찾을 수 있습니다.


### 2.3) 번역

OpenTTD는 여러 언어들로 번역됩니다. 번역은 [온라인 번역 도구](https://translator.openttd.org)를 통해 추가 및 업데이트됩니다.

## 3.0) 라이센스

OpenTTD는 GNU General Public License 2.0 에 기반합니다.
전체 라이센스 텍스트는 '[COPYING.md](./COPYING.md)'파일을 참조하세요.
이 라이센스는 아래 명시된 경우를 제외하고 이 배포판의 모든 파일에 적용됩니다.

`src/3rdparty/squirrel` 의 다람쥐 구현은 Zlib 라이센스에 기반합니다.
전체 라이센스 텍스트는 `src/3rdparty/squirrel/COPYRIGHT`를 참조하세요.

`src/3rdparty/md5`의 md5 구현은 Zlib 라이센스에 기반합니다.
전체 라이센스 텍스트는 `src/3rdparty/md5`의 소스 파일의 주석을 참조하세요.

`src/3rdparty/os2`의 OS/2를 위한 Posix `getaddrinfo` 와 `getnameinfo` 구현에서 일부는 GNU General License 2.1에 기반하고, 일부는 (3-clause) BSD 라이센스에 기반합니다.
정확한 라이센스 조건은 `src/3rdparty/os2/getaddrinfo.c` resp. `src/3rdparty/os2/getnameinfo.c` 에서 찾을 수 있습니다.

`src/3rdparty/fmt`의 fmt 구현은 MIT 라이센스에 기반합니다.
전체 라이센스 텍스트는 `src/3rdparty/fmt/LICENSE.rst`를 참고하세요.

## 4.0 크레딧

[CREDITS.md](./CREDITS.md) 를 참고하세요.
