# Pinned MIT OpenRigLogic, private C++23 object code; no upstream configure/downloads.
set(_iiao_rl_root "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/openriglogic")
set(_iiao_rl_sources
    "${_iiao_rl_root}/src/dna/LODConstraint.cpp"
    "${_iiao_rl_root}/src/dna/LODMapping.cpp"
    "${_iiao_rl_root}/src/dna/Reader.cpp"
    "${_iiao_rl_root}/src/dna/Writer.cpp"
    "${_iiao_rl_root}/src/dna/filters/AnimatedMapFilter.cpp"
    "${_iiao_rl_root}/src/dna/filters/BlendShapeFilter.cpp"
    "${_iiao_rl_root}/src/dna/filters/JointFilter.cpp"
    "${_iiao_rl_root}/src/dna/filters/MeshFilter.cpp"
    "${_iiao_rl_root}/src/dna/stream/BinaryStreamReaderImpl.cpp"
    "${_iiao_rl_root}/src/dna/stream/BinaryStreamWriterImpl.cpp"
    "${_iiao_rl_root}/src/dna/stream/FilteredBinaryInputArchive.cpp"
    "${_iiao_rl_root}/src/dna/stream/JSONStreamReaderImpl.cpp"
    "${_iiao_rl_root}/src/dna/stream/JSONStreamWriterImpl.cpp"
    "${_iiao_rl_root}/src/dna/stream/StreamReader.cpp"
    "${_iiao_rl_root}/src/dna/stream/StreamWriter.cpp"
    "${_iiao_rl_root}/src/dna/types/CoordinateSystemConverter.cpp"
    "${_iiao_rl_root}/src/dna/version/DNAVersionInfo.cpp"
    "${_iiao_rl_root}/src/pma/MemoryResource.cpp"
    "${_iiao_rl_root}/src/pma/resources/AlignedMemoryResource.cpp"
    "${_iiao_rl_root}/src/pma/resources/ArenaMemoryResource.cpp"
    "${_iiao_rl_root}/src/pma/resources/DefaultMemoryResource.cpp"
    "${_iiao_rl_root}/src/riglogic/animatedmaps/AnimatedMaps.cpp"
    "${_iiao_rl_root}/src/riglogic/animatedmaps/AnimatedMapsFactory.cpp"
    "${_iiao_rl_root}/src/riglogic/animatedmaps/AnimatedMapsImpl.cpp"
    "${_iiao_rl_root}/src/riglogic/animatedmaps/AnimatedMapsImplOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/animatedmaps/AnimatedMapsNull.cpp"
    "${_iiao_rl_root}/src/riglogic/animatedmaps/AnimatedMapsNullOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/animatedmaps/AnimatedMapsOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/blendshapes/BlendShapes.cpp"
    "${_iiao_rl_root}/src/riglogic/blendshapes/BlendShapesFactory.cpp"
    "${_iiao_rl_root}/src/riglogic/blendshapes/BlendShapesImpl.cpp"
    "${_iiao_rl_root}/src/riglogic/blendshapes/BlendShapesImplOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/blendshapes/BlendShapesNull.cpp"
    "${_iiao_rl_root}/src/riglogic/blendshapes/BlendShapesNullOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/blendshapes/BlendShapesOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/conditionaltable/ConditionalTable.cpp"
    "${_iiao_rl_root}/src/riglogic/controls/Controls.cpp"
    "${_iiao_rl_root}/src/riglogic/controls/ControlsFactory.cpp"
    "${_iiao_rl_root}/src/riglogic/controls/ControlsInputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/controls/instances/StandardControlsInputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/JointBehaviorFilter.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/Joints.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/JointsBuilder.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/JointsEvaluator.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/JointsFactory.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/JointsNullEvaluator.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/JointsNullOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/JointsOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/CPUJointsBuilder.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/CPUJointsEvaluator.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/CPUJointsOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/bpcm/BPCMCalculationStrategy.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/bpcm/BPCMJointsBuilder.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/bpcm/BPCMJointsBuilderFactory.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/bpcm/BPCMJointsEvaluator.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/ml/MLJointsBuilder.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/ml/MLJointsBuilderFactory.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/quaternions/QuaternionCalculationStrategy.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/quaternions/QuaternionJointsBuilder.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/quaternions/QuaternionJointsBuilderFactory.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/quaternions/QuaternionJointsEvaluator.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/twistswing/TwistSwingJointsBuilderFactory.cpp"
    "${_iiao_rl_root}/src/riglogic/joints/cpu/utils/JointGroupOptimizer.cpp"
    "${_iiao_rl_root}/src/riglogic/ml/MachineLearnedBehavior.cpp"
    "${_iiao_rl_root}/src/riglogic/ml/MachineLearnedBehaviorEvaluator.cpp"
    "${_iiao_rl_root}/src/riglogic/ml/MachineLearnedBehaviorFactory.cpp"
    "${_iiao_rl_root}/src/riglogic/ml/MachineLearnedBehaviorNullEvaluator.cpp"
    "${_iiao_rl_root}/src/riglogic/ml/MachineLearnedBehaviorNullOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/ml/MachineLearnedBehaviorOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/ml/cpu/CPUMachineLearnedBehaviorEvaluator.cpp"
    "${_iiao_rl_root}/src/riglogic/ml/cpu/CPUMachineLearnedBehaviorFactory.cpp"
    "${_iiao_rl_root}/src/riglogic/ml/cpu/CPUMachineLearnedBehaviorOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/ml/cpu/Operation.cpp"
    "${_iiao_rl_root}/src/riglogic/psdnet/PSDNet.cpp"
    "${_iiao_rl_root}/src/riglogic/psdnet/PSDNetFactory.cpp"
    "${_iiao_rl_root}/src/riglogic/psdnet/PSDNetImpl.cpp"
    "${_iiao_rl_root}/src/riglogic/psdnet/PSDNetImplOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/psdnet/PSDNetNull.cpp"
    "${_iiao_rl_root}/src/riglogic/psdnet/PSDNetNullOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/psdnet/PSDNetOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/rbf/RBFBehavior.cpp"
    "${_iiao_rl_root}/src/riglogic/rbf/RBFBehaviorEvaluator.cpp"
    "${_iiao_rl_root}/src/riglogic/rbf/RBFBehaviorFactory.cpp"
    "${_iiao_rl_root}/src/riglogic/rbf/RBFBehaviorNullEvaluator.cpp"
    "${_iiao_rl_root}/src/riglogic/rbf/RBFBehaviorNullOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/rbf/RBFBehaviorOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/rbf/cpu/AdditiveRBFSolver.cpp"
    "${_iiao_rl_root}/src/riglogic/rbf/cpu/CPURBFBehaviorOutputInstance.cpp"
    "${_iiao_rl_root}/src/riglogic/rbf/cpu/InterpolativeRBFSolver.cpp"
    "${_iiao_rl_root}/src/riglogic/rbf/cpu/RBFSolver.cpp"
    "${_iiao_rl_root}/src/riglogic/riglogic/RigInstanceImpl.cpp"
    "${_iiao_rl_root}/src/riglogic/riglogic/RigLogicImpl.cpp"
    "${_iiao_rl_root}/src/riglogic/version/RLVersionInfo.cpp"
    "${_iiao_rl_root}/src/status/Provider.cpp"
    "${_iiao_rl_root}/src/status/Registry.cpp"
    "${_iiao_rl_root}/src/status/Status.cpp"
    "${_iiao_rl_root}/src/status/Storage.cpp"
    "${_iiao_rl_root}/src/trio/Concepts.cpp"
    "${_iiao_rl_root}/src/trio/Stream.cpp"
    "${_iiao_rl_root}/src/trio/streams/FileStreamImpl.cpp"
    "${_iiao_rl_root}/src/trio/streams/MemoryMappedFileStream.cpp"
    "${_iiao_rl_root}/src/trio/streams/MemoryMappedFileStreamFallback.cpp"
    "${_iiao_rl_root}/src/trio/streams/MemoryMappedFileStreamUnix.cpp"
    "${_iiao_rl_root}/src/trio/streams/MemoryMappedFileStreamWindows.cpp"
    "${_iiao_rl_root}/src/trio/streams/MemoryStreamImpl.cpp"
    "${_iiao_rl_root}/src/trio/streams/StreamStatus.cpp"
)
add_library(iiActorObjectRigLogic OBJECT ${_iiao_rl_sources})
target_compile_features(iiActorObjectRigLogic PRIVATE cxx_std_23)
# ScopedPtr.h relies on a transitive <type_traits> include removed by modern libc++.
# Supply the standard header without modifying the pinned upstream sources.
target_precompile_headers(iiActorObjectRigLogic PRIVATE <type_traits>)
target_include_directories(iiActorObjectRigLogic PRIVATE "${_iiao_rl_root}/include" "${_iiao_rl_root}/src")
target_compile_definitions(iiActorObjectRigLogic PRIVATE
    RL_DISABLE_RUNTIME_FEATURE_DETECTION
    RL_BUILD_WITH_XYZ_ROTATION_ORDER RL_BUILD_WITH_XZY_ROTATION_ORDER
    RL_BUILD_WITH_YXZ_ROTATION_ORDER RL_BUILD_WITH_YZX_ROTATION_ORDER
    RL_BUILD_WITH_ZXY_ROTATION_ORDER RL_BUILD_WITH_ZYX_ROTATION_ORDER)
set_target_properties(iiActorObjectRigLogic PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
if(MSVC)
    target_compile_options(iiActorObjectRigLogic PRIVATE /w)
else()
    target_compile_options(iiActorObjectRigLogic PRIVATE -w)
endif()
target_sources(iiActorObject PRIVATE $<TARGET_OBJECTS:iiActorObjectRigLogic>
    src/iiActorObject/Object3DDnaPreflight.cpp)
target_include_directories(iiActorObject SYSTEM PRIVATE "${_iiao_rl_root}/include" "${_iiao_rl_root}/src")
