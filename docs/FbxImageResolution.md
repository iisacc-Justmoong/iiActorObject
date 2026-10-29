# FBX 이미지 해석과 변환

## 소유권과 입력 경계

FBX의 기본색과 노멀 이미지 자원은 `iiActorObject`의 `Object3DEngine`이 소유한다. `NSCharacter`는 이 본체의 에셋을 참조하며 이미지 저장소를 별도로 만들지 않는다. 입력 파서는 외부 파일을 열지 않는다. 내장 PNG는 원본 바이트를 검사하여 저장하고, 외부 이미지 또는 내장된 PNG 외 이미지는 호스트가 `FbxImportOptions::imageResolver`를 지정했을 때만 요청한다. 제공하지 않거나 해석기가 `std::nullopt`를 반환하면 해당 이미지 연결은 제외되고 경고가 남는다.

호스트는 `<iiActorObject/FbxImageResolver.h>`만 포함해 이미지 요청과 해석기 인터페이스를 구현할 수 있다. 기존 `Object3DInterchange.h`도 이 선언을 계속 제공한다. FBX 장면의 이미지·텍스처와 재질 해석은 `Object3DFbxResources` 내부 객체 타깃이 수행하며, 가져오기 작업은 그 결과를 본체 에셋 자료에 반영한다. 이 타깃은 장면과 호스트 해석기를 빌려 쓰고 에셋이나 엔진을 소유하지 않는다.

`FbxImageRequest`의 이름과 경로는 FBX가 제공한 신뢰할 수 없는 문자열이다. `filename`, `absoluteFilename`, `relativeFilename`은 접근 권한을 뜻하지 않는다. 호스트는 자신이 허용한 패키지·파일·자원만 해석해야 한다. `embeddedBytes`가 비어 있지 않으면 내장 이미지의 원본 바이트이며, 비어 있으면 외부 참조이다. 요청의 문자열과 바이트 뷰는 동기 `resolve` 호출이 끝나면 무효가 된다. 해석기는 가져오기가 끝날 때까지 살아 있어야 한다.

해석기는 `FbxResolvedImage`에 검증 가능한 PNG 바이트와 `image/png` MIME을 반환한다. 선택적인 `notice`는 변환이나 손실을 가져오기 경고에 표시한다. 반환된 MIME·PNG 구조·CRC·크기와 진단 문자열을 검사한다. 같은 Video에 연결된 동일 자료는 한 이미지로 공유한다. 해석기의 예외나 잘못된 반환값, 예산 초과는 전체 가져오기를 실패시키며 엔진에 부분 에셋을 등록하지 않는다.

`maxImageBytes`는 보관할 고유 PNG 바이트의 합을 제한하며 내장 원본도 변환 전 남은 예산을 넘을 수 없다. `maxImageResolverRequests`는 기본 4,096회이며 그보다 큰 값은 거부한다. 이 예산은 동시에 필요한 디코딩 버퍼와 호스트가 소유한 외부 원본을 합친 프로세스 전체 메모리 상한은 아니다.

## 선택형 iiVisualCodec 어댑터

`IIACTOROBJECT_WITH_VISUALCODEC=ON`으로 빌드하면 별도 정적 컴포넌트 `iiActorObject::VisualCodec`을 사용할 수 있다. `FbxExternalImageSource`를 구현해 외부 바이트를 제공하고 `FbxVisualImageResolver`에 빌려준다. 내장 PNG 외 자료는 소스를 호출하지 않고 어댑터에 직접 전달한다. 어댑터는 PNG를 그대로 전달하고, 코덱이 지원하는 JPEG·TGA 등의 입력은 디코딩하여 PNG로 저장한다. 변환한 원본 인코딩 바이트는 에셋에 보관하지 않으며, 방향과 색상 프로파일의 처리는 사용 중인 iiVisualCodec의 정책을 따른다. 원본 셰이더 그래프·색공간 규약과 시각적 동등성을 보장하는 기능은 아니다.

기본 `FbxVisualImageLimits`는 입력 64 MiB, 디코딩 픽셀 256 MiB, 한 축 16,384 픽셀이다. 개별 한도와 가져오기의 남은 이미지 예산을 함께 검사하고 PNG 출력은 최대 64 MiB로 제한한다. 외부 소스가 요청한 최대치를 준수하더라도 어댑터에서 다시 검사한다. 각 지원 형식의 실제 가용성은 연결된 iiVisualCodec 빌드에 달려 있다.

```cpp
#include <iiActorObject/FbxVisualImageResolver.h>

class PackageImages final : public iiActorObject::FbxExternalImageSource {
public:
    std::optional<std::vector<std::uint8_t>> read(
        const iiActorObject::FbxImageRequest& request, std::size_t maximumBytes) const override {
        // 호스트가 승인한 패키지에서 request.relativeFilename을 검색하고,
        // maximumBytes 이하의 원본만 반환한다. 없으면 std::nullopt를 반환한다.
        return lookupApprovedPackageEntry(request.relativeFilename, maximumBytes);
    }
};

PackageImages images;
iiActorObject::FbxVisualImageResolver resolver(images);
iiActorObject::FbxImportOptions options;
options.imageResolver = &resolver;
auto result = iiActorObject::importObject3DFbx(engine.reference(), fbxBytes, options);
// 호출자는 result.warnings를 확인한다.
```

`lookupApprovedPackageEntry`, `engine`, `fbxBytes`는 호스트가 제공하는 예시이다. 패키지 소비자는 `find_package(iiActorObject CONFIG REQUIRED COMPONENTS VisualCodec)` 및 `target_link_libraries(target PRIVATE iiActorObject::VisualCodec)`을 사용한다. 이 컴포넌트는 iiVisualCodec과 해당 빌드의 이미지 코덱에 의존한다. 이 옵션을 끄면 본체는 코덱에 의존하지 않으며 호스트가 직접 PNG를 제공하는 `FbxImageResolver`를 구현할 수 있다. Qt는 사용하지 않는다.

정적 iiVisualCodec 설치본은 그 설치본을 빌드할 때 사용한 이미지 라이브러리와 ABI가 맞아야 한다. 이 머신에서는 설치된 코덱이 JPEG ABI 80을 기대하므로 ABI 62 라이브러리를 선택하면 JPEG 처리가 실패한다. 검증 구성은 ABI 80인 jpeg-turbo를 명시적으로 선택했다. 다른 머신에서는 해당 설치본의 빌드와 맞는 의존성을 사용해야 하며, 이 프로젝트가 특정 머신의 경로를 강제하지 않는다.

`FbxImportOptions`의 공개 C++ 구조가 확장되었으므로 기존 바이너리는 같은 헤더와 라이브러리로 다시 빌드해야 한다. 이 작업의 빌드와 별도 설치 검사는 프로젝트 `build/` 아래에 한정하며 사용자의 `~/.local` 설치본을 자동으로 변경하지 않는다.
