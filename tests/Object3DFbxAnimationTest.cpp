#include <iiActorObject/Animation.h>
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
template<class F> void rejects(F f)
{
    bool caught = false;
    try { f(); } catch (const std::exception &) { caught = true; }
    CHECK(caught);
}
Quaternion product(Quaternion a,Quaternion b)
{
    return Quaternion{a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
        a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}.normalized();
}

Object3DData fixtureData()
{
    Object3DData data; data.name = "AnimatedFixture";
    // Stable bone IDs intentionally do not follow hierarchy order.
    data.bones = {{"root", NoBone, {}}, {"tip", 2, {{0,1,0},{},{1,1,1}}},
                  {"joint", 0, {{0,1,0},{},{1,1,1}}}};
    Object3DMesh mesh; mesh.name = "body";
    mesh.geometry.vertices = {{{0,0,0},{0,0,1},{{0,1}}},
        {{1,0,0},{0,0,1},{{1,1}}}, {{0,1,0},{0,0,1},{{2,1}}}};
    mesh.geometry.triangles = {0,1,2};
    mesh.geometry.morphTargets = {{"Smile", {{0,{.1,0,0},{}}}}};
    data.meshes.push_back(mesh); mesh.name = "eyes"; data.meshes.push_back(std::move(mesh));
    return data;
}
Object3DData logicalFixtureData()
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}}};
    auto &body=data.meshes[0];body.name="body";
    for(auto &point:body.topology->points)point.weights={{0,1}};
    body.topology->morphTargets={{"fold",{{0,{0,.1,0}}}},{"fold",{{1,{0,.2,0}}}}};
    auto garment=body;garment.name="garment";garment.topology->morphTargets.resize(1);
    data.meshes.push_back(std::move(garment));return data;
}

AnimationClip motion(const std::shared_ptr<const Skeleton> &skeleton, bool unknown = false, bool negative = false)
{
    auto root = skeleton->bones()[0].bindTransform, rootEnd = root;
    root.rotation = Quaternion::fromAxisAngle({0,0,1}, 170*std::numbers::pi/180);
    rootEnd.rotation = Quaternion::fromAxisAngle({0,0,1}, 190*std::numbers::pi/180);
    rootEnd.position.x = 2;
    auto joint = skeleton->bones()[2].bindTransform, jointEnd = joint;
    jointEnd.rotation = Quaternion::fromAxisAngle({1,2,3}, 2.5);
    jointEnd.scale = {2,1.5,.5};
    auto tip = skeleton->bones()[1].bindTransform, tipEnd = tip;
    const auto roll=Quaternion::fromAxisAngle({1,0,0},20*std::numbers::pi/180);
    const auto yaw=Quaternion::fromAxisAngle({0,0,1},35*std::numbers::pi/180);
    const double sign=negative?-1.:1.;
    tip.rotation = product(yaw,product(Quaternion::fromAxisAngle({0,1,0},sign*80*std::numbers::pi/180),roll));
    tipEnd.rotation = product(yaw,product(Quaternion::fromAxisAngle({0,1,0},sign*100*std::numbers::pi/180),roll));
    std::vector<AnimationCurve> curves{{"Smile", {{0,.25},{2,.75}}}};
    if (unknown) curves.push_back({"Unmapped", {{0,0},{2,1}}});
    return AnimationClip(skeleton, 2, {{0,{{0,root},{2,rootEnd}}},
        {1,{{.25,tip},{1.75,tipEnd}}}, {2,{{0,joint},{2,jointEnd}}}}, std::move(curves));
}

FbxAnimationExportOptions settings()
{
    FbxAnimationExportOptions options;
    options.exportMatchingMorphCurves = true;
    options.maximumSampleInterval = .1;
    options.maximumRotationError = .0005;
    return options;
}
void emit(const std::vector<std::uint8_t> &bytes)
{
    constexpr char hex[]="0123456789abcdef";
    for(const auto byte:bytes)std::cout<<hex[byte>>4]<<hex[byte&15];
    std::cout<<'\n';
}

