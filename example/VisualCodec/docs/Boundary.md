# FBX 이미지 코덱 조합 경계

이 독립 소비자는 iiActorObject의 `FbxImageResolver` 인터페이스와 iiVisualCodec의
이미지 코덱을 함께 사용한다. FBX의 파일 경로를 직접 열지 않고 호출자가 제공한
`FbxExternalImageSource`에서만 승인된 바이트를 받는다. 입력·출력 크기와 이미지
치수 한도 및 PNG 검증을 유지한다.

현재 전환 준비 단계이며 기존 iiActorObject의 선택형 컴포넌트도 그대로 남아 있다.
두 구현을 같은 앱에서 동시에 연결하지 않는다. 독립 소비자의 빌드·테스트와 설치
소비자 검증 및 기존 빌드 종료 확인 후 본체의 연결을 제거해야 한다.

소비자는 `find_package(iiActorObjectVisualCodec CONFIG REQUIRED)` 후
`iiActorObject::VisualCodec`을 사용한다. 앱이 두 SDK와 어댑터를 조합하며 기존의
`find_package(iiActorObject COMPONENTS VisualCodec)`은 이 별도 패키지로 이전한다.

2026-10-28 macOS arm64에서 검증용 actor 설치본과 기존 iiVisualCodec을 조합한
독립 프로젝트의 FBX 이미지 검사 5개 및 별도 설치 소비자 검사 1개가 통과했다.
첫 검사에서는 PNG 프레임워크의 런타임 검색 경로가 없어 5개가 실패했고, 기존
설치본과 맞는 정적 PNG 및 JPEG 라이브러리를 명시한 뒤 같은 검사가 통과했다.
검증용 설치는 actor 프로젝트 `build/` 아래에만 수행했다. Windows·Linux 실행과
실제 제작 앱의 가져오기, 전체 actor 모듈의 SDK 독립화는 확인하지 않았다.
