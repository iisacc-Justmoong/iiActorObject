#include "Object3DGlbMeshExtrasWrite.h"
#include "Object3DGlbBuffer.h"
#include "Object3DGlbPointIdentity.h"
#include "Object3DMesh.h"
#include "Skeleton.h"
#include <map>
#include <ostream>
#include <stdexcept>
#include <string_view>

namespace iiActorObject::object3d_detail::glb_export
{
void writeGlbMeshExtras(std::ostream &out,Buffer &buffer,const Object3DMesh &mesh,
    std::span<const Bone> bones,bool external,std::size_t sourceMesh)
{
    const auto &g=mesh.geometry;
    if (!g.morphTargets.empty())
    {
        std::map<std::string_view,double,std::less<>> defaults;
        for(const auto &entry:mesh.morphDefaults)defaults.emplace(entry.name,entry.value);
        out << ",\"weights\":[";
        for (std::size_t i = 0; i < g.morphTargets.size(); ++i)
        {
            if (i)
                out << ',';
            const auto found=defaults.find(g.morphTargets[i].name);
            const double value=found==defaults.end()?0.:found->second;
            const auto encoded=narrow(value);
            if(value!=0&&encoded==0)throw std::invalid_argument("GLB default morph weight underflows float32");
            out << encoded;
        }
        out << ']';
    }
    if(!g.morphTargets.empty()||mesh.topology)
    {
        out<<",\"extras\":{";bool extra=false;
        if(!g.morphTargets.empty())
        {
            out<<"\"targetNames\":[";
            for (std::size_t i = 0; i < g.morphTargets.size(); ++i)
            {
                if (i)
                    out << ',';
                out << quote(g.morphTargets[i].name);
            }
            out<<']';extra=true;
        }
        if(mesh.topology)
        {
            const auto metadata=buffer.view(object3d_detail::encodeGlbPointIdentity(mesh,bones));
            if(extra)out<<',';
            out<<"\"iiActorObjectPoints\":{\"version\":1,\"bufferView\":"<<metadata<<'}';
            extra=true;
        }
        if(external){if(extra)out<<',';out<<"\"iiActorObjectSourceMesh\":"<<sourceMesh;}
        out<<'}';
    }
}
} // namespace iiActorObject::object3d_detail::glb_export
