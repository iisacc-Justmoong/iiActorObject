#include <iiActorObject/MeshTangents.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
template<class F> void rejects(F fn) { bool caught=false;try{fn();}catch(const std::exception &){caught=true;}CHECK(caught); }
bool near(double a,double b,double tolerance=1e-6){return std::abs(a-b)<tolerance;}

SkeletalMeshLod mirrored()
{
    SkeletalMeshLod mesh;
    mesh.vertices={{{0,0,0},{0,0,1},{}},{{1,0,0},{0,0,1},{}},
                   {{0,1,0},{0,0,1},{}},{{-1,0,0},{0,0,1},{}}};
    mesh.triangles={0,1,2,0,2,3};
    return mesh;
}
std::vector<TextureCoordinate> mirroredUv(){return {{0,0},{1,0},{0,1},{1,0}};}

void seams()
{
    auto mesh=mirrored();auto uv=mirroredUv();const auto t=calculateMeshCornerTangents(mesh,uv);
    CHECK(t.size()==mesh.triangles.size());
    for(std::size_t i=0;i<3;++i)
        CHECK(near(t[i].direction.x,1)&&near(t[i].direction.y,0)&&near(t[i].direction.z,0)&&t[i].handedness==1);
    for(std::size_t i=3;i<6;++i)
        CHECK(near(t[i].direction.x,-1)&&near(t[i].direction.y,0)&&near(t[i].direction.z,0)&&t[i].handedness==-1);
    CHECK(mesh.triangles[0]==mesh.triangles[3]);
    CHECK(t[0].handedness!=t[3].handedness);
    MeshTangentOptions flipped;flipped.flipTextureV=true;const auto flip=calculateMeshCornerTangents(mesh,uv,flipped);
    for(std::size_t i=0;i<t.size();++i)
        CHECK(near(flip[i].direction.x,t[i].direction.x)&&flip[i].handedness==-t[i].handedness);
}
void deformation()
{
    auto mesh=mirrored();auto uv=mirroredUv();std::vector<DeformedVertex> vertices;
    for(const auto &v:mesh.vertices)vertices.push_back({v.position,v.normal});
    const auto expected=calculateMeshCornerTangents(mesh,uv);
    const auto actual=calculateMeshCornerTangents(vertices,mesh.triangles,uv);
    CHECK(expected.size()==actual.size());
    for(std::size_t i=0;i<actual.size();++i)
        CHECK(near(expected[i].direction.x,actual[i].direction.x)&&expected[i].handedness==actual[i].handedness);
    // Rotate the deformed surface and normals 90 degrees about X.
    for(auto &v:vertices){v.position={v.position.x,0,v.position.y};v.normal={0,-1,0};}
    const auto rotated=calculateMeshCornerTangents(vertices,mesh.triangles,uv);
    for(const auto &t:rotated)CHECK(near(t.direction.y,0)&&near(t.direction.z,0)&&near(std::abs(t.direction.x),1));
    CHECK(rotated[0].handedness==1&&rotated[3].handedness==-1);
}
void validation()
{
    auto mesh=mirrored();auto uv=mirroredUv();MeshTangentOptions options;
    options.maxCorners=5;rejects([&]{(void)calculateMeshCornerTangents(mesh,uv,options);});
    options.maxCorners=0;rejects([&]{(void)calculateMeshCornerTangents(mesh,uv,options);});
    options.maxCorners=8'000'001;rejects([&]{(void)calculateMeshCornerTangents(mesh,uv,options);});
    options.maxCorners=8'000'000;auto shortUv=uv;shortUv.pop_back();
    rejects([&]{(void)calculateMeshCornerTangents(mesh,shortUv,options);});
    auto bad=mesh;bad.triangles[1]=999;rejects([&]{(void)calculateMeshCornerTangents(bad,uv,options);});
    bad=mesh;bad.triangles.pop_back();rejects([&]{(void)calculateMeshCornerTangents(bad,uv,options);});
    bad=mesh;bad.vertices[0].normal={0,0,0};rejects([&]{(void)calculateMeshCornerTangents(bad,uv,options);});
    bad=mesh;bad.vertices[0].position.x=std::numeric_limits<double>::infinity();
    rejects([&]{(void)calculateMeshCornerTangents(bad,uv,options);});
    bad=mesh;bad.vertices[0].position.x=100'000'001;
    rejects([&]{(void)calculateMeshCornerTangents(bad,uv,options);});
    uv[0].u=std::numeric_limits<double>::max();rejects([&]{(void)calculateMeshCornerTangents(mesh,uv,options);});
}
int main(int argc,char **argv)
{
    try {if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="seams")seams();else if(mode=="deformation")deformation();else if(mode=="validation")validation();else return 2;
        std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
