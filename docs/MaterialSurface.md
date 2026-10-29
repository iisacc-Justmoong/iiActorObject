# 캐릭터 재질의 알파와 양면 설정

공용 3D 본체의 `Object3DMaterialSurface`는 캐릭터의 피부·눈·속눈썹·머리카락·의상에 적용할 알파 방식, 임계값, 양면 여부를 보관한다. 이미지와 재질은 `Object3DEngine`이 소유한 에셋에 있고 NSCharacter는 본체 에셋을 참조한다. 렌더러와 실제 셰이더는 호스트의 책임이다.

```cpp
using namespace iiActorObject;
Object3DMaterialSurface hair;
hair.alphaMode = Object3DAlphaMode::Mask;
hair.alphaCutoff = .35;
hair.doubleSided = true;
const bool changed = editor.get().setMaterialSurface(materialIndex, hair);
```

`Automatic`은 기존 동작과 같이 기본색 알파가 1 미만이거나 기본색 이미지가 있으면 `Blend`, 아니면 `Opaque`로 해석한다. `effectiveAlphaMode(material)`로 실제 선택을 조회한다. `Opaque`는 알파를 무시하고, `Mask`는 기본색과 이미지 알파를 곱한 값이 `alphaCutoff` 이상일 때만 불투명하게 취급하며, `Blend`는 알파 혼합을 뜻한다. 임계값 기본값은 0.5이고 유한한 0 이상의 float 범위이다. 1보다 큰 값도 허용하며 마스크 면을 모두 투명하게 만들 수 있다. `doubleSided`는 후면 컬링과 후면 조명 처리에 사용하는 명시적 데이터다. 정확한 렌더링은 [Khronos glTF 재질 규약](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/schema/material.schema.json)에 맞게 호스트가 구현해야 한다.

편집기는 같은 값에 이력이나 revision을 추가하지 않는다. 잘못된 재질 번호·모드·임계값은 실패하며 현재 에셋을 유지한다. 템플릿 편집에서는 다른 재질 변경과 함께 레이어에 남는다. 레이어 강도가 양수일 때 알파 방식과 양면 여부는 이산적으로 적용하고 임계값은 강도에 따라 보간한다. undo·redo와 템플릿 재생성은 본체 이력을 사용한다. NSCharacter의 동일 메서드는 본체 세션으로 위임한다.

## 저장과 교환

GLB 입력은 `OPAQUE`, `MASK`, `BLEND`, `alphaCutoff`, `doubleSided`를 보존한다. `OPAQUE`에서도 원래 기본색 알파와 이미지 알파 데이터는 남지만 그 재질의 렌더링에는 사용하지 않는다. GLB 출력은 명시된 방식을 표준 재질 속성으로 내보낸다. 기본 `Automatic`은 기존 출력과 같은 방식으로 해석한다. 마스크가 아닌 방식의 임계값은 기록할 수 있지만 해당 방식의 렌더링에는 사용하지 않는다.

본체 프로젝트는 기본값과 다른 표면 설정이 에셋 또는 편집 이력에 존재할 때 버전 29 이상으로 저장한다. 다중 후손 기준 자세가 함께 있으면 버전 30을 사용한다. 이전 프로젝트는 `Automatic`, 임계값 0.5, 단면으로 읽고 기존 투명 처리 방식을 유지한다. C++ 재질 구조가 확장되므로 헤더와 라이브러리를 함께 다시 빌드해야 한다.

FBX의 표준 불투명도에는 표현 가능한 불투명도만 출력한다. 마스크 모드·임계값·양면 여부와 Opaque의 원본 알파는 SDK 전용 재질 속성으로 함께 기록하고 SDK가 다시 읽을 수 있다. 표준 불투명도가 이 보조 정보와 1e-6을 넘게 다르면 낡은 자료로 판단하여 기본 가져오기를 거부한다. `FbxImportOptions::restoreAuthoringMetadata=false`로 다시 읽으면 경고와 함께 SDK 전용 재질 표면 속성을 무시하고 표준 불투명도를 사용한다. 이때 알파 방식·임계값·양면 설정은 기본값으로 돌아가며 일부 형상 작성용 확장도 복원하지 않는다. 다른 앱이 이 속성으로 같은 재질이나 셰이더를 구성한다는 보장은 없다. 특히 FBX의 일반 불투명도는 GLB의 마스크 임계값과 동등하지 않다.

DNA 교환은 이 재질 자료의 보존 형식이 아니다. DNA에 없는 재질과 텍스처는 호스트가 별도로 관리하고 연결해야 한다.

외부 편집 병합은 기준 재질의 유효 알파 방식·임계값·양면 설정을 확인하고 변경을 적용한다. 같은 설정이 로컬과 외부에서 다르게 변경된 경우 기본적으로 충돌을 거부하며 명시적 덮어쓰기 정책을 따른다. GLB를 거친 `Automatic`의 유효 방식은 비교하되, 변경이 없다면 원래의 `Automatic` 설정을 유지한다.

이 기능은 재질 의미를 기록하고 교환하기 위한 것이며 피부 산란, 제작용 머리카락 셰이더 또는 대상 앱에서의 시각적 일치를 보장하지 않는다. 사용자 지시에 따라 CC5·Blender·Unreal 앱은 실행하지 않는다.
