#include "../src/Interchange/GLB/Object3DGlbTangentNotes.h"
#include <iiJson/Json.h>
#include <stdexcept>
#include <string>
#include <vector>

using namespace iiActorObject::object3d_detail::glb_import;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("GLB tangent notes contract failed");}
Value parse(std::string_view text)
{auto value=ii::json::parse(text);if(!value)throw std::runtime_error("Invalid tangent notes fixture");return std::move(*value);}
}
int main()
{
    std::size_t count=0;bool truncated=false;std::vector<std::string> warnings;
    appendGlbFbxTangentNotes(parse(R"({})"),count,truncated,warnings);
    check(warnings.empty()&&count==0&&!truncated);
    appendGlbFbxTangentNotes(parse(R"({"extras":{"iiActorObjectFbxTangentNotes":["source_fbx_tangents_stale","unknown",7]}})"),count,truncated,warnings);
    check(count==3&&warnings.size()==3&&warnings[0]=="GLB FBX tangent note: source_fbx_tangents_stale"&&
          warnings[1]=="unrecognized GLB FBX tangent note was ignored"&&warnings[2]==warnings[1]);
    appendGlbFbxTangentNotes(parse(R"({"extras":{"iiActorObjectFbxTangentNotes":3}})"),count,truncated,warnings);
    check(count==4&&warnings.back()=="unrecognized GLB FBX tangent notes were ignored");

    std::string text=R"({"extras":{"iiActorObjectFbxTangentNotes":[)";
    for(int i=0;i<40;++i){if(i)text+=',';text+=R"("source_fbx_unbound_uv")";}
    text+="]}}";auto many=parse(text);count=0;truncated=false;warnings.clear();
    appendGlbFbxTangentNotes(many,count,truncated,warnings);
    check(count==32&&truncated&&warnings.size()==33&&warnings.back()=="additional GLB FBX tangent notes were ignored");
    appendGlbFbxTangentNotes(many,count,truncated,warnings);check(warnings.size()==33&&count==32);
}
