#include "../src/third_party/ufbx/ufbx.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string_view>
#include <vector>

#define CHECK(x) do { if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
static ufbx_quat quat(double x,double y,double z,double w)
{
    ufbx_quat q{};q.x=x;q.y=y;q.z=z;q.w=w;return q;
}
static ufbx_quat product(ufbx_quat a,ufbx_quat b)
{
    return quat(a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
        a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z);
}
static double error(ufbx_quat a, ufbx_quat b)
{
    if(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0) b=quat(-b.x,-b.y,-b.z,-b.w);
    const auto distance=std::hypot(std::hypot(a.x-b.x,a.y-b.y),std::hypot(a.z-b.z,a.w-b.w));
    return 4*std::asin(std::min(1.,distance/2));
}
int main(int argc,char **argv)
{
    try
    {
        const std::string encoded((std::istreambuf_iterator<char>(std::cin)),{});
        CHECK(encoded.size()%2==0);
        const auto digit=[](char c)->unsigned
        {
            if(c>='0'&&c<='9')return static_cast<unsigned>(c-'0');
            if(c>='a'&&c<='f')return static_cast<unsigned>(c-'a'+10);
            throw std::runtime_error("Invalid hexadecimal FBX fixture");
        };
        std::vector<std::uint8_t> bytes;bytes.reserve(encoded.size()/2);
        for(std::size_t i=0;i<encoded.size();i+=2)bytes.push_back(static_cast<std::uint8_t>((digit(encoded[i])<<4)|digit(encoded[i+1])));
        ufbx_load_opts options{}; options.strict=true; ufbx_error diagnostic{};
        std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> scene(
            ufbx_load_memory(bytes.data(),bytes.size(),&options,&diagnostic),ufbx_free_scene);
        if(!scene)throw std::runtime_error(std::string("ufbx: ")+
            std::string(diagnostic.description.data?diagnostic.description.data:"",diagnostic.description.length));
        CHECK(scene->anim_stacks.count==1 && scene->anim);
        CHECK(scene->anim_stacks.data[0]->layers.count==1);
        if(argc==2 && (std::string_view(argv[1])=="corrective"||std::string_view(argv[1])=="corrective_narrow"))
        {
            const bool narrow=std::string_view(argv[1])=="corrective_narrow";
            CHECK(scene->blend_channels.count==4);
            int folds=0,keeps=0;
            for(std::size_t channel=0;channel<scene->blend_channels.count;++channel)
            {
                const auto *target=scene->blend_channels.data[channel];
                const std::string_view name(target->name.data,target->name.length);
                CHECK(name=="fold"||name=="keep");
                if(name=="fold")++folds;else ++keeps;
                for(int i=0;i<=10000;++i)
                {
                    const double time=i/10000.;
                    const double influence=std::clamp(1-std::abs(time-.3)*std::numbers::pi/2/.02,0.,1.);
                    const double fold=narrow?influence*influence*(3-2*influence):time*time*(3-2*time);
                    const double expected=name=="fold"?fold:.2+.6*time;
                    CHECK(std::abs(ufbx_evaluate_blend_weight(scene->anim,target,time)-expected)<(narrow?.0011:.0003));
                }
            }
            CHECK(folds==2&&keeps==2);
            const auto *elbow=ufbx_find_node(scene.get(),"elbow");CHECK(elbow);
            for(int i=0;i<=100;++i)
            {
                const double time=i/100.;
                const auto actual=ufbx_evaluate_transform(scene->anim,elbow,time);
                const double angle=std::numbers::pi*time/4;
                CHECK(error(actual.rotation,quat(std::sin(angle),0,0,std::cos(angle)))<.0011);
            }
            std::cout<<"Independent ufbx corrective animation reader passed\n";return 0;
        }
        if(argc==2 && (std::string_view(argv[1])=="indexed_logical"||std::string_view(argv[1])=="indexed_logical_external"))
        {
            const bool grouped=std::string_view(argv[1])=="indexed_logical_external";
            CHECK(scene->blend_channels.count==(grouped?2:3));
            const auto *body=ufbx_find_node(scene.get(),"body"),*garment=ufbx_find_node(scene.get(),"garment");
            CHECK(body&&garment&&body->mesh&&garment->mesh);
            CHECK(body->mesh->blend_deformers.count==1&&garment->mesh->blend_deformers.count==1);
            const auto *a=body->mesh->blend_deformers.data[0],*b=garment->mesh->blend_deformers.data[0];
            CHECK(a->channels.count==(grouped?1:2)&&b->channels.count==1);
            for(int i=0;i<=200;++i)
            {
                const double time=i*.01;
                for(const auto *channel:a->channels)
                    CHECK(std::abs(ufbx_evaluate_blend_weight(scene->anim,channel,time)-(.1+.4*time))<2e-5);
                CHECK(std::abs(ufbx_evaluate_blend_weight(scene->anim,b->channels.data[0],time)-(.8-.3*time))<2e-5);
            }
            std::cout<<"Independent ufbx logical morph animation reader passed\n";return 0;
        }
        CHECK(scene->blend_channels.count==2);
        if(argc==2 && std::string_view(argv[1])=="baked_glb")
        {
            const auto *body=ufbx_find_node(scene.get(),"body"),*garment=ufbx_find_node(scene.get(),"garment"),
                       *joint=ufbx_find_node(scene.get(),"joint");
            CHECK(body&&garment&&joint&&body->mesh&&garment->mesh);
            CHECK(body->mesh->blend_deformers.count==1&&garment->mesh->blend_deformers.count==1);
            const auto *a=body->mesh->blend_deformers.data[0],*b=garment->mesh->blend_deformers.data[0];
            CHECK(a->channels.count==1&&b->channels.count==1);
            for(int i=0;i<=100;++i)
            {
                const double time=i/100.;
                CHECK(std::abs(ufbx_evaluate_blend_weight(scene->anim,a->channels.data[0],time)-time)<2e-5);
                CHECK(std::abs(ufbx_evaluate_blend_weight(scene->anim,b->channels.data[0],time)-(1-time))<2e-5);
                CHECK(std::abs(ufbx_evaluate_transform(scene->anim,joint,time).translation.x-2*time)<2e-5);
            }
            std::cout<<"Independent ufbx baked GLB animation reader passed\n";return 0;
        }
        if(argc==2 && std::string_view(argv[1])=="indexed")
        {
            const auto *body=ufbx_find_node(scene.get(),"body"),*eyes=ufbx_find_node(scene.get(),"eyes");
            CHECK(body&&eyes&&body->mesh&&eyes->mesh);
            CHECK(body->mesh->blend_deformers.count==1&&eyes->mesh->blend_deformers.count==1);
            const auto *a=body->mesh->blend_deformers.data[0],*b=eyes->mesh->blend_deformers.data[0];
            CHECK(a->channels.count==1&&b->channels.count==1);
            for(int i=0;i<=200;++i)
            {
                const double time=i*.01;
                CHECK(std::abs(ufbx_evaluate_blend_weight(scene->anim,a->channels.data[0],time)-(.1+.4*time))<2e-5);
                CHECK(std::abs(ufbx_evaluate_blend_weight(scene->anim,b->channels.data[0],time)-(.8-.3*time))<2e-5);
            }
            std::cout<<"Independent ufbx per-mesh animation reader passed\n";return 0;
        }
        if(argc==2 && std::string_view(argv[1])=="scalar")
        {
            for(int i=0;i<=200;++i)
            {
                const double time=i*.01;
                const double expected=-.5+2*std::clamp((time-.25)/1.5,0.,1.);
                for(std::size_t channel=0;channel<scene->blend_channels.count;++channel)
                    CHECK(std::abs(ufbx_evaluate_blend_weight(scene->anim,scene->blend_channels.data[channel],time)-expected)<2e-5);
            }
            std::cout<<"Independent ufbx scalar-only animation reader passed\n";return 0;
        }
        if(argc>2 || (argc==2 && std::string_view(argv[1])!="negative"))return 2;
        const double sign=argc==2?-1.:1.;
        const auto *root=ufbx_find_node(scene.get(),"root"), *joint=ufbx_find_node(scene.get(),"joint"),
                   *tip=ufbx_find_node(scene.get(),"tip");
        CHECK(root && joint && tip && tip->parent == joint);
        double maximumRotationError=0;
        for(int i=0;i<=200;++i)
        {
            const double time=i*.01;
            const auto r=ufbx_evaluate_transform(scene->anim,root,time);
            const double radians=(170+10*time)*std::numbers::pi/180;
            maximumRotationError=std::max(maximumRotationError,error(r.rotation,quat(0,0,std::sin(radians/2),std::cos(radians/2))));
            CHECK(std::abs(r.translation.x-time)<2e-5);
            const auto j=ufbx_evaluate_transform(scene->anim,joint,time);
            const double angle=1.25*time, s=std::sin(angle/2)/std::sqrt(14.);
            maximumRotationError=std::max(maximumRotationError,error(j.rotation,quat(s,2*s,3*s,std::cos(angle/2))));
            const auto t=ufbx_evaluate_transform(scene->anim,tip,time);
            const double gimbal=sign*(80+20*std::clamp((time-.25)/1.5,0.,1.))*std::numbers::pi/180;
            const auto roll=quat(std::sin(10*std::numbers::pi/180),0,0,std::cos(10*std::numbers::pi/180));
            const auto yaw=quat(0,0,std::sin(17.5*std::numbers::pi/180),std::cos(17.5*std::numbers::pi/180));
            const auto expected=product(yaw,product(quat(0,std::sin(gimbal/2),0,std::cos(gimbal/2)),roll));
            maximumRotationError=std::max(maximumRotationError,error(t.rotation,expected));
            CHECK(std::abs(j.translation.y-1)<2e-5);
            CHECK(std::abs(j.scale.x-(1+time*.5))<2e-5);
            CHECK(std::abs(j.scale.y-(1+time*.25))<2e-5);
            CHECK(std::abs(j.scale.z-(1-time*.25))<2e-5);
            for(std::size_t channel=0;channel<scene->blend_channels.count;++channel)
                CHECK(std::abs(ufbx_evaluate_blend_weight(scene->anim,scene->blend_channels.data[channel],time)-(.25+time*.25))<2e-5);
        }
        CHECK(maximumRotationError<.0006);
        std::cout << "Independent ufbx animation reader passed; maximum rotation error radians: " << maximumRotationError << '\n';
        return 0;
    }
    catch(const std::exception &exception) { std::cerr << exception.what() << '\n'; return 1; }
}