void validation()
{
    Object3DEngine engine; const auto asset = engine.createObject(fixtureData());
    const auto clip = motion(asset.get().skeleton()); auto options = settings();
    const AnimationClip scoped(asset.get().skeleton(),2,{},{{"Left",{{0,.1},{2,.9}}},{"Right",{{0,.8},{2,.2}}}});
    const std::array indexed{AnimationMorphBinding{0,0,0},AnimationMorphBinding{1,1,0}};
    FbxAnimationExportOptions scopedOptions;scopedOptions.indexedBindings=indexed;
    CHECK(!exportObject3DFbx(asset.get(),scoped,scopedOptions).empty());
    const std::array badCurve{AnimationMorphBinding{2,0,0}};
    scopedOptions.indexedBindings=badCurve;rejects([&]{(void)exportObject3DFbx(asset.get(),scoped,scopedOptions);});
    const std::array badMesh{AnimationMorphBinding{0,2,0}};
    scopedOptions.indexedBindings=badMesh;rejects([&]{(void)exportObject3DFbx(asset.get(),scoped,scopedOptions);});
    const std::array badTarget{AnimationMorphBinding{0,0,1}};
    scopedOptions.indexedBindings=badTarget;rejects([&]{(void)exportObject3DFbx(asset.get(),scoped,scopedOptions);});
    const std::array repeated{AnimationMorphBinding{0,0,0},AnimationMorphBinding{1,0,0}};
    scopedOptions.indexedBindings=repeated;rejects([&]{(void)exportObject3DFbx(asset.get(),scoped,scopedOptions);});
    const AnimationClip conflict(asset.get().skeleton(),2,{},{{"Left",{{0,0},{2,1}}},{"Smile",{{0,1},{2,0}}}});
    const std::array explicitLeft{AnimationMorphBinding{0,0,0}};
    scopedOptions.indexedBindings=explicitLeft;scopedOptions.exportMatchingMorphCurves=true;
    rejects([&]{(void)exportObject3DFbx(asset.get(),conflict,scopedOptions);});
    const auto first = exportObject3DFbx(asset.get(), clip, options);
    CHECK(!first.empty() && first == exportObject3DFbx(asset.get(), clip, options));
    CHECK(exportObject3DFbx(asset.get()) == exportObject3DFbx(asset.get(),FbxExportOptions{}));
    const AnimationClip morphOnly(asset.get().skeleton(),2,{},{{"Smile",{{0,.25},{2,.75}}}});
    CHECK(!exportObject3DFbx(asset.get(),morphOnly,options).empty());
    rejects([&] { (void)exportObject3DFbx(asset.get(),clip); });
    auto unknown = motion(asset.get().skeleton(),true);
    rejects([&] { (void)exportObject3DFbx(asset.get(),unknown,options); });
    options.allowDiscardScalarCurves = true;
    CHECK(!exportObject3DFbx(asset.get(),unknown,options).empty());
    options = settings(); options.exportMatchingMorphCurves = false; options.allowDiscardScalarCurves = true;
    CHECK(!exportObject3DFbx(asset.get(),clip,options).empty());
    options = settings(); options.maximumSamples = 1;
    rejects([&] { (void)exportObject3DFbx(asset.get(),clip,options); });
    options.maximumSamples = 0;
    rejects([&] { (void)exportObject3DFbx(asset.get(),clip,options); });
    options.maximumSamples = 1000001;
    rejects([&] { (void)exportObject3DFbx(asset.get(),clip,options); });
    options = settings(); options.maximumSampleInterval = 0;
    rejects([&] { (void)exportObject3DFbx(asset.get(),clip,options); });
    options.maximumSampleInterval = 1e-100;
    rejects([&] { (void)exportObject3DFbx(asset.get(),clip,options); });
    options = settings(); options.maximumRotationError = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { (void)exportObject3DFbx(asset.get(),clip,options); });
    options.maximumRotationError = 0;
    rejects([&] { (void)exportObject3DFbx(asset.get(),clip,options); });
    options = settings();
    auto foreignSkeleton = std::make_shared<Skeleton>(*asset.get().skeleton());
    const auto foreign = motion(foreignSkeleton);
    rejects([&] { (void)exportObject3DFbx(asset.get(),foreign,options); });
    const AnimationClip empty(asset.get().skeleton(),2,{});
    rejects([&] { (void)exportObject3DFbx(asset.get(),empty,options); });
    std::vector<Transform> bind;
    for (const auto &bone : asset.get().skeleton()->bones()) bind.push_back(bone.bindTransform);
    bind[1].position.y += .2;
    const AnimationClip bound(asset.get().skeleton(),2,
        std::vector<BoneTrack>(clip.tracks().begin(),clip.tracks().end()),{},bind);
    rejects([&] { (void)exportObject3DFbx(asset.get(),bound,options); });
    const auto transform = asset.get().skeleton()->bones()[0].bindTransform;
    const AnimationClip collision(asset.get().skeleton(),1,{{0,{{.5,transform},{.500000000001,transform}}}});
    rejects([&] { (void)exportObject3DFbx(asset.get(),collision,options); });
    const AnimationClip boundaryCollision(asset.get().skeleton(),1,{{0,{{1e-13,transform}}}});
    rejects([&] { (void)exportObject3DFbx(asset.get(),boundaryCollision,options); });
    const AnimationClip hugeDuration(asset.get().skeleton(),1e100,{{0,{{0,transform}}}});
    rejects([&] { (void)exportObject3DFbx(asset.get(),hugeDuration,options); });
    auto huge = transform; huge.position.x = std::numeric_limits<double>::max();
    const AnimationClip hugeValue(asset.get().skeleton(),1,{{0,{{0,huge}}}});
    rejects([&] { (void)exportObject3DFbx(asset.get(),hugeValue,options); });
    auto tiny = transform; tiny.scale.x = 1e-100;
    const AnimationClip tinyScale(asset.get().skeleton(),1,{{0,{{0,tiny}}}});
    rejects([&] { (void)exportObject3DFbx(asset.get(),tinyScale,options); });
    options.allowDiscardScalarCurves = true; options.exportMatchingMorphCurves = false;
    rejects([&] { (void)exportObject3DFbx(asset.get(),morphOnly,options); });
}

