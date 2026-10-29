# 메시 탄젠트 생성과 GLB 교환

`iiActorObject` 본체가 메시·스킨·모프 자료와 탄젠트 계산을 소유한다. NSCharacter는 엔진과 에셋을 참조한다. [MikkTSpace](https://github.com/mmikk/MikkTSpace)의 상용 사용이 허용된 원본 소스를 고정 리비전으로 포함하고 C++23으로 컴파일한다. 출처·라이선스·해시는 [별도 기록](../src/third_party/mikktspace/PROVENANCE.md)에 있다. Autodesk·Unreal·Blender SDK와 Qt는 링크하지 않는다.

## 면별 계산

`calculateMeshCornerTangents`는 `SkeletalMeshLod` 또는 이미 변형된 `DeformedVertex` 배열, 삼각형 인덱스와 정점별 UV를 받는다. 반환 배열은 삼각형 인덱스와 순서가 같은 **면의 각 꼭짓점별** 방향과 부호이다. 같은 원본 정점이라도 거울 UV 또는 불연속 경계에서는 값이 다를 수 있으므로 기존 정점 인덱스에 결과를 덮어쓰면 안 된다. 비탄젠트는 `handedness * cross(normal, direction)`으로 구한다. 입력과 에셋은 변경하지 않는다.

```cpp
#include <iiActorObject/MeshTangents.h>

iiActorObject::MeshTangentOptions options;
options.flipTextureV = true; // 사용할 출력 형식의 UV가 1 - 원본 V일 때
auto corners = iiActorObject::calculateMeshCornerTangents(mesh.geometry, mesh.textureCoordinates, options);
// corners[i]는 mesh.geometry.triangles[i]에 대응한다.
```

노멀은 계산용으로 정규화한다. 모든 값은 유한해야 하며 V 변환을 적용한 뒤의 위치·UV 절댓값은 `1e8` 이하여야 한다. 기본 코너 상한은 800만이며 정점도 800만 개 이하이다. UV 수 불일치, 비어 있거나 3의 배수가 아닌 인덱스, 범위 밖 인덱스, 길이가 0인 노멀, 유효한 직교 기저를 만들 수 없는 결과는 거부한다. 퇴화 UV가 인접 면에서 값을 이어받는 처리는 MikkTSpace의 규칙을 따르며 완전히 정의되지 않은 UV의 시각적 의미까지 보장하지 않는다. 제한은 삼각형 수나 최종 바이트만의 프로세스 전체 메모리 상한이 아니다.

## GLB의 노멀 맵과 모프

GLB에서 가져온 원본 탄젠트와 모프 차이가 유효하면 [원본 탄젠트 계약](GlbAuthoredTangents.md)에 따라 우선 보존한다. 그 외에 노멀 텍스처가 있는 재질은 해당 재질이 선택한 UV 세트로 MikkTSpace를 계산한다. GLB에 기록하는 `1 - V` 좌표에서 계산하며, 면별 탄젠트를 보존하기 위해 해당 primitive의 정점을 코너별로 분리한다. 스킨의 모든 영향, UV 세트와 모프 위치·노멀 델타도 분리된 정점에 복제한다. 원본 탄젠트와 노멀 맵이 모두 없는 primitive는 기존 인덱스를 사용한다. 코너 분리로 GLB 크기가 커질 수 있고, GLB의 정점 번호는 편집 원본 ID의 보존 수단이 아니다.

각 모프 target에 대해서는 해당 target의 위치와 노멀을 적용하여 새 탄젠트를 계산하고, 기준 방향과의 XYZ 차이를 GLB morph `TANGENT`로 기록한다. 개별 target에 대한 표현이며 여러 target을 임의 비율로 섞은 경우의 비선형 재계산 결과와 정확히 같다고 보장하지 않는다. 노멀 맵이 선택한 UV가 재질마다 다르면 각각의 UV에서 계산한다. 모든 입력 면의 기존 삼각분할을 사용하므로, 원본 베이킹에 사용한 사각형 분할이 다르면 탄젠트 공간도 달라질 수 있다.

계산은 면별 800만 코너, 메시 전체 코너 수와 모프 수의 곱 2억 5,600만, 재질별 모프 탄젠트 델타 200만 개로 제한한다. 범위 초과, 유효하지 않은 기저, GLB morph에서 표현할 수 없는 부호 변경 등의 경우 해당 primitive에서 탄젠트와 탄젠트 모프를 생략하고 위치·노멀·UV·스킨·기존 모프는 그대로 출력한다. 이때 primitive의 `extras.iiActorObjectTangentFallback`에 `corner_budget`, `morph_work_budget`, `morph_delta_budget`, `invalid_basis`, `morph_handedness` 중 하나를 기록한다. 노멀 맵이 없어 탄젠트를 계산하지 않은 경우에는 이 진단을 쓰지 않는다. 소비자는 노멀 텍스처 UV로 탄젠트를 재계산해야 한다. [glTF 2.0 명세](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)는 탄젠트가 없을 때 이 방식의 MikkTSpace 계산을 권고한다. 가져가는 앱이 실제로 그렇게 처리하는지는 확인하지 않았다.

[FBX 탄젠트 레이어](FbxTangents.md)는 노멀 맵 UV별 기준 기저를 원본 폴리곤 코너로 투영한다. FBX의 UV에 연결된 원본 탄젠트는 기준 상태와 함께 보존한다. 모프 탄젠트 출력, 모든 PBR 슬롯, 피부 셰이더의 색상 규약 및 CC5·Blender·Unreal 실행 동작의 검증은 남아 있다. C++23 공개 헤더는 패키지에 포함되며 기존 에셋과 엔진은 새 계산을 호출할 때만 사용한다.
