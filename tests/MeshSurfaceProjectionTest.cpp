#include <iiActorObject/MeshSurface.h>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>

using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class Error,class F> void rejects(F run)
{bool caught=false;try{run();}catch(const Error&){caught=true;}CHECK(caught);}
bool same(const SurfacePairHit &a,const SurfacePairHit &b)
{
    return a.valid()==b.valid()&&a.source.triangle==b.source.triangle&&a.source.vertices==b.source.vertices&&
        a.source.barycentric==b.source.barycentric&&a.source.position==b.source.position&&a.source.distance==b.source.distance&&
        a.targetBarycentric==b.targetBarycentric&&a.targetPosition==b.targetPosition;
}
Vector3 add(Vector3 a,Vector3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vector3 multiply(Vector3 a,double s){return {a.x*s,a.y*s,a.z*s};}
std::array<Vector3,3> triangle(Vector3 origin,unsigned axis,double side)
{
    Vector3 u,v;if(axis==0){u.y=side;v.z=side;}else if(axis==1){u.z=side;v.x=side;}else{u.x=side;v.y=side;}
    return {origin,add(origin,u),add(origin,v)};
}
void oracle()
{
    std::size_t queries=0;
    for(double scale:{.01,1.,100.})for(unsigned winding=0;winding<2;++winding)
    {
        std::uint32_t state=0x20261027;
        auto random=[&]{state=state*1664525u+1013904223u;return (static_cast<int>(state>>20)-2048)/512.;};
        auto point=[&]{return multiply({random(),random(),random()},scale);};
        std::vector<Vector3> points{{1e150,0,0}};
        std::vector<std::uint32_t> indices;
        for(unsigned i=0;i<32;++i)
        {
            auto t=triangle(point(),i%3,scale*.75);if(winding)std::swap(t[1],t[2]);
            for(auto p:t){indices.push_back(static_cast<std::uint32_t>(points.size()));points.push_back(p);}
        }
        indices.insert(indices.end(),{indices[0],indices[1],indices[2]});
        MeshSurface surface(points,indices);
        std::vector<MeshSurface> singles;singles.reserve(indices.size()/3);
        const std::array<std::uint32_t,3> local{0,1,2};
        for(std::size_t i=0;i<indices.size();i+=3)
        {
            const std::array<Vector3,3> t{points[indices[i]],points[indices[i+1]],points[indices[i+2]]};
            singles.emplace_back(t,local);
        }
        for(unsigned q=0;q<64;++q)
        {
            auto query=triangle(point(),q%3,scale*.75);
            if(q%8==0){const auto base=(q%32)*3;query={points[indices[base]],points[indices[base+1]],points[indices[base+2]]};}
            SurfacePairHit nearest;
            // Each one-triangle query starts with an infinite radius, so neither
            // a BVH candidate ordering nor a finite-distance prefilter selects it.
            for(std::size_t t=0;t<singles.size();++t)
            {
                auto hit=singles[t].closestTriangle(query,std::numeric_limits<double>::infinity());
                hit.source.triangle=static_cast<std::uint32_t>(t);hit.source.vertices={indices[t*3],indices[t*3+1],indices[t*3+2]};
                if(!nearest.valid()||hit.source.distance<nearest.source.distance||
                    (hit.source.distance==nearest.source.distance&&t<nearest.source.triangle))nearest=hit;
            }
            CHECK(nearest.valid());const auto d=nearest.source.distance;
            const std::array radii{0.,scale*.01,scale,std::numeric_limits<double>::infinity(),d,
                std::nextafter(d,std::numeric_limits<double>::infinity()),d>0?std::nextafter(d,0.):0.};
            for(auto radius:radii)
            {
                const auto expected=d<=radius?nearest:SurfacePairHit{};const auto actual=surface.closestTriangle(query,radius);++queries;
                if(!same(actual,expected))
                {
                    std::cerr<<std::setprecision(17)<<"query="<<q<<" scale="<<scale<<" winding="<<winding<<" radius="<<radius
                        <<" actual="<<actual.source.triangle<<','<<actual.source.distance<<" expected="<<expected.source.triangle<<','<<expected.source.distance<<'\n';
                    throw std::runtime_error("BVH triangle result differs from the one-triangle oracle");
                }
            }
        }
    }
    std::cout<<"exactOracleQueries="<<queries<<'\n';
}
void budgets()
{
    const std::array<Vector3,6> points{{{0,0,0},{0,1,0},{0,0,1},{3,0,0},{3,1,0},{3,0,1}}};
    const std::array<std::uint32_t,6> indices{0,1,2,3,4,5};
    const std::array<Vector3,3> query{points[3],points[4],points[5]};MeshSurface surface(points,indices);
    SurfaceQueryBudget full;const auto hit=surface.closestTriangle(query,.01,full);CHECK(hit.valid()&&hit.source.triangle==1&&hit.source.distance==0);CHECK(full.nodeVisits==1&&full.triangleTests==2);
    SurfaceQueryBudget limited;limited.maximumTriangleTests=1;rejects<std::length_error>([&]{(void)surface.closestTriangle(query,.01,limited);});CHECK(limited.nodeVisits==1&&limited.triangleTests==1);
    SurfaceQueryBudget empty;empty.maximumNodeVisits=0;rejects<std::length_error>([&]{(void)surface.closestTriangle(query,.01,empty);});CHECK(empty.nodeVisits==0&&empty.triangleTests==0);
}
void legacyErrors()
{
    for(double side:{1e-310,1.})
    {
        const std::array<Vector3,6> points{{{0,0,0},{0,side,0},{0,0,1e-310},{1e100,0,0},{1e100,side,0},{1e100,0,1e-310}}};
        const std::array<std::uint32_t,6> indices{0,1,2,3,4,5};const std::array<Vector3,3> query{points[3],points[4],points[5]};MeshSurface surface(points,indices);SurfaceQueryBudget budget;
        rejects<std::overflow_error>([&]{(void)surface.closestTriangle(query,.002,budget);});CHECK(budget.nodeVisits==1&&budget.triangleTests==1);
    }
    const std::array<Vector3,6> points{{{-1e308,0,0},{-1e308,1,0},{-1e308,0,1},{1e308,0,0},{1e308,1,0},{1e308,0,1}}};
    const std::array<std::uint32_t,6> indices{0,1,2,3,4,5};const std::array<Vector3,3> query{points[3],points[4],points[5]};MeshSurface surface(points,indices);SurfaceQueryBudget budget;
    rejects<std::overflow_error>([&]{(void)surface.closestTriangle(query,.002,budget);});CHECK(budget.nodeVisits==1&&budget.triangleTests==1);
}
int main()
{
    try{oracle();budgets();legacyErrors();std::cout<<"triangleProjectionCases=3\n";return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
