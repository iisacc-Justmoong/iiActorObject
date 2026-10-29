#include <iiActorObject/Object3DInterchange.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
std::vector<std::uint8_t> fixture(double shear)
{
    std::ostringstream s;s.precision(17);s<<R"(; FBX 7.4.0 project file
FBXHeaderExtension: { FBXHeaderVersion: 1003
 FBXVersion: 7400
}
GlobalSettings: { Version: 1000
 Properties70: { P: "UpAxis", "int", "Integer", "",1
 P: "UpAxisSign", "int", "Integer", "",1
 P: "FrontAxis", "int", "Integer", "",2
 P: "FrontAxisSign", "int", "Integer", "",1
 P: "CoordAxis", "int", "Integer", "",0
 P: "CoordAxisSign", "int", "Integer", "",1
 P: "UnitScaleFactor", "double", "Number", "",100
 }
}
Objects: {
 Model: 1, "Model::Rig", "Null" { Version: 232 }
 Model: 2, "Model::root", "LimbNode" { Version: 232 }
 NodeAttribute: 7, "NodeAttribute::root", "LimbNode" { TypeFlags: "Skeleton" }
 Model: 3, "Model::Mesh", "Mesh" { Version: 232 }
 Geometry: 4, "Geometry::Mesh", "Mesh" {
  Vertices: *9 { a: 0,0,0,1,0,0,0,1,0 }
  PolygonVertexIndex: *3 { a: 0,1,-3 }
 }
 Deformer: 5, "Deformer::Skin", "Skin" { Version: 101 }
 Deformer: 6, "SubDeformer::Cluster", "Cluster" {
  Version: 100
  Indexes: *3 { a: 0,1,2 }
  Weights: *3 { a: 1,1,1 }
  Transform: *16 { a: 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 }
  TransformLink: *16 { a: 1,0,0,0,)"<<shear<<R"(,1,0,0,0,0,1,0,0,0,0,1 }
 }
}
Connections: {
 C: "OO",1,0
 C: "OO",2,1
 C: "OO",7,2
 C: "OO",3,0
 C: "OO",4,3
 C: "OO",5,4
 C: "OO",6,5
 C: "OO",2,6
}
)";const auto text=s.str();return {text.begin(),text.end()};
}
int main()
{
    try
    {
        Object3DEngine engine;const auto data=fixture(5e-7);rejects([&]{(void)importObject3DFbx(engine.reference(),data);});CHECK(engine.objectCount()==0);
        FbxImportOptions options;options.maximumJointMatrixError=1e-6;const auto imported=importObject3DFbx(engine.reference(),data,options);CHECK(imported.object.get().data().bones.size()==1&&!imported.warnings.empty()&&imported.maximumJointMatrixError>1e-7&&imported.maximumJointMatrixError<1e-6);
        rejects([&]{(void)importObject3DFbx(engine.reference(),fixture(.1),options);});CHECK(engine.objectCount()==1);
        options.maximumJointMatrixError=std::numeric_limits<double>::quiet_NaN();rejects([&]{(void)importObject3DFbx(engine.reference(),data,options);});options.maximumJointMatrixError=.1;rejects([&]{(void)importObject3DFbx(engine.reference(),data,options);});CHECK(engine.objectCount()==1);
        std::cout<<"Explicit near-SRT tolerance, warning, true shear rejection and atomic failure passed\n";
    }
    catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
