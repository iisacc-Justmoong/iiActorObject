#include "../src/third_party/ufbx/ufbx.h"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>

#define CHECK(x) do { if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
int main(int argc,char **argv)
{
    try
    {
        if(argc!=2)return 2;std::ifstream file(argv[1],std::ios::binary);CHECK(file.good());
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
        ufbx_load_opts options{};options.strict=true;options.retain_vertex_attrib_w=true;
        ufbx_error error{};std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> scene(
            ufbx_load_memory(bytes.data(),bytes.size(),&options,&error),ufbx_free_scene);
        if(!scene)throw std::runtime_error(std::string("ufbx: ")+
            std::string(error.description.data?error.description.data:"",error.description.length));
        CHECK(scene->meshes.count==1);const auto *mesh=scene->meshes.data[0];
        CHECK(mesh->num_indices==6&&mesh->uv_sets.count==2);
        for(std::size_t uv=0;uv<mesh->uv_sets.count;++uv)
        {
            const auto &set=mesh->uv_sets.data[uv];
            CHECK(set.vertex_tangent.exists&&set.vertex_bitangent.exists&&mesh->vertex_normal.exists);
            for(std::size_t i=0;i<mesh->num_indices;++i)
            {
                const auto n=ufbx_get_vertex_vec3(&mesh->vertex_normal,i);
                const auto t=ufbx_get_vertex_vec3(&set.vertex_tangent,i);
                const auto b=ufbx_get_vertex_vec3(&set.vertex_bitangent,i);
                const auto w=ufbx_get_vertex_w_vec3(&set.vertex_tangent,i);
                CHECK(w==1||w==-1);
                CHECK(std::abs(t.x*n.x+t.y*n.y+t.z*n.z)<2e-5);
                CHECK(std::abs(b.x-w*(n.y*t.z-n.z*t.y))<2e-5);
                CHECK(std::abs(b.y-w*(n.z*t.x-n.x*t.z))<2e-5);
                CHECK(std::abs(b.z-w*(n.x*t.y-n.y*t.x))<2e-5);
            }
        }
        std::cout<<"Separate ufbx tangent and UV layer reader passed\n";return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
