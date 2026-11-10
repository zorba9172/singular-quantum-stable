// Canonical, deterministic scene authoring. Blender is only an execution adapter.
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "../include/cycraft/scene/polygon_mesh.hpp"
#include "../include/cycraft/scene/scene_edit.hpp"
namespace fs = std::filesystem;
struct V { double x,y,z; };
using singular::scene::quote;
static std::string vec(V p) {
    std::ostringstream s; s.precision(10); s<<"["<<p.x<<","<<p.y<<","<<p.z<<"]";return s.str();
}
static std::string rotation_track(const std::string& bone, V start, V end) {
    return "{\"bone\":"+quote(bone)+",\"keyframes\":[{\"frame\":1,\"rotation_degrees\":"+
        vec(start)+"},{\"frame\":48,\"rotation_degrees\":"+vec(end)+"}]}";
}
static std::string pbr_textures(const fs::path& directory,double tile_size_m) {
    if(!std::isfinite(tile_size_m) || tile_size_m<0.01 || tile_size_m>100)
        throw std::runtime_error("PBR tile size is outside metric bounds");
    const auto asset=directory.filename().string();
    std::string result="{\"tile_size_m\":"+std::to_string(tile_size_m);
    const std::vector<std::string> roles={"base_color","roughness","normal"};
    const std::vector<std::string> suffixes={"diff","rough","nor_gl"};
    for(size_t i=0;i<roles.size();++i) {
        const auto file=fs::absolute(directory/(asset+"_"+suffixes[i]+"_2k.jpg"));
        if(!fs::is_regular_file(file))throw std::runtime_error("PBR texture missing: "+file.string());
        result+=","+quote(roles[i])+":"+quote(file.string());
    }
    return result+"}";
}
struct Scene {
    std::string name="Nocturne_Meridian";
    std::vector<std::string> materials,objects,cameras,lights,assets;
    void add(std::vector<std::string>& target,std::string value) {
        if(target.size()>=10000)throw std::runtime_error("Scene object budget exceeded");
        target.push_back(std::move(value));
    }
    void material(std::string name,V color,double rough,double metal=0,
                  double emission=0,std::string surface="smooth",std::string textures="",double transmission=0) {
        add(materials,"{\"id\":"+quote(name)+",\"color\":"+vec(color)+",\"roughness\":"+
            std::to_string(rough)+",\"metallic\":"+std::to_string(metal)+
            ",\"emission_strength\":"+std::to_string(emission)+",\"surface\":"+quote(surface)+
            ",\"transmission\":"+std::to_string(transmission)+
            (textures.empty()?"":",\"textures\":"+textures)+"}");
    }
    void shape(std::string id,std::string kind,V location,V size,std::string material,
               double bevel=0.025,std::string extra="") {
        add(objects,"{\"id\":"+quote(id)+",\"kind\":"+quote(kind)+",\"location\":"+vec(location)+
            ",\"size\":"+vec(size)+",\"material\":"+quote(material)+",\"bevel\":"+
            std::to_string(bevel)+extra+"}");
    }
    void extrusion(const std::string& id,V location,
                   std::span<const singular::scene::Point2> footprint,double height,
                   const std::string& material,double bevel) {
        const auto mesh=singular::scene::extrude(footprint,height);
        indexed(id,location,mesh,material,bevel);
    }
    void indexed(const std::string& id,V location,const singular::scene::PolygonMesh& mesh,
                 const std::string& material,double bevel,std::string extra="") {
        std::string vertices="[",faces="[";
        for(const auto& p:mesh.vertices) {
            if(vertices.size()>1)vertices+=",";
            vertices+=vec({p[0],p[1],p[2]});
        }
        for(const auto& face:mesh.faces) {
            if(faces.size()>1)faces+=",";
            faces+="[";
            for(size_t i=0;i<face.size();++i)faces+=(i?",":"")+std::to_string(face[i]);
            faces+="]";
        }
        add(objects,"{\"id\":"+quote(id)+",\"kind\":\"mesh\",\"location\":"+vec(location)+
            ",\"material\":"+quote(material)+",\"bevel\":"+std::to_string(bevel)+
            ",\"vertices\":"+vertices+"],\"faces\":"+faces+"]"+extra+"}");
    }
    void asset(std::string id,const fs::path& file,double height,
               std::vector<std::string> mesh_names = {}) {
        if(!fs::is_regular_file(file))throw std::runtime_error("Missing asset: "+file.string());
        std::string selection = "[";
        for (const auto& name : mesh_names) {
            if (selection.size() > 1) selection += ",";
            selection += quote(name);
        }
        selection += "]";
        add(assets,"{\"id\":"+quote(id)+",\"uri\":"+quote(fs::absolute(file).string())+
            ",\"normalize_height\":"+std::to_string(height)+",\"mesh_names\":"+selection+"}");
    }
    void instance(std::string id,std::string asset,V location,double yaw=0) {
        add(objects,singular::scene::asset_instance_json(id,asset,vec(location),std::to_string(yaw)));
    }
    void facade_window(const std::string& id, V p, V outward, const std::string& material,bool interior=false) {
        auto panel=[&](const std::string& suffix,double offset,double z,double depth,
                       double width,double height,const std::string& surface,double bevel,double lateral=0) {
            shape(id+suffix,"box",{p.x+outward.x*offset+std::abs(outward.y)*lateral,
                p.y+outward.y*offset+std::abs(outward.x)*lateral,p.z+z},
                {std::abs(outward.x)*depth+std::abs(outward.y)*width,
                 std::abs(outward.y)*depth+std::abs(outward.x)*width,height},surface,bevel);
        };
        if(interior) {
            for(int side:{-1,1}) {
                const auto suffix=std::to_string(side);
                panel("_jamb_"+suffix,0,0,0.18,0.135,2.10,"steel",0.008,side*0.7425);
                panel("_rail_"+suffix,0,side*0.98,0.18,1.35,0.14,"steel",0.008);
                panel("_return_"+suffix,-0.64,0,1.10,0.08,2.10,"interior_wall",0,side*0.77);
                panel("_room_slab_"+suffix,-0.64,side*1.01,1.10,1.62,0.08,"interior_wall",0);
                panel("_curtain_"+suffix,-0.30,0,0.035,0.30,1.82,"curtain",0.008,side*0.50);
            }
            panel("_room_back",-1.20,0,0.08,1.62,2.10,
                material=="window_dark"?"interior_wall":material,0);
            panel("_mullion",0.035,0,0.08,0.045,1.82,"steel",0.004);
            panel("",0.02,0,0.012,1.35,1.82,"window_glass",0);
        } else {
            panel("_frame",0,0,0.18,1.62,2.10,"steel",0.015);
            panel("",0.11,0,0.035,1.35,1.82,material,0.005);
        }
        panel("_sill",0.20,-1.05,0.52,1.80,0.12,"granite",0.025);
    }
    void camera(std::string id,V location,V target,double lens,std::string animation="[]") {
        add(cameras,"{\"id\":"+quote(id)+",\"location\":"+vec(location)+",\"target\":"+vec(target)+
            ",\"lens_mm\":"+std::to_string(lens)+",\"f_stop\":8,\"keyframes\":"+animation+"}");
    }
    void light(std::string id,V location,V target,V color,double power,double size) {
        add(lights,"{\"id\":"+quote(id)+",\"kind\":\"AREA\",\"location\":"+vec(location)+
            ",\"target\":"+vec(target)+",\"color\":"+vec(color)+",\"power_watts\":"+
            std::to_string(power)+",\"size_m\":"+std::to_string(size)+"}");
    }
    void write(const fs::path& output,bool four_k=false) {
        fs::create_directories(output.parent_path());
        std::ofstream out(output);
        if(!out)throw std::runtime_error("Cannot write scene");
        out<<"{\n\"schema\":\"singular.scene/1\",\"name\":"<<quote(name)<<","
             "\"units\":\"meters\",\"up_axis\":\"Z\",\"fps\":24,\"frame_start\":1,\"frame_end\":48,"
             "\"physics\":{\"gravity_m_s2\":[0,0,-9.81]},"
             "\"render\":{\"engine\":\"CYCLES\",\"width\":"<<(four_k?3840:960)<<
             ",\"height\":"<<(four_k?2160:540)<<",\"samples\":"<<(four_k?128:48)<<
             ",\"adaptive_threshold\":"<<(four_k?0.015:0.025)<<",\"camera\":\"street\",\"device\":\"METAL\","
             "\"denoise\":true,\"exposure\":0.7},"
             "\"world\":{\"color\":[0.025,0.045,0.09],\"strength\":0.35},\n";
        auto array=[&](const char* key,const auto& values,bool last=false){
            out<<quote(key)<<":[\n";for(size_t i=0;i<values.size();++i)out<<(i?",\n":"")<<values[i];
            out<<"\n]"<<(last?"\n":",\n");
        };
        array("materials",materials);array("assets",assets);array("objects",objects);
        array("lights",lights);array("cameras",cameras,true);out<<"}\n";
        if(!out)throw std::runtime_error("Scene write failed");
    }
};
static void city(Scene& s,const fs::path& assets,const fs::path& legacy_assets) {
    s.material("asphalt",{0.045,0.052,0.065},0.19,0,0,"asphalt",
        pbr_textures(assets/"pbr/asphalt_02",3));
    // Physical repeat sizes from Poly Haven dimensions (millimeters converted to meters).
    s.material("concrete",{0.24,0.26,0.28},0.55,0,0,"stone",
        pbr_textures(assets/"pbr/concrete_wall_001",1.5));
    s.material("granite",{0.065,0.075,0.085},0.36,0,0,"stone");
    s.material("bronze",{0.18,0.095,0.035},0.28,0.75);
    s.material("steel",{0.045,0.055,0.07},0.26,0.85);
    s.material("plaster",{0.27,0.23,0.19},0.65,0,0,"stone",
        pbr_textures(assets/"pbr/plastered_wall_02",2.23));
    s.material("brick",{0.32,0.15,0.08},0.7,0,0,"stone",
        pbr_textures(assets/"pbr/brick_wall_001",3));
    s.material("paving",{0.24,0.26,0.28},0.7,0,0,"stone",
        pbr_textures(assets/"pbr/pavement_02",2.12));
    const std::array<std::string,3> facade_materials={"plaster","brick","concrete"};
    s.material("window_dark",{0.012,0.025,0.04},0.09,0.72);
    s.material("window_warm",{0.75,0.49,0.27},0.72,0,0.65);
    s.material("window_cool",{0.50,0.62,0.72},0.72,0,0.45);
    s.material("window_glass",{0.94,0.97,0.98},0.055,0,0,"smooth","",1.0);
    s.material("interior_wall",{0.34,0.30,0.25},0.85);
    s.material("curtain",{0.36,0.31,0.23},0.95,0,0,"fabric");
    s.material("neon_cyan",{0.04,0.70,1.0},0.25,0,10.0);
    s.material("neon_amber",{1.0,0.22,0.055},0.25,0,9.0);
    s.material("paint",{0.72,0.61,0.39},0.38);
    s.material("suit",{0.025,0.035,0.048},0.34,0.45,0,"fabric");
    s.shape("road","box",{0,-14,-0.12},{7.6,80,0.24},"asphalt",0.01);
    for(int side:{-1,1}) {
        // Continuous collision foundation; separate surface slabs expose joints.
        s.shape("walk_"+std::to_string(side),"box",{side*4.85,-14,0.04},{2.1,80,0.12},"concrete");
        for(int slab=0;slab<40;++slab)
            s.shape("paving_"+std::to_string(side)+"_"+std::to_string(slab),"box",
                {side*4.85,25-slab*2.0,0.12},{2.08,1.985,0.04},"paving",0.006);
        for(int j=0;j<26;++j)
            s.shape("curb_"+std::to_string(side)+"_"+std::to_string(j),"box",
                {side*3.88,14-j*2.5,0.12},{0.18,2.46,0.22},"granite",0.015);
        for(int j=0;j<7;++j) {
            double y=5-j*8.3,h=13+(j*7+(side>0?3:0))%18;
            std::string id="building_"+std::to_string(side)+"_"+std::to_string(j);
            const std::string& facade=facade_materials[j%facade_materials.size()];
            // Recess the rear mass so real window cavities are not buried in a solid box.
            s.shape(id,"box",{side*9.85,y,h/2},{4.9,7.8,h},facade,0.09);
            const std::array<std::array<double,2>,4> piers={{{-3.9,-3.06},{-1.44,-0.81},{0.81,1.44},{3.06,3.9}}};
            for(size_t pier=0;pier<piers.size();++pier) {
                const auto span=piers[pier];
                s.shape(id+"_pier_"+std::to_string(pier),"box",
                    {side*6.56,y+(span[0]+span[1])/2,h/2},{1.4,span[1]-span[0],h},facade,0.012);
            }
            double band_start=0;
            s.shape(id+"_plinth","box",{side*9.05,y,0.7},{6.55,7.95,1.4},"granite",0.035);
            for(int floor=0;floor<int((h-0.35)/3.1);++floor) {
                double z=2.4+floor*3.1;
                const double band_end=z-1.05;
                s.shape(id+"_spandrel_"+std::to_string(floor),"box",
                    {side*6.56,y,(band_start+band_end)/2},{1.4,7.8,band_end-band_start},facade,0.012);
                band_start=z+1.05;
                s.shape(id+"_cornice_"+std::to_string(floor),"box",{side*5.86,y,z-1.25},
                        {0.22,7.92,0.15},"granite",0.015);
                for(int col=0;col<3;++col) {
                    std::string w=id+"_window_"+std::to_string(floor)+"_"+std::to_string(col);
                    V p{side*5.86,y+(col-1)*2.25,z};
                    int lit=(floor*11+col*7+j*3+(side>0?1:0))%9;
                    s.facade_window(w,p,{double(-side),0,0},
                        lit<3?"window_warm":lit==4?"window_cool":"window_dark",true);
                }
            }
            if(h>band_start)s.shape(id+"_parapet","box",{side*6.56,y,(band_start+h)/2},
                {1.4,7.8,h-band_start},facade,0.012);
            s.shape(id+"_door","box",{side*5.80,y,1.35},{0.12,1.55,2.65},"window_dark");
            s.shape(id+"_awning","box",{side*5.12,y,3.0},{1.50,2.9,0.18},"steel",0.04);
            if(j%2==0) {
                s.shape(id+"_sign","box",{side*5.65,y-2.7,5.2},{0.35,0.65,3.2},"steel",0.04);
                s.shape(id+"_neon","box",{side*5.43,y-2.7,5.2},{0.05,0.42,2.8},
                    side<0?"neon_amber":"neon_cyan",0.02);
            }
        }
    }
    // A modeled T junction closes the street's previous empty horizon.
    s.shape("cross_street","box",{0,-58,-0.12},{56,8,0.24},"asphalt",0.01);
    s.shape("walk_cross","box",{0,-63.2,0.06},{56,2.4,0.16},"paving",0.01);
    const std::vector<double> skyline_heights={19,28,23,34,21};
    const std::vector<singular::scene::Point2> corner_footprint={
        {-3.7f,-4.5f},{3.7f,-4.5f},{4.6f,-3.6f},{4.6f,3.6f},
        {3.7f,4.5f},{-3.7f,4.5f},{-4.6f,3.6f},{-4.6f,-3.6f}};
    for(size_t block=0;block<skyline_heights.size();++block) {
        const double x=(double(block)-2)*9.5,h=skyline_heights[block];
        const auto id="backdrop_"+std::to_string(block);
        s.extrusion(id,{x,-69,0},corner_footprint,h,facade_materials[block%facade_materials.size()],0.08);
        s.extrusion(id+"_roof",{x,-69,h},corner_footprint,0.36,"granite",0.025);
        for(int floor=0;floor<int(h/3.1);++floor)for(int col=0;col<3;++col)
            s.facade_window(id+"_window_"+std::to_string(floor)+"_"+std::to_string(col),
                {x+(col-1)*2.5,-64.45,2.4+floor*3.1},{0,1,0},
                (floor+col+block)%4==0?"window_warm":"window_dark");
    }
    for(int j=0;j<14;++j)s.shape("lane_"+std::to_string(j),"box",{0,10-j*4.4,0.008},{0.10,2.1,0.01},"paint",0);
    s.asset("car",assets/"CarConcept.glb",1.43);
    s.asset("lamp",assets/"Lantern.glb",3.8);
    s.instance("specter_hero","car",{-1.55,1.8,0.015},-12);
    s.instance("specter_parked","car",{1.6,-17,0.015},180);
    for(int side:{-1,1})for(int i=0;i<4;++i) {
        double y=4-i*11;std::string id="lamp_"+std::to_string(side)+"_"+std::to_string(i);
        s.instance(id,"lamp",{side*4.15,y,0.14},side<0?90:-90);
        s.light(id+"_source",{side*3.9,y,3.45},{side*2.0,y,0},{1,0.54,0.24},430,0.65);
    }
    const auto character = fs::absolute(legacy_assets / "characters/snow_v2/snow_v02.blend");
    if (!fs::is_regular_file(character)) throw std::runtime_error("Legacy Snow package unavailable");
    s.add(s.objects,"{\"id\":\"nocturne_snow\",\"kind\":\"blend_collection\",\"uri\":"+
        quote(character.string())+",\"collection\":\"CH-snow\",\"location\":[4.7,0.5,0.14],"
        "\"yaw_degrees\":150,\"scale\":[1,1,1],\"preserve_actions\":true,"
        "\"rig_animation\":{\"armature\":\"RIG-Snow\",\"properties\":["
        "{\"bone\":\"Properties\",\"name\":\"ik_left_upperarm\",\"value\":0.0},"
        "{\"bone\":\"Properties\",\"name\":\"ik_right_upperarm\",\"value\":0.0}],\"tracks\":["+
        rotation_track("FK-UpperArm.L",{0,0,65},{0,0,20})+","+
        rotation_track("FK-UpperArm.R",{0,0,-65},{0,0,-65})+","+
        rotation_track("FK-Forearm.L",{-10,0,0},{-35,0,0})+"]}}");
    s.light("moon_fill",{0,-10,24},{0,-8,0},{0.38,0.56,1},2800,18);
    s.light("foreground_softbox",{-5,8,7},{-1,0,1},{0.50,0.65,1},1000,7);
    s.light("warm_shop",{5.5,1,2.4},{0,0,0.6},{1,0.43,0.17},500,3);
    s.camera("street",{1.2,12,1.7},{0,-12,2.5},32,
        "[{\"frame\":1,\"location\":[1.2,12,1.7],\"target\":[0,-12,2.5]},"
        "{\"frame\":48,\"location\":[1.2,8,1.7],\"target\":[0,-12,2.5]}]");
    s.camera("hero",{2.5,7,2.0},{4.7,0.5,1.2},38);
    s.camera("aerial",{3,16,35},{0,-13,0.5},36);
    // A physically simulated crate is explicit, distinct from authored static geometry.
    s.shape("simulation_crate","box",{4.7,-8,2.5},{0.65,0.65,0.65},"bronze",0.035,
            ",\"physics\":{\"type\":\"ACTIVE\",\"mass_kg\":12,\"friction\":0.65}");
}
static void cape_studio(Scene& s,const fs::path& assets) {
    s.name="Nocturne_Cape_Lookdev_Unrigged_Anatomy";
    const auto body=assets/"human_base_meshes/human_base_meshes_bundle.blend";
    if(!fs::is_regular_file(body))throw std::runtime_error("Recovered anatomy bundle missing");
    s.material("suit",{0.028,0.035,0.045},0.6,0,0,"fabric");
    s.material("cape_fabric",{0.009,0.012,0.02},0.76,0,0,"fabric");
    s.material("stage",{0.065,0.075,0.09},0.45);
    s.shape("stage","box",{0,0,-0.05},{8,8,0.1},"stage",0.01,",\"collision\":true");
    s.add(s.objects,"{\"id\":\"hero_body\",\"kind\":\"blend_collection\",\"uri\":"+quote(body.string())+
        ",\"collection\":\"Body Male - Realistic\",\"location\":[0,0,0.006],\"yaw_degrees\":0,"
        "\"scale\":[1,1,1],\"material_override\":\"suit\",\"collision\":true}");
    singular::scene::PolygonMesh mesh;
    constexpr unsigned columns=33,rows=49;
    for(unsigned row=0;row<rows;++row) {
        const double t=double(row)/(rows-1);
        for(unsigned col=0;col<columns;++col) {
            const double u=2.0*col/(columns-1)-1;
            // r020 fitted shoulders measured z=1.359..1.388, y=.085..092 m.
            // Author the whole rest surface from that seam: fitting just row 0
            // of the old z=1.43 plane left row 1 above it, creating a stiff shelf.
            // The arch is an authored tailoring approximation, not scanned skin.
            const double seam_height=1.36+0.025*(1-u*u);
            const double back_clearance=0.045*(1-std::exp(-12*t))+0.05*t;
            mesh.vertices.push_back({u*(0.155+0.445*t),0.085+back_clearance,
                                     seam_height-1.1*t});
        }
    }
    for(unsigned row=0;row+1<rows;++row)for(unsigned col=0;col+1<columns;++col) {
        const unsigned a=row*columns+col;
        mesh.faces.push_back({a,a+1,a+1+columns,a+columns});
    }
    std::string pins="[";
    for(unsigned i=0;i<columns;++i)pins+=(i?",":"")+std::to_string(i);
    pins+="]";
    s.indexed("hero_cape",{0,0,0},mesh,"cape_fabric",0,
        ",\"topology\":\"open_surface\",\"smooth\":true,\"cloth\":{\"model\":\"blender.cloth/1\","
        "\"mass_kg_per_vertex\":"+std::to_string(1.02/(columns*rows))+",\"quality_steps\":8,\"collision_quality_steps\":4,\"tension_stiffness\":15,"
        "\"compression_stiffness\":15,\"shear_stiffness\":5,\"bending_stiffness\":0.5,"
        "\"tension_damping\":5,\"compression_damping\":5,\"shear_damping\":5,"
        "\"bending_damping\":0.5,\"air_damping\":0.01,\"pins\":"+pins+
        ",\"pin_surface\":{\"object\":\"hero_body\",\"max_distance_m\":0.2,\"offset_m\":0.008},"
        "\"collision_objects\":[\"hero_body\",\"stage\"],\"collision_distance_m\":0.003,"
        "\"self_collision_distance_m\":0.003,\"self_collision\":true}");
    s.light("key",{-3,1,4},{0,0,1},{0.62,0.75,1},700,3);
    s.light("rim",{2,-2,3},{0,0,1},{1,0.58,0.28},900,2);
    s.light("fill",{1,4,2},{0,0,1},{0.4,0.6,1},350,3);
    s.camera("street",{2.4,3.2,1.8},{0,0,0.85},40);
    s.camera("front",{2.2,-3,1.7},{0,0,0.85},40);
    s.camera("side",{3.4,0.5,1.4},{0,0,0.85},38);
}
int main(int argc,char**argv) {
    try {
        if(argc>=2&&std::string(argv[1])=="edit-schema") {
            if(argc!=2&&(argc!=3||std::string(argv[2])!="2"))throw std::runtime_error("Usage: singular-scene edit-schema [2]");
            std::cout<<singular::scene::editing::schema_json(argc==3?2:1);return 0;
        }
        if(argc>=2&&std::string(argv[1])=="edit") {
            if(argc!=5)throw std::runtime_error("Usage: singular-scene edit BASE.json PROPOSAL.json NEW_SCENE.json");
            singular::scene::editing::run(argv[2],argv[3],argv[4]);return 0;
        }
        if(argc<3 || argc>5 || (argc==5 && std::string(argv[4])!="4k" && std::string(argv[4])!="cape")) {
            std::cerr<<"Usage: singular-scene ASSET_DIRECTORY OUTPUT.json [LEGACY_ASSET_DIRECTORY [4k|cape]]\n";return 2;
        }
        const auto assets = fs::absolute(argv[1]);
        const auto legacy = argc >= 4 ? fs::path(argv[3]) : assets.parent_path().parent_path()
            .parent_path().parent_path().parent_path() / "Simulator/plugins/assets";
        const bool cape=argc==5 && std::string(argv[4])=="cape";
        Scene scene;
        if(cape)cape_studio(scene,assets);else city(scene,assets,legacy);
        scene.write(argv[2],argc==5);
        std::cout<<"SCENE_AUTHORED objects="<<scene.objects.size()<<" assets="<<scene.assets.size()
                 <<" cameras="<<scene.cameras.size()<<" output="<<argv[2]<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}

