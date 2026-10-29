# 템플릿과 부속 메시의 모프 샘플

`bakeObject3DTemplateMorphs`는 본체 소유 템플릿을 기준 입력과 선택한 목표 입력에서 평가하여 별도의 본체 소유 에셋을 만든다. `NSCharacter::CharacterTemplate::bakeMorphs`는 같은 본체 함수를 호출하는 참조 인터페이스다. 원본 프로그램과 원본 에셋은 수정하지 않는다. 에셋 핸들은 엔진을 소유하지 않으며 엔진이 파괴되면 만료된다.

```cpp
const std::array baseline{iiActorObject::TemplateValue{"height", 0.4}};
const std::array samples{
    iiActorObject::TemplateMorphSample{"height", 0.8, "height_up"}};
auto baked = character.bakeMorphs(baseline, samples);
// baked.object는 내보내기용 에셋이며 baked.channels[0].meshes는 메시 번호 목록이다.
```

각 샘플은 하나의 원본 타깃 또는 의미 조절 입력만 기준값에서 목표값으로 바꾼다. 나머지 입력은 유지한다. 생성된 모프 가중치 `0`은 기준 형상, `1`은 지정 목표 형상이며 채널 변위는 두 형상의 차이다. 입력 범위와 타깃·의미 조절의 합산 규칙은 템플릿 평가와 같다. 기준에서 생략한 입력은 0이다. `adaptMesh`의 `report.baselineValues`는 원본 타깃 가중치이므로 같은 타깃을 구동하는 의미 조절을 추가하면 기여도가 합산된다. 의미 조절을 샘플링할 때는 호스트가 중복 적용을 피하는 기준 입력을 선택해야 한다.

표면 추종 DAG, 접지, 규칙 기반 맞춤을 포함한 템플릿의 전체 평가 결과를 이용하므로 몸뿐 아니라 실제 움직인 눈·치아·혀·의상과 후속 추종 메시에도 같은 이름의 채널을 기록한다. 작성용 점·숨은 보조점과 UV·스킨은 유지하고, 작성용 토폴로지가 없는 렌더 메시도 처리한다. 토폴로지 모프 법선은 변위에서 파생하며 렌더 전용 메시의 법선 차이는 별도로 기록한다. 토폴로지 위치 차이가 `epsilon` 이하이거나 렌더 전용 정점의 위치와 법선 차이가 모두 그 이하이면 해당 변위를 생략한다. 기존 모프는 유지하고 이름 충돌은 기본적으로 거부한다. `replaceExisting=true`를 명시하면 선택한 이름만 교체한다.

반환 채널에는 입력명·채널명·기준값·목표값, 변위가 기록된 메시 번호, 변경된 로컬 bind 관절 번호와 변위 수가 포함된다. FBX와 GLB의 모프는 메시마다 독립적이다. **같은 이름만으로 여러 메시가 자동으로 함께 움직이는 것은 보장되지 않는다.** 호스트가 반환된 메시 목록의 같은 이름 채널에 동일한 정규화 가중치를 전달해야 한다. FBX/GLB 모프는 관절의 bind 프레임을 동적으로 바꾸지 않으므로 `changedJoints`가 비어 있지 않으면 다른 형상에서 포즈를 취할 때 차이가 날 수 있다. 기준 에셋에는 기준 관절이 기록된다.

SDK 런타임에서는 `createObject3DTemplateMorphRig`가 본체 리그 세션을 만들어 같은 채널 이름을 가진 모든 메시의 모프 가중치를 함께 설정한다. 입력 이름은 샘플의 채널명이고 허용 값은 0~1이다. 누락한 named 입력은 0이며 여러 채널을 유지하려면 매번 필요한 값을 모두 전달한다. 프로그램과 결과의 채널·메시 목록이 일치하지 않으면 생성을 거부한다. 다른 에셋에 `cloneFor`로 재연결할 때도 채널이 있는 메시 이름이 일치해야 한다.

