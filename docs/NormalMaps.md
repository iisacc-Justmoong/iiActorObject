# 공용 3D 노멀 맵

`iiActorObject`의 `Object3DData::images`가 불변 PNG 바이트를 보관하고, 각 `Object3DMaterial`의 `normalImage`, `normalUvSet`, `normalScale`이 탄젠트 공간 노멀 텍스처를 참조한다. `NSCharacter`는 캐릭터의 코어 에셋을 빌려 사용하며 별도의 이미지나 엔진을 소유하지 않는다. 이 데이터 모델은 실제 이미지를 제작하거나 렌더링하지 않는다.

```cpp
auto data = object.get().data();
data.images.emplace_back("SkinNormal", std::move(pngBytes));
auto &material = data.materials.at(0);
material.normalImage = data.images.size() - 1;
material.normalUvSet = "SkinUV"; // 빈 문자열은 해당 메시의 첫 UV 세트
material.normalScale = 0.7;
editor.get().replace(std::move(data), "Skin normal map");
```

이미지 인덱스는 에셋의 이미지 목록을 가리킨다. 선택한 이름은 해당 머티리얼을 쓰는 각 메시의 세트에 존재해야 한다. 노멀 이미지가 연결된 메시에는 UV 좌표가 필요하다. 이름을 지정하려면 노멀 이미지도 연결해야 하며 스케일은 유한한 단정도 범위여야 한다. 기본값은 1이고 음수도 허용한다. 이미지와 UV 선택은 편집 레이어의 이산 값이며 레이어 가중치가 양수이면 적용된다. 스케일은 연속 값으로 레이어 가중치에 따라 보간된다. 외부 편집 병합은 스케일 차이와 UV 선택을 반영하고, 이미지 변경은 명시적 자원 편집이 필요하므로 거부한다.

프로젝트는 현재 에셋이나 보존 이력의 레이어에 노멀 이미지·UV 이름·기본값이 아닌 스케일이 있으면 버전 18 이상을 사용한다. 명명 점 선택과 함께 저장할 수 있다. 버전 1~21을 읽으며 기준 법선 보정이 있으면 버전 19 이상이 필요하다. 노멀 맵과 상위 버전 자료가 없으면 이전 버전 선택을 유지한다. 비활성 레이어를 포함한 복원은 이미지 참조와 UV 연결을 검증하며, 재생성 결과와 스냅샷이 다르면 기존 엔진 상태를 변경하지 않는다.

GLB는 `normalTexture`의 이미지 인덱스, `texCoord` 및 `scale`을 기록한다. 같은 이름의 UV 세트가 메시마다 다른 순서에 있거나 기본 색상과 노멀 텍스처가 다른 세트를 참조할 때는 필요한 머티리얼 변형을 만든다. GLB 명세의 노멀 RGB는 선형 탄젠트 공간 값이다. [메시 탄젠트 계약](MeshTangents.md)에 따라 노멀 텍스처의 UV로 면별 MikkTSpace를 계산하고 표현 가능한 모프 탄젠트도 기록한다. 한도나 표현 제한 때문에 탄젠트를 생략하는 경우에는 가져가는 쪽의 재계산에 의존한다. 앱이나 렌더러의 실제 동작은 검사하지 않았다.

FBX는 내장 PNG를 `Video`로 저장하고 `Texture`의 `NormalMap` 연결과 UV 세트 이름을 기록한다. 노멀 맵이 사용하는 UV에는 가능한 경우 [FBX 탄젠트와 비탄젠트](FbxTangents.md)를 기록하며, 표현할 수 없는 기저나 모프의 기준 형상만 기록하는 경우에는 상태를 남긴다. ufbx가 제공하는 노멀 맵과 표준 FBX `NormalMap`을 읽는다. [호스트 이미지 해석기](FbxImageResolution.md)를 지정하면 외부 PNG를 제공하거나 내장·외부 JPEG와 TGA 등을 PNG로 변환할 수 있다. 해석기가 없는 경우에는 그런 이미지를 건너뛰고 경고한다. 비활성 노멀 연결, 지원하지 않는 래핑과 UV 변환은 가져오지 않으며 경고한다. ufbx가 별도 변환으로 해석하지 않는 레거시 `ModelUVTranslation`, `ModelUVScaling`, `Cropping`도 DOM에서 확인하여 기본값이 아니거나 구조가 모호하면 연결을 제외한다. 이미지 바이트는 자원 예산에 포함된다.

FBX의 스케일은 iiActorObject 전용 `iiNormalScale` 속성에 저장하므로, iiActorObject가 다시 읽을 때만 보존된다는 보장이 있다. 이 속성이 숫자가 아니거나 표현 범위를 벗어나면 입력을 거부한다. 외부 FBX 도구가 이 속성을 적용하거나 보존한다고 가정할 수 없다. FBX 입력의 다른 셰이더 전용 스케일, 채널 변환, DirectX/OpenGL 그린 채널 규약, 텍스처 색 공간 메타데이터는 자동 변환하지 않는다. 이 밖의 PBR 텍스처 슬롯과 완전한 셰이더 그래프도 별도 작업이다.

`Object3DMaterial`의 공개 구조가 바뀌었으므로 이 API를 사용하는 기존 C++ 바이너리는 같은 버전의 헤더와 라이브러리로 다시 빌드해야 한다. 저장소의 `build/`에 준비한 설치본은 사용자의 `~/.local` 설치본을 자동으로 바꾸지 않는다.
