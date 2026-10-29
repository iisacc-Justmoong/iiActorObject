# FBX 골격·표정 애니메이션 출력

`exportObject3DFbx(asset, clip, options)`는 본체가 소유한 골격·메시와 하나의 `AnimationClip`을 FBX 7.4 바이너리에 작성한다. 기존 애니메이션 없는 `exportObject3DFbx(asset)`와 별도 API이다. C++23과 표준 라이브러리만 사용하며 애플리케이션 SDK나 Qt를 연결하지 않는다. 본체의 `Object3DEngine`이 자산과 골격을 소유하고 NSCharacter는 이를 참조한다. 출력 바이트의 저장은 호스트가 담당한다.

클립과 에셋은 동일한 `Skeleton` 인스턴스를 사용해야 한다. 본 ID는 바뀌지 않으며 부모 인덱스의 순서는 제한하지 않는다. 기준 자세 프로그램이 있는 원본이나 인스턴스별 변형 바인드를 요구하는 클립은 기존의 정적 FBX 및 GLB 출력과 같은 이유로 거부한다. 먼저 기준 형상과 바인드를 별도 에셋으로 구체화한 후 그 골격에 맞춰 클립을 생성해야 한다. 트랙이 없는 본은 기본 바인드 변환을 사용한다. 내보낼 본 트랙이나 명시적으로 연결한 모프 곡선이 하나 이상 필요하다.

## 회전과 시간

SDK의 위치·스케일은 선형 보간하고 회전은 최단 경로 쿼터니언 보간한다. FBX는 XYZ 오일러 각도의 축별 선형 곡선으로 쓴다. 원본 키, 시작과 끝, 최대 샘플 간격에 맞춘 시간에 변환을 채집하고 각도의 동등한 표현을 이어 붙인다. 결과 축 곡선을 1/4, 1/2, 3/4 지점에서 원본 회전과 비교하며 `maximumRotationError`를 넘는 구간을 이분한다. 키에서도 float32 변환 후 오차를 검사한다. 이는 **유한한 검사 지점에서의 오차 한도**이며 그 사이의 연속 구간 전체나 다른 앱의 보간 방식에 대한 수학적 보장은 아니다. 엄격한 오차 또는 특이점 때문에 한도 내 표현이 불가능하면 출력하지 않는다.

`maximumSampleInterval`의 기본값은 1/30초, `maximumRotationError`의 기본값은 0.001라디안이다. 오차의 허용 범위는 `1e-7`~π라디안이다. `maximumSamples`의 기본값은 250,000이고 1~1,000,000 범위여야 한다. 시간 샘플은 본별로 계산하고 같은 모프 이름이 여러 메시의 채널에 연결되면 채널별로 각각 계산한다. 최대 샘플 수나 64회의 분할 한도를 초과하면 실패한다. FBX 시간은 초당 46,186,158,000틱의 signed 64비트이다. 키 시각이나 클립 양 끝이 같은 틱으로 겹치거나 범위를 넘으면 거부한다. 위치·회전·스케일 및 모프 값은 float32이므로 반올림된다. 표현할 수 없는 값과 0으로 소실되는 스케일·비영 모프 값도 거부한다.

## 스칼라와 모프

`AnimationClip`의 스칼라 곡선은 일반 데이터이므로 기본적으로 임의로 모프라고 가정하지 않는다. `exportMatchingMorphCurves=true`를 명시하면 곡선 이름과 정확히 같은 논리 모프 이름을 찾고, 모든 해당 FBX 블렌드 셰이프 채널에 연결한다. 여러 메시 또는 같은 메시의 분리된 모프 레코드도 각 채널에 연결한다. `indexedBindings`는 곡선·메시·논리 모프 인덱스를 직접 지정하므로 동명 모프를 메시별로 다른 곡선에 연결할 수 있다. 한 논리 모프의 여러 작성 레코드는 그 메시의 같은 곡선에 연결된다. 명시된 곡선은 이름으로 다시 자동 연결하지 않으며 타깃 충돌과 잘못된 인덱스는 거부한다. 입력 대응은 최대 1,000,000개다. 원본 스칼라 1은 FBX `DeformPercent` 100에 대응하며 값은 클램프하지 않는다. 선형 키와 시작·끝의 유지 구간을 보낸다. 정적 geometry 옵션은 `options.geometry`로 그대로 전달한다. 외부 문서 토큰으로 동일 이름 레코드를 묶는 동작도 기존 정적 출력 계약과 같다.

일치하지 않는 스칼라 곡선은 기본적으로 오류이다. `allowDiscardScalarCurves=true`인 경우에만 연결되지 않는 스칼라 곡선을 제외한다. 이 선택은 알려진 데이터 손실이며 결과에 골격 또는 모프 트랙이 하나도 없으면 여전히 오류이다. 이 일반 `exportObject3DFbx` 오버로드는 한 클립을 출력하며 재질·리그 제어·constraint 곡선의 의미 변환과 제작용 리그 연결을 구현하지 않았다. 점진 리그를 포함한 복수 테이크 출력은 [전용 경로](ProgressiveMorphAnimation.md)를 따른다. [별도의 FBX 애니메이션 가져오기](FbxAnimationImport.md)는 여러 테이크의 본과 표정 곡선을 표본화한다. 기존 정적 FBX 가져오기는 애니메이션을 읽지 않고 경고한다.

```cpp
#include <iiActorObject/Animation.h>
#include <iiActorObject/Object3DInterchange.h>

iiActorObject::FbxAnimationExportOptions options;
options.exportMatchingMorphCurves = true;
options.maximumSampleInterval = 1.0 / 60.0;
auto bytes = iiActorObject::exportObject3DFbx(asset, clip, options);
// 호스트가 bytes를 저장한다.
```

## 검증 경계

점진 모프 리그 제어와 기존 골격·일반 모프를 함께 출력하는 오버로드는 [점진 모프 애니메이션 계약](ProgressiveMorphAnimation.md)을 따른다.

독립 Python 이진 파서는 FBX 애니메이션 그래프·키 시간·선형 곡선·채널 연결을 확인한다. 별도 MIT ufbx 판독기는 FBX를 읽고 서로 다른 계층 순서, 180도 경계, 양·음의 90도 특이점, 시작·끝 유지, 세 축 회전, 이동·스케일, 여러 메시의 모프를 독립 평가한다. 이들은 형식과 수치 검사이며 CC5·Blender·Unreal의 실제 가져오기, 리그 편집·연결, 시각적 변형 품질이나 MetaHuman 호환을 입증하지 않는다. 사용자의 지시에 따라 이번 작업에서 앱 실행 검증은 하지 않는다.
