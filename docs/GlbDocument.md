# GLB 문서와 접근자 판독

`GlbDocument`는 호출자가 메모리로 제공한 [GLB 2.0](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc)을 공용 iiActorObject 코어에서 읽는 불변 객체다. 원본 JSON과 바이너리를 내부에 복사하므로 입력 버퍼는 호출 뒤 해제할 수 있다. 복사한 `GlbDocument`는 같은 불변 자료를 공유한다. `json()`의 뷰는 해당 문서 또는 복사본이 살아 있는 동안 유효하다. 파일 경로를 열거나 외부 URI를 자동으로 해석하지 않는다.

문서 판독 구현은 엔진 소유와 분리된 내부 객체 타깃으로 컴파일되며 JSON 파싱을 위해 `iiJson`을 사용한다. 최종 SDK는 기존과 같은 라이브러리와 `GlbDocument.h` 공개 API를 제공한다.

출력 경로에서는 역바인드 행렬, 문서 JSON과 GLB 청크 포장을 각각 독립 처리기로 조합한다. 처리기들은 호출자가 준비한 자료를 빌려 쓰며 입력 바이너리를 수정하지 않는다. 이 출력 처리기는 최종 라이브러리에 포함되는 내부 타깃이며 `GlbDocument` 판독 API와 별도로 동작한다.

`GlbReadOptions`의 바이트, JSON 항목, 버퍼 뷰, 접근자, 전체 구성값, 희소 엔트리 상한을 호출자가 낮출 수 있다. JSON 파싱은 iiSDK의 `iiJson`을 사용한다. 유효하지 않은 헤더·청크 및 버퍼와 접근자의 참조·범위·정렬·stride, 허용하지 않는 타입과 정렬되지 않은 희소 인덱스는 예외를 반환한다. BIN 안의 선언된 단일 버퍼만 지원하며 외부 파일과 data URI는 거부한다. `bufferView()`는 이미지 등 접근자가 아닌 자료의 읽기 전용 바이트 뷰를 돌려주며 문서 또는 그 복사본이 살아 있는 동안 유효하다. 노드·메시 등 나머지 JSON 구조와 미지원 확장을 포함한 문서의 해석 여부는 상위 소비자가 판단해야 한다.

접근자는 SCALAR, VEC2–VEC4, MAT2–MAT4와 표준 정수·float 구성 타입을 읽는다. 정규화 정수, 인터리브 stride, 희소 덮어쓰기, 바이트·짧은 정수 행렬의 열 패딩을 처리한다. `readFloats()`는 정규화 값을 포함해 `double` 배열을 반환하고 float의 NaN·무한대는 읽을 때 거부한다. `readUnsigned()`는 정규화하지 않은 부호 없는 정수만 반환한다. 각각 원소의 열 우선 구성 순서를 유지한다. 잘못된 인덱스는 `std::out_of_range`, 유효하지 않은 입력 또는 타입은 `std::invalid_argument`를 반환한다.

```cpp
#include <iiActorObject/GlbDocument.h>

const auto document = iiActorObject::GlbDocument::parse(glbBytes);
for (std::size_t i = 0; i < document.accessorCount(); ++i) {
    const auto info = document.accessor(i);
    const auto values = document.readFloats(i);
    // 상위 모듈이 JSON의 노드·메시·리그 의미와 접근자를 연결한다.
}
```

이 객체 자체는 메시, 토폴로지, 리그, 스킨, 머티리얼 또는 애니메이션을 `Object3DAsset`과 `AnimationClip`으로 가져오지 않는다. 본체의 별도 [GLB 자산 입력](GlbImport.md)은 지원되는 메시·리그·스킨·모프를 편집 가능한 에셋으로 변환하고 미지원 자료를 거부하거나 명시적으로 보고한다. `GlbDocument`의 바이너리 판독 성공만으로 전체 캐릭터 왕복 변환이나 대상 앱의 호환성을 보장하지 않는다.

GLB 출력의 바이트 정렬·숫자와 JSON 문자열 변환·버퍼 뷰·희소 모프 접근자 기록은 내부 `Object3DGlbBuffer`가 담당한다. 이 기록기는 장면과 에셋을 소유하지 않는다. 잘못된 버퍼 뷰와 중복되거나 범위를 벗어난 희소 모프 정점은 거부한다. 파일 출력 공개 API는 기존 `exportObject3DGlb`를 사용한다.
