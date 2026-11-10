#pragma once
#include "o3de_geometry2d.hpp"
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace singular::scene {
using Point2 = o3de_geometry::Vector2;
struct PolygonMesh {
    std::vector<std::array<double,3>> vertices;
    std::vector<std::vector<unsigned>> faces;
};

// Convex building footprints only. Concave or degenerate input must not be fan-triangulated.
inline PolygonMesh extrude(std::span<const Point2> footprint,double height) {
    const auto n=footprint.size();
    if(n<3 || n>256 || !std::isfinite(height) || height<=0 || height>1000)
        throw std::runtime_error("Polygon extrusion budget or height invalid");
    double area=0;
    for(size_t i=0;i<n;++i) {
        const auto a=footprint[i],b=footprint[(i+1)%n];
        if(!std::isfinite(a.x) || !std::isfinite(a.y) || std::abs(a.x)>10000 || std::abs(a.y)>10000
            || (a-b).GetLengthSq()<1e-6f)
            throw std::runtime_error("Polygon vertex or edge invalid");
        area+=double(a.x)*b.y-double(a.y)*b.x;
        if(std::abs(o3de_geometry::Signed2DTriangleArea(a,b,footprint[(i+2)%n]))<1e-6f)
            throw std::runtime_error("Polygon contains a degenerate corner");
    }
    if(area<=1e-6 || !o3de_geometry::IsSimplePolygon(footprint)
        || !o3de_geometry::IsConvex(footprint))
        throw std::runtime_error("Footprint must be a simple counterclockwise convex polygon");
    PolygonMesh mesh;
    mesh.vertices.reserve(2*n);mesh.faces.reserve(n+2*(n-2));
    for(double z:{0.0,height})for(auto p:footprint)mesh.vertices.push_back({p.x,p.y,z});
    for(unsigned i=0;i<n;++i) {
        const auto j=unsigned((i+1)%n),top=unsigned(n);
        mesh.faces.push_back({i,j,j+top,i+top});
    }
    for(unsigned i=1;i+1<n;++i) {
        mesh.faces.push_back({0,i+1,i});
        mesh.faces.push_back({unsigned(n),unsigned(n)+i,unsigned(n)+i+1});
    }
    return mesh;
}
}
