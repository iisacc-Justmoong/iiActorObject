#include "../src/Project/Object3DProjectRigArchive.h"
#include "../src/Project/Object3DProjectArchive.h"
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;

namespace
{
void check(bool value)
{
    if(!value)throw std::runtime_error("Project rig archive test failed");
}
}

int main()
{
    ProjectArchive archive;
    ProjectRigArchiveBuilder builder(archive);
    const std::array<BoneIndex,2> joints{3,1};
    const std::array<std::byte,3> source{std::byte{0},std::byte{42},std::byte{255}};
    const std::array<std::byte,2> other{std::byte{5},std::byte{6}};
    builder.appendBytes(ProjectRigByteKind::Dna,0,joints,source);
    builder.appendBytes(ProjectRigByteKind::Dna,1,joints,source);
    builder.appendBytes(ProjectRigByteKind::Dna,0,joints,other);
    builder.appendBytes(ProjectRigByteKind::Pose,0,joints,source);
    builder.appendBytes(ProjectRigByteKind::Corrective,0,joints,source);
    check(archive.dna.size()==2&&archive.dna[0]==std::vector<std::uint8_t>({0,42,255}));
    check(archive.posePrograms.size()==1&&archive.correctivePrograms.size()==1);
    check(archive.rigs.size()==5&&archive.rigs[0].kind==0&&archive.rigs[0].source==0);
    check(archive.rigs[1].asset==1&&archive.rigs[1].source==0);
    check(archive.rigs[2].source==1&&archive.rigs[3].kind==1&&archive.rigs[4].kind==3);
    check(archive.rigs[0].joints==std::vector<BoneIndex>({3,1}));

    TemplateMorphRigProgram morph;
    morph.channels.emplace_back();
    morph.channels[0].input="height";
    const auto equalMorph=morph;
    auto distinctMorph=morph;
    distinctMorph.channels[0].input="width";
    builder.appendMorph(0,joints,morph,2);
    builder.appendMorph(1,joints,morph,std::nullopt);
    builder.appendMorph(0,joints,equalMorph,3);
    builder.appendMorph(0,joints,distinctMorph,std::nullopt);
    check(archive.morphPrograms.size()==2);
    check(archive.rigs[5].kind==2&&archive.rigs[5].source==0&&archive.rigs[5].templateProgram==2);
    check(archive.rigs[6].source==0&&!archive.rigs[6].templateProgram);
    check(archive.rigs[7].source==0&&archive.rigs[7].templateProgram==3);
    check(archive.rigs[8].source==1);

    ProgressiveMorphProgram progressive;
    progressive.channels.emplace_back();
    progressive.channels[0].name="smile";
    const auto equalProgressive=progressive;
    auto distinctProgressive=progressive;
    distinctProgressive.channels[0].name="blink";
    builder.appendProgressive(0,joints,progressive);
    builder.appendProgressive(1,joints,equalProgressive);
    builder.appendProgressive(0,joints,distinctProgressive);
    check(archive.progressiveMorphPrograms.size()==2);
    check(archive.rigs[9].kind==4&&archive.rigs[9].source==0);
    check(archive.rigs[10].source==0&&archive.rigs[11].source==1);

    const auto previous=archive.rigs.size();
    try
    {
        builder.appendBytes(static_cast<ProjectRigByteKind>(99),0,joints,source);
        throw std::runtime_error("Invalid byte source kind accepted");
    }
    catch(const std::invalid_argument &) {}
    check(archive.rigs.size()==previous);
    builder.appendBytes(ProjectRigByteKind::Dna,2,{},{});
    check(archive.dna.size()==3&&archive.dna.back().empty());
    check(archive.rigs.back().asset==2&&archive.rigs.back().source==2&&archive.rigs.back().joints.empty());
}