int main(int argc,char **argv)
{
    try
    {
        if (argc != 2) return 2;
        if (std::string_view(argv[1]) == "validation") validation();
        else if (std::string_view(argv[1]) == "fixture" || std::string_view(argv[1]) == "negative_fixture")
        {
            Object3DEngine engine; const auto asset = engine.createObject(fixtureData());
            const auto bytes = exportObject3DFbx(asset.get(),motion(asset.get().skeleton(),false,
                std::string_view(argv[1]) == "negative_fixture"),settings());
            emit(bytes);
        }
        else if (std::string_view(argv[1]) == "scalar_fixture")
        {
            Object3DEngine engine; const auto asset = engine.createObject(fixtureData());
            const AnimationClip clip(asset.get().skeleton(),2,{},{{"Smile",{{.25,-.5},{1.75,1.5}}}});
            const auto bytes = exportObject3DFbx(asset.get(),clip,settings());
            emit(bytes);
        }
        else if (std::string_view(argv[1]) == "indexed_fixture")
        {
            Object3DEngine engine;const auto asset=engine.createObject(fixtureData());
            const AnimationClip clip(asset.get().skeleton(),2,{},{{"Left",{{0,.1},{2,.9}}},{"Right",{{0,.8},{2,.2}}}});
            const std::array indices{AnimationMorphBinding{0,0,0},AnimationMorphBinding{1,1,0}};
            FbxAnimationExportOptions options;options.indexedBindings=indices;
            emit(exportObject3DFbx(asset.get(),clip,options));
        }
        else if(std::string_view(argv[1])=="indexed_logical_fixture"||std::string_view(argv[1])=="indexed_logical_external_fixture")
        {
            Object3DEngine engine;const auto asset=engine.createObject(logicalFixtureData());
            const AnimationClip clip(asset.get().skeleton(),2,{},{{"Body",{{0,.1},{2,.9}}},{"Garment",{{0,.8},{2,.2}}}});
            const std::array indices{AnimationMorphBinding{0,0,0},AnimationMorphBinding{1,1,0}};
            FbxAnimationExportOptions options;options.indexedBindings=indices;
            if(std::string_view(argv[1])=="indexed_logical_external_fixture")options.geometry.externalDocument="indexed-logical";
            emit(exportObject3DFbx(asset.get(),clip,options));
        }
        else return 2;
        return 0;
    }
    catch(const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
