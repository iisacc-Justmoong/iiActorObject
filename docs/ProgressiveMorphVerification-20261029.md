# 점진 모프 교환·리그 검증 기록

## 대상과 근거

[점진 모프 계약](ProgressiveMorphRig.md)의 정적 FBX 입력, 중간 형상 제어, 본체 편집과 프로젝트 저장을 검사했다. `resources/fixtures/fbx/progressive-morph-ascii.fbx`는 프로젝트에서 직접 작성한 합성 자료이다. 상용 애플리케이션 코드나 에셋을 포함하지 않는다.

전용 정적 FBX 출력은 형상을 편집하고 프로젝트 버전 31로 저장·복원한 뒤 실행했다. 별도 Python FBX 7.4 판독기가 채널 기본값 15%, 다섯 full weight, Shape 연결과 편집된 정점·법선 배열을 확인한다. ufbx 평가와 SDK에서 다시 가져온 리그의 여러 보간·외삽값 및 정점 위치를 비교한다. 위치 편집에서 계산한 법선과 명시적 법선의 복원도 확인한다. 선형 모프 기본값·골격·DQ 스키닝이 공존하는 경우, 한 타깃을 두 점진 제어가 함께 사용하는 경우와 단일 100% 키를 검사한다. 동일한 두 메시의 공유 지오메트리·스킨과 하나의 공유 제어는 외부 FBX 판독기 및 본체 재입력으로 확인한다. 기본값 충돌, 외부 문서 옵션, 내용이나 스킨이 달라진 인스턴스, 공유 집합에 포함된 일반 모프, 일부만 겹치는 메시 집합, 동일 타깃의 다중 원본 레코드, 제어점에 투영할 수 없는 경계 법선, 다른 에셋의 리그 및 손상된 SDK 마커는 거부한다.

독립 ufbx 평가와 SDK 리그를 같은 입력값에서 비교했다. 음수·양수 full weight, 정확한 키, 중립, 키 사이와 키 범위 밖을 확인하고 각 타깃 유효 가중치와 변형 정점 위치를 비교한다. ufbx의 FBX 평가와 일치한다는 뜻이며 대상 앱의 동작을 확인했다는 뜻은 아니다.

입력은 기본 제어값, 원본 채널과 형상 이름, 여러 메시에 공유된 한 채널, 이름이 같은 서로 다른 채널, 100%가 아닌 단일 타깃, 정점·명시적 법선 자료와 스킨이 연결된 합성 골격을 다룬다. 포즈가 설정된 액터에 리그를 적용해 기존 관절 포즈가 유지됨을 확인했다. 선형과 DQ 스키닝의 정점 결과, 중간 형상 편집 후 리그 재연결, undo·redo와 프로젝트 복원도 검사한다. 0·중복 키, 누락 타깃, 잘못된 바인딩, 비유한 값·오버플로, 이름·모프 예산, 충돌하는 SDK 메타데이터와 참조 만료는 거부한다. 애니메이션 테이크가 있는 입력은 이 경로가 이를 가져오지 않는다는 경고를 확인한다.

프로젝트 버전 31의 외부 Python 판독은 헤더·길이·CRC와 채널·키·메시 바인딩의 실제 바이트를 확인한다. 체크섬을 다시 계산한 뒤 존재하지 않는 타깃이나 다른 메시로 바꾼 프로그램도 복원을 거부하며, 실패 전의 엔진 에셋·리그와 다음 객체 ID를 유지한다. 이 검사는 형식과 경계의 확인이지 악의적인 작성자를 인증하는 기능이 아니다.

## 수행 결과