```cpp
#include <iiActorObject/Object3DRig.h>
auto actor = baked.object.get().instantiate();
auto rig = iiActorObject::createObject3DTemplateMorphRig(baked);
const std::array controls{iiActorObject::NamedRigControl{"height_up", 1.0}};
rig.get().applyTo(*actor, rig.get().evaluateNamed(controls));
```

이 리그의 기존 `applyTo`는 포즈 관절과 인스턴스 바인드를 건드리지 않으므로 기존 포즈 리그와 함께 사용할 수 있다. 다른 리그가 같은 모프를 쓴다면 마지막으로 적용한 값이 남는다. `applyTo`는 기존 공통 리그 계약을 따르므로 위처럼 해당 에셋에서 만든 액터를 사용한다. 외부 FBX·GLB의 앱 제어기는 생성하지 않는다.

## 형상에 맞춘 런타임 바인드

`applyObject3DTemplateMorphRigWithBind`는 같은 모프 가중치를 적용하면서 원본 템플릿의 전체 기준 입력과 선택된 채널을 합성해 템플릿의 현재 관절을 다시 평가한다. 반환 채널의 기준 입력에 각 `weight * (endpoint - baseline)`을 더한다. 같은 원본 입력에 여러 채널이 있으면 기여를 합산하고, 입력 범위나 원본 드라이버의 합산 제약에 어긋나면 액터를 바꾸지 않고 거부한다.

```cpp
const std::array controls{iiActorObject::NamedRigControl{"height_up", .5}};
iiActorObject::applyObject3DTemplateMorphRigWithBind(rig, *actor, controls);
// actor->pose().locals()는 기존 애니메이션을 유지한다.
// worldMatrices()/skinMatrices()는 새 관절 기준과 역바인드를 반영한다.
```

원본 템플릿과 전체 기준 입력이 살아 있어야 한다. 바인드 적용 시 원본 기준 골격과 리그 대상의 본 ID·이름·부모·바인드가 정확히 일치해야 한다. 이름이나 바인드를 따로 수정한 재연결 대상은 거부한다. 원본 템플릿이나 기준 입력이 없는 리그는 기존 `applyTo`만 사용할 수 있다. 프로젝트 복원 후 원본이 남아 있으면 같은 호출을 사용할 수 있지만, 프로젝트가 실행 중인 액터의 모프값이나 인스턴스 바인드를 자동 저장하지는 않는다.

이 호출은 최초에 기준 템플릿을 평가하고, 합성 입력이 바뀔 때 현재 템플릿 그래프를 다시 평가한다. 본체 리그의 불변 출처를 공유하는 복제 세션은 기준 관절과 가장 최근의 합성 입력·관절 결과를 공유하며, 액터의 포즈와 모프값은 각 인스턴스에 남는다. 같은 입력은 캐시를 재사용한다. 원본 템플릿이나 원본 에셋의 명시적 제거는 캐시가 있어도 계속 검사한다. 대형 캐릭터에서 입력이 연속적으로 바뀌는 실시간 처리 비용은 별도로 측정해야 한다. 여러 메시의 형상 자체는 여전히 베이크된 선형 모프이므로 비선형 형상 계산이나 여러 채널 혼합에서 정확한 템플릿 표면과 다를 수 있다. 관절은 합성된 템플릿 입력으로 재계산한다. `Pose`의 인스턴스 바인드를 변경하므로 이후 기존 `applyTo`를 호출해도 바인드는 유지된다. 원래 바인드는 `actor->pose().clearDeformationBind()`로 복원한다. 바인드의 자세·IK 제한과 C++ API 계약은 [캐릭터 런타임](Character.md)을 따른다.

이 기능은 iiActorObject 런타임의 팔레트 계산이다. FBX·GLB로 인스턴스의 동적 바인드 제어기를 내보내거나 CC5·Blender·Unreal에서 같은 동작을 검증한 것은 아니다.

구성별 검사와 인체 표본의 수치 범위는 [동적 바인드 검증 기록](DynamicMorphBindVerification-20261028.md)에 정리한다.

