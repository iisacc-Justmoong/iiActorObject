#include "../src/third_party/ufbx/ufbx.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>
#define CHECK(x) do { if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
int main(int argc,char **argv)
{
    try
    {
        if(argc<2||argc>3)return 2;std::ifstream file(argv[1],std::ios::binary);CHECK(file.good());
        std::string contents((std::istreambuf_iterator<char>(file)),{});
        const bool cubic=argc==3&&std::string_view(argv[2])=="cubic";
        const bool pivot=argc==3&&std::string_view(argv[2])=="pivot";
        const bool axisLeft=argc==3&&std::string_view(argv[2])=="axis-left";
        const bool axis=axisLeft||(argc==3&&std::string_view(argv[2])=="axis");
        if(argc==3&&!cubic&&!pivot&&!axis)return 2;
        if(cubic)
        {
            const std::string linear="KeyAttrFlags: *1 { a: 4 }", replacement="KeyAttrFlags: *1 { a: 1032 }";
            const auto index=contents.find(linear);CHECK(index!=std::string::npos);contents.replace(index,linear.size(),replacement);
        }
        if(pivot)
        {
            const auto node=contents.find("Model: 101,");CHECK(node!=std::string::npos);
            const auto index=contents.find("P: \"InheritType\"",node);CHECK(index!=std::string::npos);
            contents.insert(index,"P: \"PreRotation\", \"Vector3D\", \"Vector\", \"\",0,0,30\n   "
                                  "P: \"RotationPivot\", \"Vector3D\", \"Vector\", \"\",1,0,0\n   ");
        }
        if(axis)
        {
            const auto replace=[&](std::string_view from,std::string_view to)
            {const auto i=contents.find(from);CHECK(i!=std::string::npos);contents.replace(i,from.size(),to);};
            replace("\"UpAxis\", \"int\", \"Integer\", \"\",1","\"UpAxis\", \"int\", \"Integer\", \"\",2");
            replace("\"FrontAxis\", \"int\", \"Integer\", \"\",2","\"FrontAxis\", \"int\", \"Integer\", \"\",1");
            if(!axisLeft)replace("\"FrontAxisSign\", \"int\", \"Integer\", \"\",1","\"FrontAxisSign\", \"int\", \"Integer\", \"\",-1");
            replace("\"UnitScaleFactor\", \"double\", \"Number\", \"\",100","\"UnitScaleFactor\", \"double\", \"Number\", \"\",1");
        }
        const std::vector<std::uint8_t> bytes(contents.begin(),contents.end());
        ufbx_load_opts options{};options.strict=true;options.target_axes=ufbx_axes_right_handed_y_up;
        options.target_unit_meters=1;options.space_conversion=UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;
        if(axisLeft)options.handedness_conversion_axis=UFBX_MIRROR_AXIS_X;
        ufbx_error error{};std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> scene(
            ufbx_load_memory(bytes.data(),bytes.size(),&options,&error),ufbx_free_scene);
        if(!scene){char message[4096];ufbx_format_error(message,sizeof(message),&error);throw std::runtime_error(message);}
        CHECK(scene->anim_stacks.count==2);
        const auto *carrier=ufbx_find_node(scene.get(),"Carrier"),*bone=ufbx_find_node(scene.get(),"Bone");
        CHECK(carrier&&bone&&bone->parent==carrier);
        if(axisLeft)
        {
            const auto *meshNode=ufbx_find_node(scene.get(),"Triangle");CHECK(meshNode&&meshNode->mesh);
            CHECK(meshNode->mesh->vertices.count==3);
            const auto point1=meshNode->mesh->vertices.data[1],point2=meshNode->mesh->vertices.data[2];
            const auto p=ufbx_transform_position(&meshNode->geometry_to_world,point1);
            const auto q=ufbx_transform_position(&meshNode->geometry_to_world,point2);
            CHECK(std::abs(p.x-.01)<1e-9 && std::abs(q.z-.01)<1e-9);
        }
        for(const auto *stack:scene->anim_stacks)
        {
            const auto name=std::string_view(stack->name.data,stack->name.length);
            const double start=name=="NegativeRange"?-1.:2.,end=name=="NegativeRange"?1.:3.;
            CHECK((name=="NegativeRange"||name=="PositiveRange") && stack->time_begin==start && stack->time_end==end);
            for(int i=0;i<=10;++i)
            {
                const double time=std::lerp(start,end,i/10.);
                if(axis)
                {
                    ufbx_evaluate_opts evaluate{};ufbx_error evaluationError{};
                    std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> sample(
                        ufbx_evaluate_scene(scene.get(),stack->anim,time,&evaluate,&evaluationError),ufbx_free_scene);
                    CHECK(sample&&bone->typed_id<sample->nodes.count);
                    const auto &world=sample->nodes.data[bone->typed_id]->node_to_world;
                    CHECK(std::abs(world.m03-time*.01)<1e-9);
                    CHECK(std::abs(world.m13)<1e-9);
                    CHECK(std::abs(world.m23+(axisLeft?-.01:.01))<1e-9);
                    continue;
                }
                const auto parent=ufbx_evaluate_transform(stack->anim,carrier,time);
                const auto local=ufbx_evaluate_transform(stack->anim,bone,time);
                const double t=(time-start)/(end-start);
                const double expected=cubic&&name=="NegativeRange"?-1+2*(3*t*t-2*t*t*t):time;
                if(std::abs(parent.translation.x-expected)>=1e-6)
                    std::cerr<<"t="<<t<<" actual="<<parent.translation.x<<" expected="<<expected<<'\n';
                CHECK(std::abs(parent.translation.x-expected)<1e-6);
                const double expectedX=pivot?1-std::cos(std::acos(-1.)/6):0;
                const double expectedY=pivot?.5:1;
                CHECK(std::abs(local.translation.x-expectedX)<1e-10);
                CHECK(std::abs(local.translation.y-expectedY)<1e-10);
                if(pivot)CHECK(std::abs(local.rotation.z-std::sin(std::acos(-1.)/12))<1e-10);
            }
        }
        std::cout<<"Independent ufbx evaluation of the hand-authored two-take FBX passed\n";return 0;
    }
    catch(const std::exception &exception){std::cerr<<exception.what()<<'\n';return 1;}
}
