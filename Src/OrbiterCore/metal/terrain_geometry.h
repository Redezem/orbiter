#pragma once
#include "planet_mesh.h"
namespace core_metal {
// Fixed bounded geography only. Not a terrain selector/LOD/streaming system.
struct TerrainRegion {
    int level,row,col; // internal surface address (archive level = level+4)
    double lng,lat;
};
inline TerrainRegion terrainRegion(bool coast) {
    constexpr double pi=3.14159265358979323846;
    // Mountain witness selected from installed Elev archive(10,19,92).
    double lng=(coast ? -80.6209 : 80.91683349609374)*pi/180;
    double lat=(coast ? 28.6235 : 35.31412353515625)*pi/180;
    int p=coast?13:10,n=1<<p;
    return {p,int((.5-lat/pi)*n),int((1+lng/pi)*n),lng,lat};
}
inline VECTOR3 terrainPoint(double radius,double height,double lng,double lat) {
    return _V((radius+height)*cos(lat)*cos(lng),(radius+height)*sin(lat),(radius+height)*cos(lat)*sin(lng));
}
inline std::array<double,2> terrainCoordinate(const TerrainRegion &r,unsigned row,unsigned col) {
    constexpr double pi=3.14159265358979323846;
    // Slightly inset endpoint avoids sampling the neighbouring archive at a tile boundary.
    double u=.001+.998*col/32.,v=.001+.998*row/32.,n=double(1<<r.level);
    return {pi*((r.col+u)/n-1),pi*(.5-(r.row+v)/n)};
}
}
