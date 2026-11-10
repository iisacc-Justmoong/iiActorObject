# Build and resource layout

Windows SDK의 `min`·`max` 매크로가 활성화된 상태에서도 공개 헤더의 표준
함수와 극한값 호출이 매크로로 바뀌지 않도록 보호한다. 소비자의 매크로 설정은
변경하지 않으며, 테스트 빌드에 이 포함 순서의 컴파일 전용 검사를 둔다.
이 항목은 소스 구성에 대한 기록이며 Windows 빌드 성공을 의미하지 않는다.

Compile the module, its agent-facing tool and tests in the repository `build/`
directory. Character-authoring example executables are opt-in through
`IIACTOROBJECT_BUILD_EXAMPLE`; generated character assets belong under
`resources/`, not the build tree. The example and external applications are
not executed by default.

Some legacy asset-generation, format and installed-consumer tests still use
`build/` for fixtures or invoke other applications. Their isolation is a
separate migration; clean-layout verification must select tests that do not
create those files or launch external applications. Existing project assets and
source snapshots recovered from prior builds are retained under ignored
`resources/.recovery/` paths until reviewed.
The earlier `tools/` directory contained a separate Unreal Engine Git checkout
and Blender assets; those were preserved together in
`resources/.recovery/actor-history-20261031/tools/` rather than treated as
rebuildable compiler output.

macOS 12 배포 타깃에서도 USD 부동소수점 값을 내보낼 수 있도록 표준
클래식 로캘과 `max_digits10` 정밀도를 사용한다. 직렬화 결과는 프로세스
로캘과 무관하며 유한한 `double` 값을 다시 읽었을 때 보존한다. 같은
규칙을 리그 샘플 카탈로그의 JSON 숫자 직렬화에도 적용한다.
구형 Apple 배포 타깃에서 실수 `from_chars`를 사용할 수 없다면 엄격한
십진 문법과 클래식 로캘을 사용하는 파서로 전환한다. 정수 파싱은 기존
`from_chars` 경로를 유지하며, 리그 샘플 에이전트 도구도 동일한 실수
파서를 사용한다.