macOS arm64의 C++23 Release에서 관련 기존 FBX·프로젝트·리그 선별 회귀 90/90이 통과했다. 그 후 최종 소스의 FBX 입력·리그 선별 회귀는 NSCharacter 포함 및 본체 전용에서 각각 13/13 통과했다. 점진 모프 검사 7건은 두 구성에서 각각 7/7 통과했다. DNA 지원을 켠 구성은 점진 모프 검사 7건과 합성 DNA 리그·점진 리그를 함께 저장하고 복원하는 검사 1건이 모두 통과했다. 합성 DNA는 제작용 MetaHuman 검증이 아니다. Debug ASan·UBSan 구성의 선별 검사도 7/7 통과했다. 누수 검출은 비활성화했다. 별도의 설치 소비자는 본체 전용과 NSCharacter 포함 구성이 각각 1/1 통과했다. 모든 구성의 애플리케이션 실행 검증 옵션은 꺼져 있다.

정적 단일 메시 출력 추가 당시에는 NSCharacter 포함 Release의 FBX·모프 법선·프로젝트 선별 회귀 94/94, 본체 전용 Release의 점진 모프 9/9와 FBX 애니메이션·법선 회귀 16/16이 통과했다. NSCharacter 포함 Debug ASan·UBSan은 점진 모프 9/9가 통과했고 누수 검출은 비활성화했다. `build/` 안의 별도 설치 위치에 설치한 뒤 본체 전용과 NSCharacter 포함 소비자 검사가 각각 1/1 통과했다. 공유 라이브러리의 직접 링크에서 Qt는 발견되지 않았다. 앱 실행은 수행하지 않았다.

공유 메시 지원을 추가한 최종 소스에서는 NSCharacter 포함 Release의 관련 FBX·법선·프로젝트 회귀 90/90, 본체 전용 Release의 점진 모프 9/9 및 NSCharacter 포함 Debug ASan·UBSan의 점진 모프 9/9가 통과했다. 공유 스킨을 포함한 최종 점진 모프 검사는 NSCharacter 포함 Release에서도 9/9 통과했다. 별도 설치된 본체 전용과 NSCharacter 포함 소비자의 공유 메시 출력·재입력은 각각 1/1 통과했다. ASan 누수 검출은 활성화하지 않았다.

기록은 `build/glb-import-ninja/progressive-regression.xml`, 두 Release 빌드의 `progressive-final-regression.xml`과 `progressive-morph-final.xml`, `build/progressive-morph-dna.xml`, `build/glb-import-full-asan/progressive-morph-final.xml`, 두 소비자의 `progressive-consumer.xml`이다. 별도 설치 경로는 프로젝트의 `build/` 안이며 사용자 설치본은 변경하지 않았다. 검사한 공유 라이브러리의 직접 링크는 시스템 C++ 런타임과 시스템 라이브러리이며 Qt 링크는 없다.

정적 출력 추가 후 결과는 `build/glb-import-ninja/progressive-export-regression.xml`, `build/core-tangent-ninja/progressive-export.xml` 및 `progressive-export-related.xml`, `build/glb-import-full-asan/progressive-export.xml`, `build/consumer-corrective-animation-{full,core}/progressive-export.xml`에 기록했다.

공유 메시 추가 후 결과는 `build/glb-import-ninja/progressive-shared-regression.xml`, 세 구성의 `progressive-shared-final.xml`, `build/consumer-corrective-animation-{full,core}/progressive-shared.xml`에 기록했다.

## 미완 범위

점진 제어 애니메이션의 추가 입출력·적용은 [별도 계약](ProgressiveMorphAnimation.md)과 검증 기록을 따른다. 서로 다른 메시가 공유하는 채널, 동일 타깃의 다중 원본 레코드 및 서로 다른 경계 법선의 점진 출력은 남아 있다. 일반 FBX·GLB 출력은 중간 형상을 독립 타깃으로만 기록한다. CC5·Blender·Unreal 앱 실행과 실제 가져오기·시각 품질은 사용자의 지시에 따라 검사하지 않았다. 이 결과는 전체 캐릭터 생성·편집 목표나 제작용 MetaHuman 호환의 완료를 증명하지 않는다.