`inspectObject3DTemplateMorphRig(rig)`는 원본 템플릿의 비소유 핸들과 기준 입력·원래 입력명·끝점·출력 채널명·메시 이름·변경된 관절의 원본 ID와 당시 이름·희소 변위 기록 수·베이크 설정을 반환한다. 관절 이름과 ID는 같은 순서이며, 재연결한 에셋의 현재 관절 목록이 아니라 샘플 생성 시점의 출처이다. 이 값은 이후 편집한 에셋이 원본 템플릿과 동일하다는 보증이 아니다. `baselineInputs=nullopt`와 `bakeOptions=nullopt`는 해당 출처를 알 수 없다는 뜻이며, 알려진 빈 입력 목록이나 기본 설정과 구분한다. 원본 템플릿을 명시적으로 제거하면 핸들은 만료되지만 모프 리그와 출처 데이터는 남는다.

호스트가 이 리그를 `exportObject3DProject(editor, rigs)`에 포함하면 프로젝트 버전 21이 본체 에셋과 리그 설정을 저장한다. 원본 템플릿이 살아 있고 기준 입력과 베이크 설정이 알려져 있으면, 저장된 채널의 입력·끝점·이름을 `TemplateMorphSample`로 구성하여 같은 설정으로 다시 베이크할 수 있다. 원본이 이미 제거되었다면 없는 상태로 기록한다. 서로 다른 리그가 같은 설정을 사용하면 그 설정은 공유하며 각각의 연결 에셋과 원본 템플릿 참조는 별도로 보존한다. 임의 사용자 백엔드는 여전히 거부한다. 자세한 제한은 [프로젝트 계약](Object3DProject.md)을 따른다. `TemplateMorphBakeResult`에 출처 필드가 추가되었으므로 C++ 소비자는 같은 버전의 헤더와 라이브러리로 다시 빌드해야 한다.

복제한 리그 세션은 불변 출처와 메시 이름 목록을 공유하지만, 각 평가 결과와 액터의 포즈·모프 값은 독립적이다. 프로젝트를 복원할 때 같은 설정과 원본 템플릿의 조합은 한 번 구성하고 각 대상 에셋의 채널 연결은 따로 검증한다.

```cpp
auto info = iiActorObject::inspectObject3DTemplateMorphRig(restored.rigs.at(0));
if (info.templateProgram.valid() && info.program.baselineInputs && info.program.bakeOptions) {
    std::vector<iiActorObject::TemplateMorphSample> samples;
    for (const auto& channel : info.program.channels)
        samples.push_back({channel.input, channel.endpoint, channel.name});
    auto rebuilt = iiActorObject::bakeObject3DTemplateMorphs(
        info.templateProgram, *info.program.baselineInputs, samples, *info.program.bakeOptions);
}
```

샘플은 각 목표점에서 정확한 위치 차이를 기록하지만, 삼각형 프레임 회전·곡선 조절·충돌 맞춤·접지와 같은 비선형 과정의 중간값 또는 여러 채널 혼합은 선형 모프만으로 정확히 재현하지 못할 수 있다. 다른 기준이나 목표를 원하면 별도로 베이크해야 한다. 본 기능은 자동 생성 프로그램 자체를 교체하거나 외부 앱의 제어 UI를 생성하지 않는다.

`maximumSamples`, `maximumPointVisits`, `maximumDeltas`는 샘플 수와 비교·출력량을 제한한다. 각 템플릿의 맞춤 계산과 렌더 모프에도 별도 한도가 적용된다. 알 수 없거나 중복된 입력, 잘못된 범위, 중복 채널명, 기존 이름 충돌 및 자원 한도 초과 시 실패하고 새 에셋을 등록하지 않는다. 완성된 결과는 일반 FBX·GLB 내보내기에서 사용할 수 있다. 실제 CC5·Blender·Unreal 실행과 시각 검증은 현재 작업 범위에 포함하지 않는다.
